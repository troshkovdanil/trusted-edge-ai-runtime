// SPDX-License-Identifier: Apache-2.0

#include "runtime_paths.h"
#include "telemetry.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
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
#define OPTD_PATH "/bin/tear-optd"
#define TEARICTL_PATH "/bin/tearictl"
#define RUNTIME_MANAGER_PATH "/bin/tear-runtime-manager"
#define MODEL_V1_PATH "/etc/tear/model-v1.json"
#define MODEL_V2_PATH "/etc/tear/model-v2.json"
#endif

enum supervisor_state {
    SUPERVISOR_STATE_INIT,
    SUPERVISOR_STATE_READY,
    SUPERVISOR_STATE_RUNNING,
    SUPERVISOR_STATE_SHUTTING_DOWN,
};

struct supervisor_config {
    const char *workload;
    const char *manifest;
    int enable_optimizer;
    int daemon_mode;
};

static enum supervisor_state supervisor_state = SUPERVISOR_STATE_INIT;
static volatile sig_atomic_t supervisor_running = 1;
static int run_workload_once(const struct supervisor_config *cfg);

static void handle_signal(int signo)
{
    (void)signo;
    supervisor_running = 0;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

static const char *state_name(enum supervisor_state state)
{
    switch (state) {
    case SUPERVISOR_STATE_INIT:
        return "INIT";
    case SUPERVISOR_STATE_READY:
        return "READY";
    case SUPERVISOR_STATE_RUNNING:
        return "RUNNING";
    case SUPERVISOR_STATE_SHUTTING_DOWN:
        return "SHUTTING_DOWN";
    default:
        return "UNKNOWN";
    }
}

static int create_supervisor_socket(void)
{
    const char *socket_path = tear_supervisor_socket_path();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int parse_run_command(const char *buf,
                             struct supervisor_config *run_cfg)
{
    char workload[256];
    char manifest[256];
    char opt[32] = {0};

    memset(run_cfg, 0, sizeof(*run_cfg));

    int fields = sscanf(buf,
                        "RUN %255s %255s %31s",
                        workload,
                        manifest,
                        opt);

    if (fields < 2)
        return -1;

    run_cfg->workload = strdup(workload);
    run_cfg->manifest = strdup(manifest);

    if (!run_cfg->workload || !run_cfg->manifest)
        return -1;

    if (fields == 3 && strcmp(opt, "optimizer") == 0)
        run_cfg->enable_optimizer = 1;

    return 0;
}

static void free_run_config(struct supervisor_config *cfg)
{
    free((void *)cfg->workload);
    free((void *)cfg->manifest);
}

static void handle_supervisor_client(int client)
{
    char buf[128];
    ssize_t n = read(client, buf, sizeof(buf) - 1);

    if (n <= 0)
        return;

    buf[n] = '\0';

    if (strncmp(buf, "PING", 4) == 0) {
        dprintf(client, "PONG\n");
    } else if (strncmp(buf, "STATUS", 6) == 0) {
        dprintf(client,
                "STATUS %s\n",
                state_name(supervisor_state));
    } else if (strncmp(buf, "RUN ", 4) == 0) {
        struct supervisor_config run_cfg;
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
           dprintf(client, "ERR busy\n");
           return;
        }

        if (parse_run_command(buf, &run_cfg) < 0) {
            dprintf(client, "ERR invalid_run_command\n");
            return;
        }

        ret = run_workload_once(&run_cfg);

        free_run_config(&run_cfg);

        if (ret == 0)
            dprintf(client, "OK\n");
        else
            dprintf(client, "ERR run_failed\n");
    } else {
        dprintf(client, "ERR unknown_command\n");
    }
}

static struct supervisor_config parse_args(int argc, char **argv)
{
    struct supervisor_config cfg = {
        .workload = "/bin/tear-hello",
        .manifest = MODEL_V2_PATH,
        .enable_optimizer = 0,
        .daemon_mode = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--enable-optimizer") == 0) {
            cfg.enable_optimizer = 1;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            cfg.daemon_mode = 1;
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

static void stop_child(pid_t pid)
{
    if (pid <= 0)
        return;

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
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
}

static int run_workload_once(const struct supervisor_config *cfg)
{
    if (cfg->enable_optimizer) {
        if (provision_selected_manifest(cfg->manifest) < 0)
            return 1;
    } else {
        if (provision_demo_model() < 0)
            return 1;
    }

    supervisor_state = SUPERVISOR_STATE_RUNNING;
    tear_event("workload_start");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event_kv("supervisor_error", "errno", errno);
        supervisor_state = SUPERVISOR_STATE_READY;
        return 1;
    }

    if (pid == 0) {
        run_runtime_manager(cfg);

        perror("execl runtime manager");
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        tear_event_kv("supervisor_error", "errno", errno);
        supervisor_state = SUPERVISOR_STATE_READY;
        return 1;
    }

    if (WIFEXITED(status)) {
        tear_event_kv("workload_exit", "status", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        tear_event_kv("workload_signal", "signal", WTERMSIG(status));
    } else {
        tear_event("workload_unknown_exit");
    }

    supervisor_state = SUPERVISOR_STATE_READY;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

static int run_supervisor_daemon(pid_t trustd_pid, pid_t optd_pid)
{
    int server = create_supervisor_socket();

    if (server < 0) {
        perror("supervisor socket");
        tear_event("supervisor_socket_failed");
        return 1;
    }

    install_signal_handlers();

    supervisor_state = SUPERVISOR_STATE_READY;
    tear_event("supervisor_daemon_ready");

    while (supervisor_running) {
        int client = accept(server, NULL, NULL);

        if (client < 0) {
            if (errno == EINTR)
                break;

            perror("accept supervisor");
            continue;
        }

        handle_supervisor_client(client);
        close(client);
    }

    supervisor_state = SUPERVISOR_STATE_SHUTTING_DOWN;
    tear_event("supervisor_daemon_shutdown");

    close(server);
    unlink(tear_supervisor_socket_path());

    stop_child(optd_pid);
    stop_child(trustd_pid);

    return 0;
}

int main(int argc, char **argv)
{
    struct supervisor_config cfg = parse_args(argc, argv);

    tear_event("supervisor_start");

    pid_t trustd_pid = start_trustd();

    if (trustd_pid < 0) {
        tear_event("trustd_start_failed");
        return 1;
    }

    pid_t optd_pid = -1;

    if (cfg.enable_optimizer) {
        optd_pid = start_optd();

        if (optd_pid < 0) {
            tear_event("optd_start_failed");
            stop_child(trustd_pid);
            return 1;
        }
    }

    if (cfg.daemon_mode)
        return run_supervisor_daemon(trustd_pid, optd_pid);

    supervisor_state = SUPERVISOR_STATE_READY;

    int ret = run_workload_once(&cfg);

    stop_child(optd_pid);
    stop_child(trustd_pid);

    tear_event("supervisor_shutdown");

    return ret;
}
