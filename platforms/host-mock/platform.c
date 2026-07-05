/* SPDX-License-Identifier: Apache-2.0 */

#include "linux/linux_platform.h"

int tear_platform_run_workload(const struct tear_workload_run *run,
                               int *raw_status)
{
    return tear_linux_platform_run_workload(run, raw_status);
}

int tear_platform_start_process(const char *path,
                                char *const argv[],
                                tear_platform_process_t *process)
{
    return tear_linux_platform_start_process(path, argv, process);
}

int tear_platform_run_process(const char *path,
                              char *const argv[],
                              int *exit_code)
{
    return tear_linux_platform_run_process(path, argv, exit_code);
}

void tear_platform_stop_process(tear_platform_process_t process)
{
    tear_linux_platform_stop_process(process);
}

void tear_platform_sleep_ms(unsigned int milliseconds)
{
    tear_linux_platform_sleep_ms(milliseconds);
}

int tear_platform_socket_listen(const char *path,
                                tear_platform_socket_t *socket_out)
{
    return tear_linux_platform_socket_listen(path, socket_out);
}

int tear_platform_socket_accept(tear_platform_socket_t server,
                                tear_platform_socket_t *client_out)
{
    return tear_linux_platform_socket_accept(server, client_out);
}

ssize_t tear_platform_socket_read(tear_platform_socket_t socket_fd,
                                  void *buf,
                                  size_t len)
{
    return tear_linux_platform_socket_read(socket_fd, buf, len);
}

void tear_platform_socket_close(tear_platform_socket_t socket_fd)
{
    tear_linux_platform_socket_close(socket_fd);
}

void tear_platform_socket_unlink(const char *path)
{
    tear_linux_platform_socket_unlink(path);
}
