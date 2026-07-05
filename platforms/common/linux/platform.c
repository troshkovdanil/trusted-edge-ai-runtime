/* SPDX-License-Identifier: Apache-2.0 */

#include "platform.h"

#include "observability.h"
#include "workload_contract.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEAR_COMPONENT "platform_linux"

static void exec_workload(const struct tear_workload_run *run)
{
    char command[1024];

    if (run->extra_args && run->extra_args[0] != '\0') {
        snprintf(command, sizeof(command),
                 "%s %s %s %s %s %s %s %s",
                 run->workload,
                 TEAR_WORKLOAD_ARG_PROFILE,
                 run->profile_path,
                 TEAR_WORKLOAD_ARG_RUN_ID,
                 run->run_id,
                 TEAR_WORKLOAD_ARG_EVENT_LOG,
                 run->workload_event_log,
                 run->extra_args);

        execl("/bin/sh", "sh", "-c", command, NULL);
    } else {
        execl(run->workload,
              run->workload,
              TEAR_WORKLOAD_ARG_PROFILE,
              run->profile_path,
              TEAR_WORKLOAD_ARG_RUN_ID,
              run->run_id,
              TEAR_WORKLOAD_ARG_EVENT_LOG,
              run->workload_event_log,
              NULL);
    }

    tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
             "failed to exec workload %s: %s",
             run->workload, strerror(errno));
    _exit(127);
}

int tear_linux_platform_run_workload(const struct tear_workload_run *run,
                                     int *raw_status)
{
    pid_t pid;
    int status = 0;

    if (!run || !raw_status)
        return -1;

    tear_event(TEAR_COMPONENT, "platform_workload_launch");

    pid = fork();
    if (pid < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to fork workload: %s", strerror(errno));
        tear_event(TEAR_COMPONENT, "platform_workload_fork_failed");
        return -1;
    }

    if (pid == 0)
        exec_workload(run);

    if (waitpid(pid, &status, 0) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to wait for workload: %s", strerror(errno));
        tear_event(TEAR_COMPONENT, "platform_workload_wait_failed");
        return -1;
    }

    *raw_status = status;

    tear_event(TEAR_COMPONENT, "platform_workload_exit");

    return 0;
}
