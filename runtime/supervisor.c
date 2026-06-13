// SPDX-License-Identifier: Apache-2.0

#include "runtime_paths.h"
#include "observability.h"

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
#else
#define TRUSTD_PATH "/bin/tear-trustd"
#define OPTD_PATH "/bin/tear-optd"
#define TEARICTL_PATH "/bin/tearictl"
#define RUNTIME_MANAGER_PATH "/bin/tear-runtime-manager"
#endif

#define TEAR_COMPONENT "supervisor"

enum supervisor_state {
    SUPERVISOR_STATE_INIT,
    SUPERVISOR_STATE_READY,
    SUPERVISOR_STATE_RUNNING,
    SUPERVISOR_STATE_SHUTTING_DOWN,
};

struct supervisor_config {
    const char *name;
    const char *workload;
    const char *manifest;
    const char *args;
    int enable_optimizer;
    int daemon_mode;
};

static enum supervisor_state supervisor_state = SUPERVISOR_STATE_INIT;
static volatile sig_atomic_t supervisor_running = 1;
static int run_workload_once(const struct supervisor_config *cfg);
static int run_plan_file(const char *path);

static void supervisor_event(const char *event)
{
    tear_event_ex(TEAR_COMPONENT, NULL, NULL, event);
}

static void supervisor_event_kv(const char *event,
                                const char *key,
                                long value)
{
    tear_event_ex_kv(TEAR_COMPONENT, NULL, NULL, event, key, value);
}

static void supervisor_workload_event(const char *workload,
                                      const char *event)
{
    tear_event_ex(TEAR_COMPONENT, workload, NULL, event);
}

static void supervisor_workload_event_kv(const char *workload,
                                         const char *event,
                                         const char *key,
                                         long value)
{
    tear_event_ex_kv(TEAR_COMPONENT, workload, NULL, event, key, value);
}

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

