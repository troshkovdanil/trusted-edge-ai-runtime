/* SPDX-License-Identifier: Apache-2.0 */

#include "linux/platform.h"

int tear_platform_run_workload(const struct tear_workload_run *run,
                               int *raw_status)
{
    return tear_linux_platform_run_workload(run, raw_status);
}
