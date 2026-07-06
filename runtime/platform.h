/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_PLATFORM_H
#define TEAR_PLATFORM_H

#include "workload_adapter.h"

#include <stddef.h>
#include <sys/types.h>

typedef long tear_platform_process_t;
typedef int tear_platform_socket_t;

#define TEAR_PLATFORM_INVALID_PROCESS ((tear_platform_process_t)-1)
#define TEAR_PLATFORM_INVALID_SOCKET ((tear_platform_socket_t)-1)

int tear_platform_run_workload(const struct tear_workload_run *run,
                               int *raw_status);

int tear_platform_start_process(const char *path,
                                char *const argv[],
                                tear_platform_process_t *process);

int tear_platform_run_process(const char *path,
                              char *const argv[],
                              int *exit_code);

void tear_platform_stop_process(tear_platform_process_t process);

void tear_platform_sleep_ms(unsigned int milliseconds);

int tear_platform_path_exists(const char *path);

int tear_platform_process_exited(int status);

int tear_platform_process_exit_code(int status);

int tear_platform_socket_listen(const char *path,
                                tear_platform_socket_t *socket_out);

int tear_platform_socket_connect(const char *path,
                                 tear_platform_socket_t *socket_out);

int tear_platform_socket_accept(tear_platform_socket_t server,
                                tear_platform_socket_t *client_out);

ssize_t tear_platform_socket_read(tear_platform_socket_t socket_fd,
                                  void *buf,
                                  size_t len);

void tear_platform_socket_close(tear_platform_socket_t socket_fd);

void tear_platform_socket_unlink(const char *path);

#endif
