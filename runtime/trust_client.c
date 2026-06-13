// SPDX-License-Identifier: Apache-2.0

#include "trust_client.h"
#include "runtime_paths.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int connect_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path,
            tear_trustd_socket_path(),
            sizeof(addr.sun_path) - 1);

    if (connect(fd,
                (struct sockaddr *)&addr,
                sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int expect_ok(int fd)
{
    char buf[32];

    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n <= 0)
        return -1;

    buf[n] = '\0';

    return strncmp(buf, "OK", 2) == 0 ? 0 : -1;
}

int tear_trust_enroll(
    const struct tear_model_manifest *manifest)
{
    int fd = connect_socket();

    if (fd < 0)
        return -1;

    dprintf(fd,
            "ENROLL %s %d %s %s\n",
            manifest->artifact_id,
            manifest->version,
            manifest->backend,
            manifest->model_hash);

    int ret = expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_update_model(const struct tear_model_manifest *manifest)
{
        int fd = connect_socket();
        if (fd < 0)
                return -1;

        dprintf(fd, "UPDATE %s %d %s %s\n",
                manifest->artifact_id,
                manifest->version,
                manifest->backend,
                manifest->model_hash);

        int ret = expect_ok(fd);
        close(fd);
        return ret;
}

int tear_trust_verify(
    const struct tear_model_manifest *manifest)
{
    int fd = connect_socket();

    if (fd < 0)
        return -1;

    dprintf(fd,
            "VERIFY %s %d %s %s\n",
            manifest->artifact_id,
            manifest->version,
            manifest->backend,
            manifest->model_hash);

    int ret = expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_report(void)
{
    int fd = connect_socket();

    if (fd < 0)
        return -1;

    dprintf(fd, "REPORT\n");

    char buf[256];

    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n <= 0) {
        close(fd);
        return -1;
    }

    buf[n] = '\0';
    printf("%s", buf);

    close(fd);

    return 0;
}

int tear_trust_record_decision(const char *artifact_id,
                               const char *proposal,
                               const char *decision,
                               const char *reason,
                               long value)
{
    int fd = connect_socket();

    if (fd < 0)
        return -1;

    dprintf(fd,
            "RECORD_DECISION %s %s %s %s %ld\n",
            artifact_id,
            proposal,
            decision,
            reason,
            value);

    int ret = expect_ok(fd);

    close(fd);

    return ret;
}

int tear_trust_report_decision(char *decision, size_t decision_size)
{
    int fd = connect_socket();
    char buf[512];
    char reported[512];
    ssize_t n;

    if (!decision || decision_size == 0)
        return -1;

    if (fd < 0)
        return -1;

    dprintf(fd, "REPORT_DECISION\n");

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0)
        return -1;

    buf[n] = '\0';

    if (sscanf(buf, "DECISION %511[^\n]", reported) != 1)
        return -1;

    snprintf(decision, decision_size, "%s", reported);
    return 0;
}
