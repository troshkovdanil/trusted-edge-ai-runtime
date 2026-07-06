/* SPDX-License-Identifier: Apache-2.0 */

#include "linux/linux_platform.h"

#include "observability.h"
#include "workload_contract.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
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

int tear_linux_platform_start_process(const char *path,
                                      char *const argv[],
                                      tear_platform_process_t *process)
{
    pid_t pid;

    if (!path || !argv || !process)
        return -1;

    pid = fork();
    if (pid < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to fork process %s: %s",
                 path, strerror(errno));
        tear_event(TEAR_COMPONENT, "platform_process_fork_failed");
        return -1;
    }

    if (pid == 0) {
        execv(path, argv);

        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to exec process %s: %s",
                 path, strerror(errno));
        _exit(127);
    }

    *process = (tear_platform_process_t)pid;
    tear_event(TEAR_COMPONENT, "platform_process_started");

    return 0;
}

int tear_linux_platform_run_process(const char *path,
                                    char *const argv[],
                                    int *exit_code)
{
    tear_platform_process_t process;
    int status = 0;

    if (!exit_code)
        return -1;

    if (tear_linux_platform_start_process(path, argv, &process) < 0)
        return -1;

    if (waitpid((pid_t)process, &status, 0) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR,
                 "failed to wait for process %s: %s",
                 path, strerror(errno));
        tear_event(TEAR_COMPONENT, "platform_process_wait_failed");
        return -1;
    }

    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
        tear_event(TEAR_COMPONENT, "platform_process_exit");
        return 0;
    }

    if (WIFSIGNALED(status)) {
        *exit_code = 128 + WTERMSIG(status);
        tear_event(TEAR_COMPONENT, "platform_process_signal");
        return 0;
    }

    *exit_code = 1;
    tear_event(TEAR_COMPONENT, "platform_process_unknown_exit");

    return 0;
}

void tear_linux_platform_stop_process(tear_platform_process_t process)
{
    pid_t pid = (pid_t)process;

    if (process == TEAR_PLATFORM_INVALID_PROCESS || pid <= 0)
        return;

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}

void tear_linux_platform_sleep_ms(unsigned int milliseconds)
{
    usleep(milliseconds * 1000);
}

int tear_linux_platform_path_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return 0;

    return stat(path, &st) == 0;
}

int tear_linux_platform_process_exited(int status)
{
    return WIFEXITED(status);
}

int tear_linux_platform_process_exit_code(int status)
{
    return WEXITSTATUS(status);
}

int tear_linux_platform_socket_listen(const char *path,
                                      tear_platform_socket_t *socket_out)
{
    int fd;
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    if (!path || !socket_out || path[0] == '\0')
        return -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }

    *socket_out = fd;

    return 0;
}

int tear_linux_platform_socket_connect(const char *path,
                                       tear_platform_socket_t *socket_out)
{
    int fd;
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    if (!path || !socket_out || path[0] == '\0')
        return -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    *socket_out = fd;

    return 0;
}

int tear_linux_platform_socket_accept(tear_platform_socket_t server,
                                      tear_platform_socket_t *client_out)
{
    int client;

    if (server == TEAR_PLATFORM_INVALID_SOCKET || !client_out)
        return -1;

    client = accept(server, NULL, NULL);
    if (client < 0)
        return -1;

    *client_out = client;

    return 0;
}

ssize_t tear_linux_platform_socket_read(tear_platform_socket_t socket_fd,
                                        void *buf,
                                        size_t len)
{
    if (socket_fd == TEAR_PLATFORM_INVALID_SOCKET || !buf)
        return -1;

    return read(socket_fd, buf, len);
}

void tear_linux_platform_socket_close(tear_platform_socket_t socket_fd)
{
    if (socket_fd == TEAR_PLATFORM_INVALID_SOCKET)
        return;

    close(socket_fd);
}

void tear_linux_platform_socket_unlink(const char *path)
{
    if (!path || path[0] == '\0')
        return;

    unlink(path);
}
