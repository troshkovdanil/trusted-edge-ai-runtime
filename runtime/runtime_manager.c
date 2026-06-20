// SPDX-License-Identifier: Apache-2.0

#include "runtime_manager.h"

#include "model_manifest.h"
#include "runtime_paths.h"
#include "observability.h"
#include "profile.h"
#include "trust_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEAR_COMPONENT "runtime_manager"
#define TEAR_METRICS_PATH_MAX 256
#define TEAR_EVENT_PATH_MAX 256
#define TEAR_RUN_ID_MAX 64

#ifdef TEAR_HOST_BUILD
#define DEFAULT_EVENT_PATH "build/host/tear-runtime-manager-events.log"
#else
#define DEFAULT_EVENT_PATH "/tmp/tear-runtime-manager-events.log"
#endif

struct tear_run_config {
    const char *name;
    const char *workload;
    const char *manifest;
    const char *profile;
    const char *args;
    const char *event_log;
    int enable_optimizer;
};

struct opt_proposal {
    char action[128];
    char reason[128];
    int available;
};

static void runtime_event(const char *workload,
                          const char *artifact_id,
                          const char *event)
{
    tear_event_ex(TEAR_COMPONENT, workload, artifact_id, event);
}

static void runtime_event_kv(const char *workload,
                             const char *artifact_id,
                             const char *event,
                             const char *key,
                             long value)
{
    tear_event_ex_kv(TEAR_COMPONENT,
                     workload,
                     artifact_id,
                     event,
                     key,
                     value);
}

static struct tear_run_config parse_args(int argc, char **argv)
{
    struct tear_run_config cfg = {
        .name = "runtime-workload",
        .workload = NULL,
        .manifest = NULL,
        .profile = NULL,
        .args = "",
        .event_log = DEFAULT_EVENT_PATH,
        .enable_optimizer = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            cfg.name = argv[++i];
        } else if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            cfg.profile = argv[++i];
        } else if (strcmp(argv[i], "--args") == 0 && i + 1 < argc) {
            cfg.args = argv[++i];
        } else if (strcmp(argv[i], "--event-log") == 0 && i + 1 < argc) {
            cfg.event_log = argv[++i];
        } else if (strcmp(argv[i], "--enable-optimizer") == 0) {
            cfg.enable_optimizer = 1;
        }
    }

    return cfg;
}

static int validate_config(const struct tear_run_config *cfg)
{
    if (!cfg->event_log || cfg->event_log[0] == '\0') {
        fprintf(stderr, "TEAR: missing --event-log <path>\n");
        return -1;
    }

    if (!cfg->workload || !cfg->manifest || !cfg->profile) {
        fprintf(stderr,
                "usage: tear-runtime-manager "
                "[--name <name>] "
                "--workload <path> "
                "--manifest <path> "
                "--profile <path> "
                "[--args <args>] "
                "[--event-log <path>] "
                "[--enable-optimizer]\n");
        return -1;
    }

    return 0;
}

static void build_run_id(char *run_id, size_t run_id_size)
{
    snprintf(run_id,
             run_id_size,
             "run-%ld-%ld",
             (long)getpid(),
             (long)time(NULL));
}

