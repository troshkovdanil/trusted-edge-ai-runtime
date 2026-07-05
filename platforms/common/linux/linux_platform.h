/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_LINUX_PLATFORM_H
#define TEAR_LINUX_PLATFORM_H

#include "platform.h"

int tear_linux_platform_run_workload(const struct tear_workload_run *run,
                                     int *raw_status);

int tear_linux_platform_start_process(const char *path,
                                      char *const argv[],
                                      tear_platform_process_t *process);

int tear_linux_platform_run_process(const char *path,
                                    char *const argv[],
                                    int *exit_code);

void tear_linux_platform_stop_process(tear_platform_process_t process);

void tear_linux_platform_sleep_ms(unsigned int milliseconds);

int tear_linux_platform_socket_listen(const char *path,
                                      tear_platform_socket_t *socket_out);

int tear_linux_platform_socket_accept(tear_platform_socket_t server,
                                      tear_platform_socket_t *client_out);

ssize_t tear_linux_platform_socket_read(tear_platform_socket_t socket_fd,
                                        void *buf,
                                        size_t len);

void tear_linux_platform_socket_close(tear_platform_socket_t socket_fd);

void tear_linux_platform_socket_unlink(const char *path);

#endif
