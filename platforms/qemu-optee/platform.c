/* SPDX-License-Identifier: Apache-2.0 */

#include "linux/linux_platform.h"

int tear_platform_run_workload(const struct tear_workload_run *run,
                               int *raw_status)
{
    return tear_linux_platform_run_workload(run, raw_status);
}

int tear_platform_start_process(const char *path,
                                char *const argv[],
                                tear_platform_process_t *process)
{
    return tear_linux_platform_start_process(path, argv, process);
}

int tear_platform_run_process(const char *path,
                              char *const argv[],
                              int *exit_code)
{
    return tear_linux_platform_run_process(path, argv, exit_code);
}

void tear_platform_stop_process(tear_platform_process_t process)
{
    tear_linux_platform_stop_process(process);
}

void tear_platform_sleep_ms(unsigned int milliseconds)
{
    tear_linux_platform_sleep_ms(milliseconds);
}