static int build_metrics_path(const struct tear_profile *profile,
                              const char *run_id,
                              char *path,
                              size_t path_size)
{
    int n = snprintf(path,
                     path_size,
                     "%s-%s",
                     profile->metrics_file_template,
                     run_id);

    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int build_workload_event_path(const char *event_log,
                                     const char *run_id,
                                     char *path,
                                     size_t path_size)
{
    int n = snprintf(path,
                     path_size,
                     "%s-%s",
                     event_log,
                     run_id);

    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int ask_optd(const char *metrics_path,
                    struct opt_proposal *proposal)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path,
            tear_optd_socket_path(),
            sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    dprintf(fd, "PROPOSE %s\n", metrics_path);

    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    close(fd);

    if (n <= 0)
        return -1;

    buf[n] = '\0';

    memset(proposal, 0, sizeof(*proposal));

    if (sscanf(buf,
               "PROPOSAL %127s %127s",
               proposal->action,
               proposal->reason) == 2) {
        proposal->available = 1;
        return 0;
    }

    if (strncmp(buf, "NO_PROPOSAL", strlen("NO_PROPOSAL")) == 0) {
        proposal->available = 0;
        strncpy(proposal->action,
                "none",
                sizeof(proposal->action) - 1);
        strncpy(proposal->reason,
                "metrics_unavailable",
                sizeof(proposal->reason) - 1);
        return 0;
    }

    return -1;
}

static void approve_or_reject_proposal(const struct opt_proposal *proposal,
                                       const char **decision,
                                       const char **reason)
{
    if (!proposal->available) {
        *decision = "rejected";
        *reason = "metrics_missing";
        return;
    }

    if (strcmp(proposal->action, "request_high_accuracy_profile") == 0) {
        *decision = "rejected";
        *reason = "profile_unavailable";
        return;
    }

    if (strcmp(proposal->action, "keep_current_profile") == 0) {
        *decision = "approved";
        *reason = "policy_allows";
        return;
    }

    if (strcmp(proposal->action, "reject_input") == 0) {
        *decision = "approved";
        *reason = "input_rejected";
        return;
    }

    *decision = "rejected";
    *reason = "unknown_proposal";
}

static void record_and_report_optimizer_decision(const char *workload,
                                                 const char *artifact_id,
                                                 const char *run_id,
                                                 const char *proposal,
                                                 const char *decision,
                                                 const char *reason)
{
    char reported_decision[512];

    if (tear_trust_record_decision(run_id,
                                   artifact_id,
                                   proposal,
                                   decision,
                                   reason,
                                   0) < 0) {
        runtime_event(workload,
                      artifact_id,
                      "optimization_decision_record_failed");
        return;
    }

    runtime_event(workload,
                  artifact_id,
                  "optimization_decision_recorded_by_runtime_manager");

    if (tear_trust_report_decision(reported_decision,
                                   sizeof(reported_decision)) < 0) {
        runtime_event(workload,
                      artifact_id,
                      "optimization_decision_report_failed");
        return;
    }

    printf("TEAR: reported_decision %s\n", reported_decision);

    runtime_event(workload,
                  artifact_id,
                  "optimization_decision_reported_by_runtime_manager");
}

static void record_optimizer_decision(const char *workload,
                                      const char *artifact_id,
                                      const char *run_id,
                                      const char *metrics_path)
{
    struct opt_proposal proposal;
    const char *decision;
    const char *decision_reason;

    if (ask_optd(metrics_path, &proposal) < 0) {
        runtime_event(workload, artifact_id, "optimizer_proposal_failed");

        record_and_report_optimizer_decision(workload,
                                             artifact_id,
                                             run_id,
                                             "none",
                                             "rejected",
                                             "optimizer_unavailable");
        return;
    }

    if (!proposal.available)
        runtime_event(workload, artifact_id, "optimizer_no_proposal");
    else
        runtime_event(workload, artifact_id, "optimizer_proposal_received");

    approve_or_reject_proposal(&proposal, &decision, &decision_reason);

    runtime_event(workload, artifact_id, proposal.action);
    runtime_event(workload, artifact_id, decision);
    runtime_event(workload, artifact_id, decision_reason);

    record_and_report_optimizer_decision(workload,
                                         artifact_id,
                                         run_id,
                                         proposal.action,
                                         decision,
                                         decision_reason);
}

static void run_workload_process(const struct tear_run_config *cfg,
                                 const char *run_id,
                                 const char *event_log)
{
    char command[512];

    if (cfg->args && cfg->args[0] != '\0') {
        snprintf(command,
                 sizeof(command),
                 "%s --profile %s --run-id %s --event-log %s %s",
                 cfg->workload,
                 cfg->profile,
                 run_id,
                 event_log,
                 cfg->args);

        execl("/bin/sh", "sh", "-c", command, NULL);
    } else {
        execl(cfg->workload,
              cfg->workload,
              "--profile",
              cfg->profile,
              "--run-id",
              run_id,
              "--event-log",
              event_log,
              NULL);
    }

    perror("execl");
    _exit(127);
}

int tear_runtime_manager_main(int argc, char **argv)
{
    struct tear_run_config cfg = parse_args(argc, argv);
    struct tear_model_manifest manifest;
    struct tear_profile profile;
    int use_optimizer = 0;
    char metrics_path[TEAR_METRICS_PATH_MAX];
    char workload_event_path[TEAR_EVENT_PATH_MAX];
    char run_id[TEAR_RUN_ID_MAX];
    int status = 0;
    int ret = 1;

    if (validate_config(&cfg) < 0)
        return 1;

    if (tear_event_init(cfg.event_log) < 0) {
        fprintf(stderr, "TEAR: failed to initialize runtime manager events\n");
        return 1;
    }

    build_run_id(run_id, sizeof(run_id));

    runtime_event(cfg.name, NULL, "runtime_manager_start");
    runtime_event(cfg.name, NULL, "manifest_load_start");

    if (tear_manifest_load(cfg.manifest, &manifest) < 0) {
        fprintf(stderr, "TEAR: failed to load manifest\n");
        runtime_event(cfg.name, NULL, "manifest_load_failed");
        goto out;
    }

    runtime_event(cfg.name, manifest.artifact_id, "manifest_loaded");

    tear_manifest_print(&manifest);

    if (tear_profile_load(cfg.profile, &profile) < 0) {
        fprintf(stderr, "TEAR: failed to load profile\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "profile_load_failed");
        goto out;
    }

    runtime_event(cfg.name, manifest.artifact_id, "profile_loaded");

    if (strcmp(profile.artifact_id, manifest.artifact_id) != 0 ||
        strcmp(profile.backend, manifest.backend) != 0) {
        fprintf(stderr, "TEAR: profile does not match manifest\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "profile_manifest_mismatch");
        goto out;
    }

    runtime_event(cfg.name, manifest.artifact_id, "profile_manifest_verified");
    runtime_event(cfg.name, manifest.artifact_id, run_id);

    if (build_metrics_path(&profile,
                           run_id,
                           metrics_path,
                           sizeof(metrics_path)) < 0) {
        fprintf(stderr, "TEAR: metrics path too long\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "metrics_path_too_long");
        goto out;
    }

    if (build_workload_event_path(cfg.event_log,
                                  run_id,
                                  workload_event_path,
                                  sizeof(workload_event_path)) < 0) {
        fprintf(stderr, "TEAR: workload event path too long\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "workload_event_path_too_long");
        goto out;
    }

    if (tear_trust_verify(&manifest) < 0) {
        fprintf(stderr, "TEAR: manifest verification failed\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "manifest_verify_failed");
        goto out;
    }

    runtime_event(cfg.name, manifest.artifact_id, "manifest_verified");

    if (manifest.optimization_capable && !cfg.enable_optimizer) {
        fprintf(stderr,
                "TEAR: workload is optimization-capable "
                "but optimizer is disabled\n");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "optimizer_required_but_disabled");
        goto out;
    }

    use_optimizer = cfg.enable_optimizer && manifest.optimization_capable;

    if (cfg.enable_optimizer && !manifest.optimization_capable)
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "optimizer_skipped_manifest_not_capable");

    if (use_optimizer)
        unlink(metrics_path);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "runtime_manager_fork_failed");
        goto out;
    }

    if (pid == 0)
        run_workload_process(&cfg, run_id, workload_event_path);

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        runtime_event(cfg.name,
                      manifest.artifact_id,
                      "runtime_manager_wait_failed");
        goto out;
    }

    if (WIFEXITED(status)) {
        runtime_event_kv(cfg.name,
                         manifest.artifact_id,
                         "runtime_workload_exit",
                         "status",
                         WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        runtime_event_kv(cfg.name,
                         manifest.artifact_id,
                         "runtime_workload_signal",
                         "signal",
                         WTERMSIG(status));
    }

    if (use_optimizer)
        record_optimizer_decision(cfg.name,
                                  manifest.artifact_id,
                                  run_id,
                                  metrics_path);

    runtime_event(cfg.name,
                  manifest.artifact_id,
                  "runtime_manager_shutdown");

    ret = WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;

out:
    tear_event_shutdown();
    return ret;
}
