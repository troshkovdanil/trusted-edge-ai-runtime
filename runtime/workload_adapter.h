/* SPDX-License-Identifier: Apache-2.0 */
#ifndef TEAR_WORKLOAD_ADAPTER_H
#define TEAR_WORKLOAD_ADAPTER_H

#include <stddef.h>

#include "model_manifest.h"
#include "profile.h"

#define TEAR_WORKLOAD_ADAPTER_PATH_MAX 256
#define TEAR_WORKLOAD_ADAPTER_RUN_ID_MAX 64

struct tear_workload_run {
    const char *workload;
    const char *profile_path;
    const char *extra_args;
    const char *runtime_event_log;

    char run_id[TEAR_WORKLOAD_ADAPTER_RUN_ID_MAX];
    char metrics_path[TEAR_WORKLOAD_ADAPTER_PATH_MAX];
    char workload_event_log[TEAR_WORKLOAD_ADAPTER_PATH_MAX];
};

int tear_workload_prepare(const struct tear_model_manifest *manifest,
                          const struct tear_profile *profile,
                          const char *profile_path,
                          const char *workload,
                          const char *extra_args,
                          const char *runtime_event_log,
                          struct tear_workload_run *run);

int tear_workload_run_checked(const struct tear_workload_run *run,
                              int require_metrics_file);

#endif
