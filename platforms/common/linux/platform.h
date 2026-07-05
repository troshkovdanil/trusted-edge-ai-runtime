/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_LINUX_PLATFORM_H
#define TEAR_LINUX_PLATFORM_H

#include "workload_adapter.h"

int tear_linux_platform_run_workload(const struct tear_workload_run *run,
                                     int *raw_status);

#endif
