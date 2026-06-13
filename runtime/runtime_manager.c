// SPDX-License-Identifier: Apache-2.0

#include "runtime_manager.h"

#include "model_manifest.h"
#include "runtime_paths.h"
#include "observability.h"
#include "trust_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEAR_MNIST_METRICS "/tmp/tear-mnist-metrics"

struct runtime_config {
    const char *name;
    const char *workload;
    const char *manifest;
    const char *args;
    int enable_optimizer;
};

struct opt_proposal {
    char action[128];
    char reason[128];
    int available;
};

static struct runtime_config parse_args(int argc, char **argv)
{
    struct runtime_config cfg = {
        .name = "runtime-workload",
        .workload = NULL,
        .manifest = NULL,
        .args = "",
        .enable_optimizer = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            cfg.name = argv[++i];
        } else if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--args") == 0 && i + 1 < argc) {
            cfg.args = argv[++i];
        } else if (strcmp(argv[i], "--enable-optimizer") == 0) {
            cfg.enable_optimizer = 1;
        }
    }

    return cfg;
}

static int validate_config(const struct runtime_config *cfg)
{
    if (!cfg->workload || !cfg->manifest) {
        fprintf(stderr,
                "usage: tear-runtime-manager "
                "[--name <name>] "
                "--workload <path> "
                "--manifest <path> "
                "[--args <args>] "
                "[--enable-optimizer]\n");
        return -1;
    }

    return 0;
}

static int ask_optd(struct opt_proposal *proposal)
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

    dprintf(fd, "PROPOSE %s\n", TEAR_MNIST_METRICS);

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

static void record_and_report_optimizer_decision(const char *model_id,
                                                 const char *proposal,
                                                 const char *decision,
                                                 const char *reason)
{
    char reported_decision[512];

    if (tear_trust_record_decision(model_id,
                                   proposal,
                                   decision,
                                   reason,
                                   0) < 0) {
        tear_event("optimization_decision_record_failed");
        return;
    }

    tear_event("optimization_decision_recorded_by_runtime_manager");

    if (tear_trust_report_decision(reported_decision,
                                   sizeof(reported_decision)) < 0) {
        tear_event("optimization_decision_report_failed");
        return;
    }

    printf("TEAR: reported_decision %s\n", reported_decision);
    tear_event("optimization_decision_reported_by_runtime_manager");
}

static void record_optimizer_decision(const char *model_id)
{
    struct opt_proposal proposal;
    const char *decision;
    const char *decision_reason;

    if (ask_optd(&proposal) < 0) {
        tear_event("optimizer_proposal_failed");

        record_and_report_optimizer_decision(model_id,
                                             "none",
                                             "rejected",
                                             "optimizer_unavailable");
        return;
    }

    if (!proposal.available)
        tear_event("optimizer_no_proposal");
    else
        tear_event("optimizer_proposal_received");

    approve_or_reject_proposal(&proposal, &decision, &decision_reason);

    tear_event(proposal.action);
    tear_event(decision);
    tear_event(decision_reason);

    record_and_report_optimizer_decision(model_id,
                                         proposal.action,
                                         decision,
                                         decision_reason);
}

static void run_workload_process(const struct runtime_config *cfg)
{
    char command[512];

    if (cfg->args && cfg->args[0] != '\0') {
        snprintf(command,
                 sizeof(command),
                 "%s %s",
                 cfg->workload,
                 cfg->args);

        execl("/bin/sh", "sh", "-c", command, NULL);
    } else {
        execl(cfg->workload, cfg->workload, NULL);
    }

    perror("execl");
    _exit(127);
}

int tear_runtime_manager_main(int argc, char **argv)
{
    struct runtime_config cfg = parse_args(argc, argv);
    struct tear_model_manifest manifest;
    int use_optimizer = 0;

    tear_event("runtime_manager_start");

    if (cfg.name)
        tear_event(cfg.name);

    if (validate_config(&cfg) < 0) {
        tear_event("runtime_manager_invalid_config");
        return 1;
    }

    if (tear_manifest_load(cfg.manifest, &manifest) < 0) {
        fprintf(stderr, "TEAR: failed to load manifest\n");
        tear_event("manifest_load_failed");
        return 1;
    }

    tear_event("manifest_loaded");

    tear_manifest_print(&manifest);

    if (tear_trust_verify(&manifest) < 0) {
        fprintf(stderr, "TEAR: manifest verification failed\n");
        tear_event("manifest_verify_failed");
        return 1;
    }

    tear_event("manifest_verified");

    if (manifest.optimization_capable && !cfg.enable_optimizer) {
        fprintf(stderr,
                "TEAR: workload is optimization-capable "
                "but optimizer is disabled\n");
        tear_event("optimizer_required_but_disabled");
        return 1;
    }

    use_optimizer = cfg.enable_optimizer && manifest.optimization_capable;

    if (cfg.enable_optimizer && !manifest.optimization_capable)
        tear_event("optimizer_skipped_manifest_not_capable");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event("runtime_manager_fork_failed");
        return 1;
    }

    if (pid == 0) {
        if (use_optimizer)
            setenv("TEAR_TELEMETRY_FILE", TEAR_MNIST_METRICS, 1);

        run_workload_process(&cfg);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        tear_event("runtime_manager_wait_failed");
        return 1;
    }

    if (WIFEXITED(status)) {
        tear_event_kv("runtime_workload_exit",
                      "status",
                      WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        tear_event_kv("runtime_workload_signal",
                      "signal",
                      WTERMSIG(status));
    }

    if (use_optimizer)
        record_optimizer_decision(manifest.model_id);

    tear_event("runtime_manager_shutdown");

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}
