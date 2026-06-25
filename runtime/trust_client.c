// SPDX-License-Identifier: Apache-2.0

#include "trust_client.h"

#include "observability.h"
#include "runtime_paths.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TEAR_COMPONENT "trust_client"

static int connect_socket(void)
{
    const char *socket_path = tear_trustd_socket_path();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to create trustd socket: %s",
                 strerror(errno));
        return -1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to connect trustd socket %s: %s",
                 socket_path,
                 strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static void trust_client_send(int fd, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vdprintf(fd, fmt, ap);
    va_end(ap);
}

static int trust_client_expect_ok(int fd)
{
    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n <= 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to read trustd reply");
        return -1;
    }

    buf[n] = '\0';

    if (strncmp(buf, "OK", 2) != 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "trustd rejected request: %s",
                 buf);
        return -1;
    }

    return 0;
}

static int trust_client_read(int fd, char *buf, size_t buf_size)
{
    ssize_t n;

    if (!buf || buf_size == 0)
        return -1;

    n = read(fd, buf, buf_size - 1);

    if (n <= 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to read trustd response");
        return -1;
    }

    buf[n] = '\0';

    return 0;
}

int tear_trust_enroll(const struct tear_model_manifest *manifest)
{
    int fd = connect_socket();
    int ret;

    if (fd < 0)
        return -1;

    trust_client_send(fd,
                      "ENROLL %s %d %s %s\n",
                      manifest->artifact_id,
                      manifest->version,
                      manifest->backend,
                      manifest->model_hash);

    ret = trust_client_expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_update_model(const struct tear_model_manifest *manifest)
{
    int fd = connect_socket();
    int ret;

    if (fd < 0)
        return -1;

    trust_client_send(fd,
                      "UPDATE %s %d %s %s\n",
                      manifest->artifact_id,
                      manifest->version,
                      manifest->backend,
                      manifest->model_hash);

    ret = trust_client_expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_verify(const struct tear_model_manifest *manifest)
{
    int fd = connect_socket();
    int ret;

    if (fd < 0)
        return -1;

    trust_client_send(fd,
                      "VERIFY %s %d %s %s\n",
                      manifest->artifact_id,
                      manifest->version,
                      manifest->backend,
                      manifest->model_hash);

    ret = trust_client_expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_report(void)
{
    int fd = connect_socket();
    char buf[256];

    if (fd < 0)
        return -1;

    trust_client_send(fd, "REPORT\n");

    if (trust_client_read(fd, buf, sizeof(buf)) < 0) {
        close(fd);
        return -1;
    }

    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "%s", buf);

    close(fd);

    return 0;
}

int tear_trust_record_decision(const char *run_id,
                               const char *artifact_id,
                               const char *proposal,
                               const char *decision,
                               const char *reason,
                               long value)
{
    int fd = connect_socket();
    int ret;

    if (fd < 0)
        return -1;

    trust_client_send(fd,
                      "RECORD_DECISION %s %s %s %s %s %ld\n",
                      run_id,
                      artifact_id,
                      proposal,
                      decision,
                      reason,
                      value);

    ret = trust_client_expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_report_decision(char *decision, size_t decision_size)
{
    int fd = connect_socket();
    char buf[512];
    char reported[512];

    if (!decision || decision_size == 0)
        return -1;

    if (fd < 0)
        return -1;

    trust_client_send(fd, "REPORT_DECISION\n");

    if (trust_client_read(fd, buf, sizeof(buf)) < 0) {
        close(fd);
        return -1;
    }

    close(fd);

    if (sscanf(buf, "DECISION %511[^\n]", reported) != 1) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "invalid trustd decision response: %s",
                 buf);
        return -1;
    }

    if (snprintf(decision, decision_size, "%s", reported) >=
        (int)decision_size) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "reported decision is too long");
        return -1;
    }

    return 0;
}