static char *trim_newline(char *s)
{
    size_t len = strlen(s);

    while (len > 0 &&
           (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }

    return s;
}

static char *next_token(char **cursor)
{
    char *start;

    while (**cursor == ' ' || **cursor == '\t')
        (*cursor)++;

    if (**cursor == '\0')
        return NULL;

    start = *cursor;

    while (**cursor != '\0' &&
           **cursor != ' ' &&
           **cursor != '\t')
        (*cursor)++;

    if (**cursor != '\0') {
        **cursor = '\0';
        (*cursor)++;
    }

    return start;
}

static int parse_run_command(const char *buf,
                             struct supervisor_config *run_cfg)
{
    char local[512];
    char *cursor;
    char *command;
    char *name;
    char *workload;
    char *manifest;
    char *token;
    char *args = NULL;

    memset(run_cfg, 0, sizeof(*run_cfg));

    strncpy(local, buf, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';
    trim_newline(local);

    cursor = local;

    command = next_token(&cursor);
    if (!command || strcmp(command, "RUN") != 0)
        return -1;

    name = next_token(&cursor);
    workload = next_token(&cursor);
    manifest = next_token(&cursor);

    if (!name || !workload || !manifest)
        return -1;

    while ((token = next_token(&cursor)) != NULL) {
        if (strcmp(token, "optimizer") == 0) {
            run_cfg->enable_optimizer = 1;
        } else if (strcmp(token, "--") == 0) {
            while (*cursor == ' ' || *cursor == '\t')
                cursor++;
            args = cursor;
            break;
        } else {
            return -1;
        }
    }

    run_cfg->name = strdup(name);
    run_cfg->workload = strdup(workload);
    run_cfg->manifest = strdup(manifest);

    if (args && args[0] != '\0')
        run_cfg->args = strdup(args);
    else
        run_cfg->args = strdup("");

    if (!run_cfg->name ||
        !run_cfg->workload ||
        !run_cfg->manifest ||
        !run_cfg->args)
        return -1;

    return 0;
}

static void free_run_config(struct supervisor_config *cfg)
{
    free((void *)cfg->name);
    free((void *)cfg->workload);
    free((void *)cfg->manifest);
    free((void *)cfg->args);
}

static int run_command_line(const char *line)
{
    struct supervisor_config run_cfg;
    int ret;

    if (parse_run_command(line, &run_cfg) < 0)
        return -1;

    ret = run_workload_once(&run_cfg);

    free_run_config(&run_cfg);

    return ret;
}

static int should_skip_plan_line(const char *line)
{
    while (*line == ' ' || *line == '\t')
        line++;

    return *line == '\0' || *line == '\n' || *line == '#';
}

static int run_plan_file(const char *path)
{
    FILE *fp;
    char line[512];
    int line_no = 0;

    fp = fopen(path, "r");
    if (!fp) {
        perror("open plan");
        supervisor_event("run_plan_open_failed");
        return -1;
    }

    supervisor_event("run_plan_start");

    while (fgets(line, sizeof(line), fp)) {
        line_no++;

        if (should_skip_plan_line(line))
            continue;

        if (strncmp(line, "RUN ", 4) != 0) {
            supervisor_event_kv("run_plan_invalid_line", "line", line_no);
            fclose(fp);
            return -1;
        }

        if (run_command_line(line) < 0) {
            supervisor_event_kv("run_plan_failed", "line", line_no);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    supervisor_event("run_plan_done");

    return 0;
}

static void handle_supervisor_client(int client)
{
    char buf[512];
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
    } else if (strncmp(buf, "RUN_PLAN ", 9) == 0) {
        char path[256];
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            dprintf(client, "ERR busy\n");
            return;
        }

        if (sscanf(buf, "RUN_PLAN %255s", path) != 1) {
            dprintf(client, "ERR invalid_run_plan_command\n");
            return;
        }

        ret = run_plan_file(path);

        if (ret == 0)
            dprintf(client, "OK\n");
        else
            dprintf(client, "ERR run_plan_failed\n");
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
        .name = "cli-workload",
        .workload = NULL,
        .manifest = NULL,
        .args = "",
        .enable_optimizer = 0,
        .daemon_mode = 0,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (strcmp(argv[i], "--manifest") == 0 && i + 1 < argc) {
            cfg.manifest = argv[++i];
        } else if (strcmp(argv[i], "--args") == 0 && i + 1 < argc) {
            cfg.args = argv[++i];
        } else if (strcmp(argv[i], "--enable-optimizer") == 0) {
            cfg.enable_optimizer = 1;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            cfg.daemon_mode = 1;
        }
    }

    return cfg;
}

static int validate_config(const struct supervisor_config *cfg)
{
    if (cfg->daemon_mode)
        return 0;

    if (!cfg->workload || !cfg->manifest) {
        fprintf(stderr,
                "usage: tear-supervisor "
                "--workload <path> "
                "--manifest <path> "
                "[--args <args>] "
                "[--enable-optimizer]\n");
        fprintf(stderr,
                "       tear-supervisor --daemon "
                "[--enable-optimizer]\n");
        return -1;
    }

    return 0;
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

static int provision_selected_manifest(const struct supervisor_config *cfg)
{
    supervisor_workload_event(cfg->name, "provisioning_start");

    if (run_tearictl("enroll", cfg->manifest) < 0) {
        supervisor_workload_event(cfg->name, "provisioning_failed");
        return -1;
    }

    supervisor_workload_event(cfg->name, "provisioning_done");

    if (run_tearictl("report", NULL) < 0) {
        supervisor_workload_event(cfg->name, "provisioning_report_failed");
        return -1;
    }

    supervisor_workload_event(cfg->name, "provisioning_report_done");

    return 0;
}

static void run_runtime_manager(const struct supervisor_config *cfg)
{
    if (cfg->enable_optimizer) {
        execl(RUNTIME_MANAGER_PATH,
              RUNTIME_MANAGER_PATH,
              "--name",
              cfg->name,
              "--workload",
              cfg->workload,
              "--manifest",
              cfg->manifest,
              "--args",
              cfg->args,
              "--enable-optimizer",
              NULL);
    } else {
        execl(RUNTIME_MANAGER_PATH,
              RUNTIME_MANAGER_PATH,
              "--name",
              cfg->name,
              "--workload",
              cfg->workload,
              "--manifest",
              cfg->manifest,
              "--args",
              cfg->args,
              NULL);
    }
}

static int run_workload_once(const struct supervisor_config *cfg)
{
    supervisor_workload_event(cfg->name, "workload_selected");

    if (provision_selected_manifest(cfg) < 0)
        return 1;

    supervisor_state = SUPERVISOR_STATE_RUNNING;
    supervisor_workload_event(cfg->name, "workload_start");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        supervisor_workload_event_kv(cfg->name,
                                     "supervisor_error",
                                     "errno",
                                     errno);
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
        supervisor_workload_event_kv(cfg->name,
                                     "supervisor_error",
                                     "errno",
                                     errno);
        supervisor_state = SUPERVISOR_STATE_READY;
        return 1;
    }

    if (WIFEXITED(status)) {
        supervisor_workload_event_kv(cfg->name,
                                     "workload_exit",
                                     "status",
                                     WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        supervisor_workload_event_kv(cfg->name,
                                     "workload_signal",
                                     "signal",
                                     WTERMSIG(status));
    } else {
        supervisor_workload_event(cfg->name, "workload_unknown_exit");
    }

    supervisor_state = SUPERVISOR_STATE_READY;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

static int run_supervisor_daemon(pid_t trustd_pid, pid_t optd_pid)
{
    int server = create_supervisor_socket();

    if (server < 0) {
        perror("supervisor socket");
        supervisor_event("supervisor_socket_failed");
        return 1;
    }

    install_signal_handlers();

    supervisor_state = SUPERVISOR_STATE_READY;
    supervisor_event("supervisor_daemon_ready");

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
    supervisor_event("supervisor_daemon_shutdown");

    close(server);
    unlink(tear_supervisor_socket_path());

    stop_child(optd_pid);
    stop_child(trustd_pid);

    return 0;
}

int main(int argc, char **argv)
{
    struct supervisor_config cfg = parse_args(argc, argv);

    if (validate_config(&cfg) < 0)
        return 1;

    supervisor_event("supervisor_start");

    pid_t trustd_pid = start_trustd();

    if (trustd_pid < 0) {
        supervisor_event("trustd_start_failed");
        return 1;
    }

    pid_t optd_pid = -1;

    if (cfg.enable_optimizer) {
        optd_pid = start_optd();

        if (optd_pid < 0) {
            supervisor_event("optd_start_failed");
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

    supervisor_event("supervisor_shutdown");

    return ret;
}
