// SPDX-License-Identifier: Apache-2.0

#include "runtime_manager.h"

#include "model_manifest.h"
#include "observability.h"
#include "platform_adapter.h"
#include "profile.h"
#include "runtime_paths.h"
#include "trust_client.h"
#include "workload_adapter.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define TEAR_COMPONENT "runtime_manager"

#ifdef TEAR_HOST_BUILD
#define TEAR_PLATFORM_DIR "build/platforms/host-mock"
#else
#define TEAR_PLATFORM_DIR "/tmp"
#endif
#define DEFAULT_EVENT_PATH TEAR_PLATFORM_DIR "/tear-runtime-manager-events.log"

struct tear_run_config {
    const char *workload;
    const char *manifest;
    const char *profile;
    const char *args;
    const char *event_log;
};

struct opt_proposal {
    char action[128];
    char reason[128];
    int available;
};

static void runtime_event(const char *event)
{
    tear_event(TEAR_COMPONENT, event);
}

static void runtime_profile_event(const struct tear_profile *profile,
                                  const char *event)
{
    tear_event_profile(TEAR_COMPONENT, profile, event);
}

static void runtime_manifest_event(const struct tear_model_manifest *manifest,
                                   const char *event)
{
    tear_event_manifest(TEAR_COMPONENT, manifest, event);
}

static void optd_send(int fd, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vdprintf(fd, fmt, ap);
    va_end(ap);
}

static struct tear_run_config parse_args(int argc, char **argv)
{
    struct tear_run_config cfg = {
        .workload = NULL,
        .manifest = NULL,
        .profile = NULL,
        .args = "",
        .event_log = DEFAULT_EVENT_PATH,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            cfg.profile = argv[++i];
        } else if (strcmp(argv[i], "--args") == 0 && i + 1 < argc) {
            cfg.args = argv[++i];
        } else if (strcmp(argv[i], "--event-log") == 0 && i + 1 < argc) {
            cfg.event_log = argv[++i];
        } else {
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "unknown argument: %s",
                     argv[i]);
            cfg.workload = NULL;
            break;
        }
    }

    return cfg;
}

static int validate_config(const struct tear_run_config *cfg)
{
    if (!cfg->event_log || cfg->event_log[0] == '\0') {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "missing --event-log <path>");
        return -1;
    }

    if (!cfg->workload || !cfg->manifest || !cfg->profile) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "usage: tear-runtime-manager --workload <path> "
                 "--manifest <path> --profile <path> [--args <args>] "
                 "[--event-log <path>]");
        return -1;
    }

    return 0;
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

    optd_send(fd, "PROPOSE %s\n", metrics_path);

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

static void record_and_report_optimizer_decision(
    const struct tear_profile *profile,
    const char *run_id,
    const char *proposal,
    const char *decision,
    const char *reason)
{
    char reported_decision[512];

    if (tear_trust_record_decision(run_id,
                                   profile->artifact_id,
                                   proposal,
                                   decision,
                                   reason,
                                   0) < 0) {
        runtime_profile_event(profile,
                              "optimization_decision_record_failed");
        return;
    }

    runtime_profile_event(profile,
                          "optimization_decision_recorded_by_runtime_manager");

    if (tear_trust_report_decision(reported_decision,
                                   sizeof(reported_decision)) < 0) {
        runtime_profile_event(profile,
                              "optimization_decision_report_failed");
        return;
    }

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "reported_decision %s",
             reported_decision);

    runtime_profile_event(profile,
                          "optimization_decision_reported_by_runtime_manager");
}

static void record_optimizer_decision(const struct tear_profile *profile,
                                      const char *run_id,
                                      const char *metrics_path)
{
    struct opt_proposal proposal;
    const char *decision;
    const char *decision_reason;

    if (ask_optd(metrics_path, &proposal) < 0) {
        runtime_profile_event(profile, "optimizer_proposal_failed");

        record_and_report_optimizer_decision(profile,
                                             run_id,
                                             "none",
                                             "rejected",
                                             "optimizer_unavailable");
        return;
    }

    if (!proposal.available)
        runtime_profile_event(profile, "optimizer_no_proposal");
    else
        runtime_profile_event(profile, "optimizer_proposal_received");

    approve_or_reject_proposal(&proposal, &decision, &decision_reason);

    runtime_profile_event(profile, proposal.action);
    runtime_profile_event(profile, decision);
    runtime_profile_event(profile, decision_reason);

    record_and_report_optimizer_decision(profile,
                                         run_id,
                                         proposal.action,
                                         decision,
                                         decision_reason);
}

int tear_runtime_manager_main(int argc, char **argv)
{
    struct tear_run_config cfg = parse_args(argc, argv);
    struct tear_model_manifest manifest;
    struct tear_profile profile;
    struct tear_platform_context platform;
    struct tear_workload_run workload_run;
    int use_optimizer = 0;
    int ret = 1;

    if (validate_config(&cfg) < 0)
        return 1;

    if (tear_event_init(cfg.event_log) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize runtime manager events");
        return 1;
    }

    runtime_event("runtime_manager_start");
    runtime_event("manifest_load_start");

    if (tear_manifest_load(cfg.manifest, &manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to load manifest");
        runtime_event("manifest_load_failed");
        goto out;
    }

    runtime_manifest_event(&manifest, "manifest_loaded");

    tear_manifest_print(&manifest);

    if (tear_profile_load(cfg.profile, &profile) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to load profile");
        runtime_manifest_event(&manifest, "profile_load_failed");
        goto out;
    }

    runtime_profile_event(&profile, "profile_loaded");

    tear_profile_print(&profile);

    if (tear_platform_detect(&platform) < 0) {
        runtime_profile_event(&profile, "platform_detect_failed");
        goto out;
    }

    if (tear_platform_check_profile(&platform, &profile) < 0) {
        runtime_profile_event(&profile, "platform_profile_check_failed");
        goto out;
    }

    if (tear_workload_prepare(&manifest,
                              &profile,
                              cfg.profile,
                              cfg.workload,
                              cfg.args,
                              cfg.event_log,
                              &workload_run) < 0) {
        runtime_profile_event(&profile, "workload_contract_prepare_failed");
        goto out;
    }

    runtime_profile_event(&profile, "profile_manifest_verified");
    runtime_profile_event(&profile, workload_run.run_id);

    if (tear_trust_verify(&manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "manifest verification failed");
        runtime_manifest_event(&manifest, "manifest_verify_failed");
        goto out;
    }

    runtime_manifest_event(&manifest, "manifest_verified");

    use_optimizer = manifest.optimization_capable;

    if (tear_workload_run_checked(&workload_run, use_optimizer) < 0) {
        runtime_profile_event(&profile, "workload_contract_run_failed");
        goto out;
    }

    if (use_optimizer)
        record_optimizer_decision(&profile,
                                  workload_run.run_id,
                                  workload_run.metrics_path);

    runtime_profile_event(&profile, "runtime_manager_shutdown");

    ret = 0;

out:
    tear_event_shutdown();
    return ret;
}
