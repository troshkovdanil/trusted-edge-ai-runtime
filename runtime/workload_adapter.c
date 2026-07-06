/* SPDX-License-Identifier: Apache-2.0 */

#include "workload_adapter.h"

#include "observability.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TEAR_COMPONENT "workload_adapter"

static void build_run_id(char *run_id, size_t run_id_size)
{
    snprintf(run_id, run_id_size, "run-%ld-%ld",
             (long)getpid(), (long)time(NULL));
}

static int build_metrics_path(const struct tear_profile *profile,
                              const char *run_id,
                              char *path,
                              size_t path_size)
{
    int n;

    if (!profile || !profile->metrics_file_template[0] || !run_id)
        return -1;

    n = snprintf(path, path_size, "%s-%s",
                 profile->metrics_file_template, run_id);

    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int build_workload_event_log(const char *runtime_event_log,
                                    const char *run_id,
                                    char *path,
                                    size_t path_size)
{
    int n;

    if (!runtime_event_log || !runtime_event_log[0] || !run_id)
        return -1;

    n = snprintf(path, path_size, "%s-%s", runtime_event_log, run_id);

    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int profile_matches_manifest(const struct tear_model_manifest *manifest,
                                    const struct tear_profile *profile)
{
    if (strcmp(profile->artifact_id, manifest->artifact_id) != 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "profile artifact_id %s does not match manifest %s",
                 profile->artifact_id, manifest->artifact_id);
        return 0;
    }

    if (strcmp(profile->backend, manifest->backend) != 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "profile backend %s does not match manifest %s",
                 profile->backend, manifest->backend);
        return 0;
    }

    return 1;
}

int tear_workload_prepare(const struct tear_model_manifest *manifest,
                          const struct tear_profile *profile,
                          const char *profile_path,
                          const char *workload,
                          const char *extra_args,
                          const char *runtime_event_log,
                          struct tear_workload_run *run)
{
    if (!manifest || !profile || !profile_path || !workload ||
        !runtime_event_log || !run) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "invalid workload adapter input");
        return -1;
    }

    if (workload[0] == '\0' || profile_path[0] == '\0') {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "missing workload or profile path");
        return -1;
    }

    if (!profile_matches_manifest(manifest, profile)) {
        tear_event_profile(TEAR_COMPONENT, profile,
                           "workload_contract_profile_manifest_mismatch");
        return -1;
    }

    memset(run, 0, sizeof(*run));

    run->workload = workload;
    run->profile_path = profile_path;
    run->extra_args = extra_args ? extra_args : "";
    run->runtime_event_log = runtime_event_log;

    build_run_id(run->run_id, sizeof(run->run_id));

    if (build_metrics_path(profile, run->run_id,
                           run->metrics_path,
                           sizeof(run->metrics_path)) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to prepare metrics path");
        tear_event_profile(TEAR_COMPONENT, profile,
                           "workload_contract_metrics_path_failed");
        return -1;
    }

    if (build_workload_event_log(runtime_event_log, run->run_id,
                                 run->workload_event_log,
                                 sizeof(run->workload_event_log)) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to prepare workload event log path");
        tear_event_profile(TEAR_COMPONENT, profile,
                           "workload_contract_event_log_path_failed");
        return -1;
    }

    tear_event_profile(TEAR_COMPONENT, profile,
                       "workload_contract_prepared");

    return 0;
}

int tear_workload_run_checked(const struct tear_workload_run *run,
                              int require_metrics_file)
{
    int status = 0;

    if (!run || !run->workload || !run->profile_path || !run->run_id[0]) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "invalid prepared workload run");
        return -1;
    }

    if (require_metrics_file)
        unlink(run->metrics_path);

    tear_event(TEAR_COMPONENT, "workload_contract_launch");

    if (tear_platform_run_workload(run, &status) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "platform failed to run workload");
        tear_event(TEAR_COMPONENT, "workload_contract_platform_run_failed");
        return -1;
    }

    tear_event_kv("runtime_manager",
                  "runtime_workload_exit",
                  "status",
                  status);

    if (!tear_platform_process_exited(status)) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "workload did not exit normally");
        tear_event(TEAR_COMPONENT, "workload_contract_abnormal_exit");
        return -1;
    }

    if (tear_platform_process_exit_code(status) != 0) {
        int exit_code = tear_platform_process_exit_code(status);

        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "workload exited with status %d", exit_code);
        tear_event_kv(TEAR_COMPONENT,
                      "workload_contract_exit_failed",
                      "status",
                      exit_code);
        return -1;
    }

    tear_event(TEAR_COMPONENT, "workload_contract_exit_ok");

    if (require_metrics_file &&
        !tear_platform_path_exists(run->metrics_path)) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "required metrics file was not produced: %s",
                 run->metrics_path);
        tear_event(TEAR_COMPONENT, "workload_contract_metrics_missing");
        return -1;
    }

    if (require_metrics_file)
        tear_event(TEAR_COMPONENT, "workload_contract_metrics_ok");

    if (tear_platform_path_exists(run->workload_event_log))
        tear_event(TEAR_COMPONENT, "workload_contract_event_log_seen");

    return 0;
}
