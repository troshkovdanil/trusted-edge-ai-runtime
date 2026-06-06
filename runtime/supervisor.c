// SPDX-License-Identifier: Apache-2.0

#include "telemetry.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef TEAR_HOST_BUILD
#define TRUSTD_PATH "build/host/tear-trustd-host"
#define OPTD_PATH "build/host/tear-optd-host"
#define TEARICTL_PATH "build/host/tearictl-host"
#define RUNTIME_MANAGER_PATH "build/host/tear-runtime-manager-host"
#define MODEL_V1_PATH "examples/model-v1.json"
#define MODEL_V2_PATH "examples/model-v2.json"
#else
#define TRUSTD_PATH "/bin/tear-trustd"
#define TEARICTL_PATH "/bin/tearictl"
#define RUNTIME_MANAGER_PATH "/bin/tear-runtime-manager"
#define MODEL_V1_PATH "/etc/tear/model-v1.json"
#define MODEL_V2_PATH "/etc/tear/model-v2.json"
#endif

struct supervisor_config {
    const char *workload;
    const char *manifest;
    int enable_optimizer;
};

static void poweroff_guest(void)
{
    sync();
    reboot(RB_POWER_OFF);
}

static struct supervisor_config parse_args(int argc, char **argv)
{
    struct supervisor_config cfg = {
        .workload = "/bin/tear-hello",
        .manifest = MODEL_V2_PATH,
        .enable_optimizer = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--enable-optimizer") == 0) {
            cfg.enable_optimizer = 1;
        }
    }

    return cfg;
}

static pid_t start_trustd(void)
{
    const char *trustd_path = getenv("TEAR_TRUSTD_PATH");
    const char *trustd_backend = getenv("TEAR_TRUSTD_BACKEND");
    pid_t pid = fork();

    if (!trustd_path)
        trustd_path = TRUSTD_PATH;

    if (pid < 0) {
        perror("fork trustd");
        return -1;
    }

    if (pid == 0) {
        if (trustd_backend) {
            execl(trustd_path,
                  trustd_path,
                  "--backend",
                  trustd_backend,
                  NULL);
        } else {
            execl(trustd_path, trustd_path, NULL);
        }

        perror("execl trustd");
        _exit(127);
    }

    sleep(1);

    return pid;
}

#ifdef TEAR_HOST_BUILD
static pid_t start_optd(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork optd");
        return -1;
    }

    if (pid == 0) {
        execl(OPTD_PATH, OPTD_PATH, NULL);

        perror("execl optd");
        _exit(127);
    }

    sleep(1);

    return pid;
}
#endif

static int run_tearictl(const char *command, const char *arg)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork tearictl");
        return -1;
    }

    if (pid == 0) {
        if (arg) {
            execl(TEARICTL_PATH, TEARICTL_PATH, command, arg, NULL);
        } else {
            execl(TEARICTL_PATH, TEARICTL_PATH, command, NULL);
        }

        perror("execl tearictl");
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0)
        return -1;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int provision_demo_model(void)
{
    tear_event("provisioning_start");

    if (run_tearictl("enroll", MODEL_V1_PATH) < 0) {
        tear_event("provisioning_failed");
        return -1;
    }

    tear_event("provisioning_done");

    if (run_tearictl("report", NULL) < 0) {
        tear_event("provisioning_report_failed");
        return -1;
    }

    tear_event("provisioning_report_done");

    tear_event("model_update_start");

    if (run_tearictl("update-model", MODEL_V2_PATH) < 0) {
        tear_event("model_update_failed");
        return -1;
    }

    tear_event("model_update_done");

    tear_event("rollback_validation_start");

    if (run_tearictl("update-model", MODEL_V1_PATH) == 0) {
        tear_event("rollback_validation_failed");
        return -1;
    }

    tear_event("rollback_validation_done");

    return 0;
}

static int provision_selected_manifest(const char *manifest)
{
    tear_event("provisioning_start");

    if (run_tearictl("enroll", manifest) < 0) {
        tear_event("provisioning_failed");
        return -1;
    }

    tear_event("provisioning_done");

    if (run_tearictl("report", NULL) < 0) {
        tear_event("provisioning_report_failed");
        return -1;
    }

    tear_event("provisioning_report_done");

    return 0;
}

static void run_runtime_manager(const struct supervisor_config *cfg)
{
#ifdef TEAR_HOST_BUILD
    if (cfg->enable_optimizer) {
        execl(RUNTIME_MANAGER_PATH,
              RUNTIME_MANAGER_PATH,
              "--workload",
              cfg->workload,
              "--manifest",
              cfg->manifest,
              "--enable-optimizer",
              NULL);
    } else {
        execl(RUNTIME_MANAGER_PATH,
              RUNTIME_MANAGER_PATH,
              "--workload",
              cfg->workload,
              "--manifest",
              cfg->manifest,
              NULL);
    }
#else
    execl(RUNTIME_MANAGER_PATH,
          RUNTIME_MANAGER_PATH,
          "--workload",
          cfg->workload,
          "--manifest",
          cfg->manifest,
          NULL);
#endif
}

int main(int argc, char **argv)
{
    struct supervisor_config cfg = parse_args(argc, argv);

    tear_event("supervisor_start");

    pid_t trustd_pid = start_trustd();

    if (trustd_pid < 0) {
        tear_event("trustd_start_failed");
        poweroff_guest();
        return 1;
    }

#ifdef TEAR_HOST_BUILD
    pid_t optd_pid = -1;

    if (cfg.enable_optimizer) {
        optd_pid = start_optd();

        if (optd_pid < 0) {
            tear_event("optd_start_failed");
            poweroff_guest();
            return 1;
        }
    }
#endif

    if (cfg.enable_optimizer) {
        if (provision_selected_manifest(cfg.manifest) < 0) {
            poweroff_guest();
            return 1;
        }
    } else {
        if (provision_demo_model() < 0) {
            poweroff_guest();
            return 1;
        }
    }

    tear_event("workload_start");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event_kv("supervisor_error", "errno", errno);
        poweroff_guest();
        return 1;
    }

    if (pid == 0) {
        run_runtime_manager(&cfg);

        perror("execl runtime manager");
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

#ifdef TEAR_HOST_BUILD
    if (optd_pid > 0)
        kill(optd_pid, SIGTERM);
#endif

    tear_event("supervisor_shutdown");
    poweroff_guest();

    return 0;
}
