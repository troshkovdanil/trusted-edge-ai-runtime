// SPDX-License-Identifier: Apache-2.0

#include "telemetry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

static void poweroff_guest(void)
{
    sync();
    reboot(RB_POWER_OFF);
}

int main(void)
{
    const char *workload = "/bin/tear-hello";

    tear_event("supervisor_start");
    tear_event("workload_start");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event_kv("supervisor_error", "errno", errno);
        poweroff_guest();
        return 1;
    }

    if (pid == 0) {
        execl(workload, workload, NULL);
        perror("execl");
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        tear_event_kv("supervisor_error", "errno", errno);
        poweroff_guest();
        return 1;
    }

    if (WIFEXITED(status)) {
        tear_event_kv("workload_exit", "status", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        tear_event_kv("workload_signal", "signal", WTERMSIG(status));
    } else {
        tear_event("workload_unknown_exit");
    }

    tear_event("supervisor_shutdown");
    poweroff_guest();

    return 0;
}
