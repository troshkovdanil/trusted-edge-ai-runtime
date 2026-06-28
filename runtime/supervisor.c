// SPDX-License-Identifier: Apache-2.0

#include "runtime_paths.h"
#include "observability.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
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
#define DEFAULT_EVENT_PATH "build/host/tear-supervisor-events.log"
#define TRUSTD_EVENT_PATH "build/host/tear-trustd-events.log"
#define OPTD_EVENT_PATH "build/host/tear-optd-events.log"
#define RUNTIME_MANAGER_EVENT_PATH "build/host/tear-runtime-manager-events.log"
#else
#define TRUSTD_PATH "/bin/tear-trustd"
#define OPTD_PATH "/bin/tear-optd"
#define TEARICTL_PATH "/bin/tearictl"
#define RUNTIME_MANAGER_PATH "/bin/tear-runtime-manager"
#define DEFAULT_EVENT_PATH "/tmp/tear-supervisor-events.log"
#define TRUSTD_EVENT_PATH "/tmp/tear-trustd-events.log"
#define OPTD_EVENT_PATH "/tmp/tear-optd-events.log"
#define RUNTIME_MANAGER_EVENT_PATH "/tmp/tear-runtime-manager-events.log"
#endif

#define TEAR_COMPONENT "supervisor"

enum supervisor_state {
    SUPERVISOR_STATE_INIT,
    SUPERVISOR_STATE_READY,
    SUPERVISOR_STATE_RUNNING,
    SUPERVISOR_STATE_SHUTTING_DOWN,
};

struct tear_run_config {
    const char *workload;
    const char *manifest;
    const char *profile;
    const char *args;
};

struct supervisor_config {
    const char *event_log;
};

static enum supervisor_state supervisor_state = SUPERVISOR_STATE_INIT;
static volatile sig_atomic_t supervisor_running = 1;

static int run_workload_once(const struct tear_run_config *cfg);
static int run_plan_file(const char *path);
static int provision_plan_file(const char *path);
static const char *state_name(enum supervisor_state state);

static void supervisor_event(const char *event)
{
    tear_event(TEAR_COMPONENT, event);
}

static void supervisor_event_kv(const char *event,
                                const char *key,
                                long value)
{
    tear_event_kv(TEAR_COMPONENT, event, key, value);
}

static void supervisor_perror(const char *msg)
{
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "%s: %s",
             msg,
             strerror(errno));
}

static void client_reply(int client, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vdprintf(client, fmt, ap);
    va_end(ap);
}

static void client_reply_ok(int client)
{
    client_reply(client, "OK\n");
}

static void client_reply_err(int client, const char *reason)
{
    if (reason)
        client_reply(client, "ERR %s\n", reason);
    else
        client_reply(client, "ERR\n");
}

static void client_reply_pong(int client)
{
    client_reply(client, "PONG\n");
}

static void client_reply_status(int client)
{
    client_reply(client, "STATUS %s\n", state_name(supervisor_state));
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
                             struct tear_run_config *run_cfg)
{
    char local[512];
    char *cursor;
    char *command;
    char *workload;
    char *manifest;
    char *profile;
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

    workload = next_token(&cursor);
    manifest = next_token(&cursor);
    profile = next_token(&cursor);

    if (!workload || !manifest || !profile)
        return -1;

    while ((token = next_token(&cursor)) != NULL) {
        if (strcmp(token, "--") == 0) {
            while (*cursor == ' ' || *cursor == '\t')
                cursor++;
            args = cursor;
            break;
        }

        return -1;
    }

    run_cfg->workload = strdup(workload);
    run_cfg->manifest = strdup(manifest);
    run_cfg->profile = strdup(profile);
    run_cfg->args = args && args[0] != '\0' ? strdup(args) : strdup("");

    if (!run_cfg->workload ||
        !run_cfg->manifest ||
        !run_cfg->profile ||
        !run_cfg->args)
        return -1;

    return 0;
}

static void free_run_config(struct tear_run_config *cfg)
{
    free((void *)cfg->workload);
    free((void *)cfg->manifest);
    free((void *)cfg->profile);
    free((void *)cfg->args);
}

