// SPDX-License-Identifier: Apache-2.0

#include "telemetry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef TEAR_HOST_BUILD
#define TRUSTD_PATH "build/host/tear-trustd-host"
#define TEARICTL_PATH "build/host/tearictl-host"
#define RUNTIME_MANAGER_PATH "build/host/tear-runtime-manager-host"
#define DEMO_MODEL_PATH "build/host/demo-model-host"
#define MODEL_V1_PATH "examples/model-v1.json"
#define MODEL_V2_PATH "examples/model-v2.json"
#else
#define TRUSTD_PATH "/bin/tear-trustd"
#define TEARICTL_PATH "/bin/tearictl"
#define RUNTIME_MANAGER_PATH "/bin/tear-runtime-manager"
#define DEMO_MODEL_PATH "/bin/demo-model"
#define MODEL_V1_PATH "/etc/tear/model-v1.json"
#define MODEL_V2_PATH "/etc/tear/model-v2.json"
#endif

static void poweroff_guest(void)
{
    sync();
    reboot(RB_POWER_OFF);
}

static const char *parse_workload(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
    }

    return "/bin/tear-hello";
}

static pid_t start_trustd(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork trustd");
        return -1;
    }

    if (pid == 0) {
        execl(TRUSTD_PATH,
              TRUSTD_PATH,
              NULL);

        perror("execl trustd");
        _exit(127);
    }

    sleep(1);

    return pid;
}

static int run_tearictl(const char *command, const char *arg)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork tearictl");
        return -1;
    }

    if (pid == 0) {
        if (arg) {
            execl(TEARICTL_PATH,
                  TEARICTL_PATH,
                  command,
                  arg,
                  NULL);
        } else {
            execl(TEARICTL_PATH,
                  TEARICTL_PATH,
                  command,
                  NULL);
        }

        perror("execl tearictl");
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0)
        return -1;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    tear_event("supervisor_start");

    pid_t trustd_pid = start_trustd();

    if (trustd_pid < 0) {
        tear_event("trustd_start_failed");
        poweroff_guest();
        return 1;
    }

    tear_event("provisioning_start");

    if (run_tearictl("enroll", MODEL_V1_PATH) < 0) {
        tear_event("provisioning_failed");
        poweroff_guest();
        return 1;
    }

    tear_event("provisioning_done");

    if (run_tearictl("report", NULL) < 0) {
        tear_event("provisioning_report_failed");
        poweroff_guest();
        return 1;
    }

    tear_event("provisioning_report_done");

    tear_event("model_update_start");
    if (run_tearictl("update-model", MODEL_V2_PATH) < 0) {
        tear_event("model_update_failed");
        poweroff_guest();
        return 1;
    }
    tear_event("model_update_done");

    tear_event("rollback_validation_start");
    if (run_tearictl("update-model", MODEL_V1_PATH) == 0) {
        tear_event("rollback_validation_failed");
        poweroff_guest();
        return 1;
    }
    tear_event("rollback_validation_done");

    const char *workload = parse_workload(argc, argv);

    tear_event("workload_start");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event_kv("supervisor_error", "errno", errno);
        poweroff_guest();
        return 1;
    }

    if (pid == 0) {
        execl(RUNTIME_MANAGER_PATH,
              RUNTIME_MANAGER_PATH,
              "--workload",
              workload,
              "--manifest",
              MODEL_V2_PATH,
              NULL);

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
