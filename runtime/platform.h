/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_PLATFORM_H
#define TEAR_PLATFORM_H

#include "workload_adapter.h"

typedef long tear_platform_process_t;

#define TEAR_PLATFORM_INVALID_PROCESS ((tear_platform_process_t)-1)

int tear_platform_run_workload(const struct tear_workload_run *run,
                               int *raw_status);

int tear_platform_start_process(const char *path,
                                char *const argv[],
                                tear_platform_process_t *process);

int tear_platform_run_process(const char *path,
                              char *const argv[],
                              int *exit_code);

void tear_platform_stop_process(tear_platform_process_t process);

void tear_platform_sleep_ms(unsigned int milliseconds);

#endif