static int run_command_line(const char *line)
{
    struct tear_run_config run_cfg;
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
        supervisor_perror("open plan");
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

static pid_t start_trustd(void)
{
    const char *trustd_path = getenv("TEAR_TRUSTD_PATH");
    const char *trustd_backend = getenv("TEAR_TRUSTD_BACKEND");
    pid_t pid = fork();

    if (!trustd_path)
        trustd_path = TRUSTD_PATH;

    if (pid < 0) {
        supervisor_perror("fork trustd");
        return -1;
    }

    if (pid == 0) {
        if (trustd_backend) {
            execl(trustd_path,
                  trustd_path,
                  "--backend",
                  trustd_backend,
                  "--event-log",
                  TRUSTD_EVENT_PATH,
                  NULL);
        } else {
            execl(trustd_path,
                  trustd_path,
                  "--event-log",
                  TRUSTD_EVENT_PATH,
                  NULL);
        }

        supervisor_perror("execl trustd");
        _exit(127);
    }

    sleep(1);
    return pid;
}

static pid_t start_optd(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        supervisor_perror("fork optd");
        return -1;
    }

    if (pid == 0) {
        execl(OPTD_PATH,
              OPTD_PATH,
              "--event-log",
              OPTD_EVENT_PATH,
              NULL);

        supervisor_perror("execl optd");
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
    int status = 0;

    if (pid < 0) {
        supervisor_perror("fork tearictl");
        return -1;
    }

    if (pid == 0) {
        if (arg)
            execl(TEARICTL_PATH, TEARICTL_PATH, command, arg, NULL);
        else
            execl(TEARICTL_PATH, TEARICTL_PATH, command, NULL);

        supervisor_perror("execl tearictl");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0)
        return -1;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int provision_manifest_path(const char *manifest)
{
    supervisor_event("provisioning_start");

    if (run_tearictl("enroll", manifest) < 0) {
        supervisor_event("provisioning_failed");
        return -1;
    }

    supervisor_event("provisioning_done");

    if (run_tearictl("report", NULL) < 0) {
        supervisor_event("provisioning_report_failed");
        return -1;
    }

    supervisor_event("provisioning_report_done");

    return 0;
}

static int provision_command_line(const char *line)
{
    struct tear_run_config run_cfg;
    int ret;

    if (parse_run_command(line, &run_cfg) < 0)
        return -1;

    ret = provision_manifest_path(run_cfg.manifest);
    free_run_config(&run_cfg);

    return ret;
}

static int provision_plan_file(const char *path)
{
    FILE *fp;
    char line[512];
    int line_no = 0;

    fp = fopen(path, "r");
    if (!fp) {
        supervisor_perror("open provision plan");
        supervisor_event("provision_plan_open_failed");
        return -1;
    }

    supervisor_event("provision_plan_start");

    while (fgets(line, sizeof(line), fp)) {
        line_no++;

        if (should_skip_plan_line(line))
            continue;

        if (strncmp(line, "RUN ", 4) != 0) {
            supervisor_event_kv("provision_plan_invalid_line", "line", line_no);
            fclose(fp);
            return -1;
        }

        if (provision_command_line(line) < 0) {
            supervisor_event_kv("provision_plan_failed", "line", line_no);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    supervisor_event("provision_plan_done");

    return 0;
}

static int update_model_path(const char *manifest)
{
    supervisor_event("model_update_start");

    if (run_tearictl("update-model", manifest) < 0) {
        supervisor_event("model_update_failed");
        return -1;
    }

    supervisor_event("model_update_done");

    return 0;
}

static int report_trusted_state(void)
{
    if (run_tearictl("report", NULL) < 0) {
        supervisor_event("report_failed");
        return -1;
    }

    supervisor_event("report_done");

    return 0;
}

static int report_trusted_decision(void)
{
    if (run_tearictl("report-decision", NULL) < 0) {
        supervisor_event("report_decision_failed");
        return -1;
    }

    supervisor_event("report_decision_done");

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
        client_reply_pong(client);
    } else if (strncmp(buf, "STATUS", 6) == 0) {
        client_reply_status(client);
    } else if (strncmp(buf, "PROVISION_PLAN ", 15) == 0) {
        char path[256];
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            client_reply_err(client, "busy");
            return;
        }

        if (sscanf(buf, "PROVISION_PLAN %255s", path) != 1) {
            client_reply_err(client, "invalid_provision_plan_command");
            return;
        }

        ret = provision_plan_file(path);

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "provision_plan_failed");
    } else if (strncmp(buf, "PROVISION ", 10) == 0) {
        char path[256];
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            client_reply_err(client, "busy");
            return;
        }

        if (sscanf(buf, "PROVISION %255s", path) != 1) {
            client_reply_err(client, "invalid_provision_command");
            return;
        }

        ret = provision_manifest_path(path);

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "provision_failed");
    } else if (strncmp(buf, "UPDATE_MODEL ", 13) == 0) {
        char path[256];
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            client_reply_err(client, "busy");
            return;
        }

        if (sscanf(buf, "UPDATE_MODEL %255s", path) != 1) {
            client_reply_err(client, "invalid_update_model_command");
            return;
        }

        ret = update_model_path(path);

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "update_model_failed");
    } else if (strncmp(buf, "REPORT_DECISION", 15) == 0) {
        int ret;

        ret = report_trusted_decision();

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "report_decision_failed");
    } else if (strncmp(buf, "REPORT", 6) == 0) {
        int ret;

        ret = report_trusted_state();

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "report_failed");
    } else if (strncmp(buf, "RUN_PLAN ", 9) == 0) {
        char path[256];
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            client_reply_err(client, "busy");
            return;
        }

        if (sscanf(buf, "RUN_PLAN %255s", path) != 1) {
            client_reply_err(client, "invalid_run_plan_command");
            return;
        }

        ret = run_plan_file(path);

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "run_plan_failed");
    } else if (strncmp(buf, "RUN ", 4) == 0) {
        struct tear_run_config run_cfg;
        int ret;

        if (supervisor_state == SUPERVISOR_STATE_RUNNING) {
            client_reply_err(client, "busy");
            return;
        }

        if (parse_run_command(buf, &run_cfg) < 0) {
            client_reply_err(client, "invalid_run_command");
            return;
        }

        ret = run_workload_once(&run_cfg);
        free_run_config(&run_cfg);

        if (ret == 0)
            client_reply_ok(client);
        else
            client_reply_err(client, "run_failed");
    } else {
        client_reply_err(client, "unknown_command");
    }
}

static struct supervisor_config parse_args(int argc, char **argv)
{
    struct supervisor_config cfg = {
        .event_log = DEFAULT_EVENT_PATH,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--event-log") == 0 && i + 1 < argc) {
            cfg.event_log = argv[++i];
        } else {
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "usage: tear-supervisor [--event-log <path>]");
            cfg.event_log = NULL;
            break;
        }
    }

    return cfg;
}

static int validate_config(const struct supervisor_config *cfg)
{
    if (!cfg->event_log || cfg->event_log[0] == '\0') {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "missing --event-log <path>");
        return -1;
    }

    return 0;
}

static void run_runtime_manager(const struct tear_run_config *cfg)
{
    execl(RUNTIME_MANAGER_PATH,
          RUNTIME_MANAGER_PATH,
          "--workload",
          cfg->workload,
          "--manifest",
          cfg->manifest,
          "--profile",
          cfg->profile,
          "--args",
          cfg->args,
          "--event-log",
          RUNTIME_MANAGER_EVENT_PATH,
          NULL);
}

static int run_workload_once(const struct tear_run_config *cfg)
{
    pid_t pid;
    int status = 0;

    supervisor_event("workload_selected");

    supervisor_state = SUPERVISOR_STATE_RUNNING;
    supervisor_event("workload_start");

    pid = fork();

    if (pid < 0) {
        supervisor_perror("fork");
        supervisor_event_kv("supervisor_error", "errno", errno);
        supervisor_state = SUPERVISOR_STATE_READY;
        return 1;
    }

    if (pid == 0) {
        run_runtime_manager(cfg);
        supervisor_perror("execl runtime manager");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        supervisor_perror("waitpid");
        supervisor_event_kv("supervisor_error", "errno", errno);
        supervisor_state = SUPERVISOR_STATE_READY;
        return 1;
    }

    if (WIFEXITED(status)) {
        supervisor_event_kv("workload_exit",
                            "status",
                            WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        supervisor_event_kv("workload_signal",
                            "signal",
                            WTERMSIG(status));
    } else {
        supervisor_event("workload_unknown_exit");
    }

    supervisor_state = SUPERVISOR_STATE_READY;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}

static int run_supervisor_daemon(pid_t trustd_pid, pid_t optd_pid)
{
    int server = create_supervisor_socket();

    if (server < 0) {
        supervisor_perror("supervisor socket");
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

            supervisor_perror("accept supervisor");
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
    int ret;
    struct supervisor_config cfg = parse_args(argc, argv);
    pid_t trustd_pid;
    pid_t optd_pid;

    if (validate_config(&cfg) < 0)
        return 1;

    if (tear_event_init(cfg.event_log) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize events");
        return 1;
    }

    supervisor_event("supervisor_start");

    trustd_pid = start_trustd();

    if (trustd_pid < 0) {
        supervisor_event("trustd_start_failed");
        tear_event_shutdown();
        return 1;
    }

    optd_pid = start_optd();

    if (optd_pid < 0) {
        supervisor_event("optd_start_failed");
        stop_child(trustd_pid);
        tear_event_shutdown();
        return 1;
    }

    ret = run_supervisor_daemon(trustd_pid, optd_pid);

    supervisor_event("supervisor_shutdown");
    tear_event_shutdown();

    return ret;
}
