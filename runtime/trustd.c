// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "telemetry.h"
#include "trusted_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TEAR_TRUSTD_SOCKET "/tmp/tear-trustd.sock"
#define TEAR_TRUSTED_STATE "/tmp/tear-trusted-state"

static int create_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path,
            TEAR_TRUSTD_SOCKET,
            sizeof(addr.sun_path) - 1);

    unlink(TEAR_TRUSTD_SOCKET);

    if (bind(fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {

        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int main(void)
{
    int server = create_socket();

    if (server < 0) {
        perror("trustd socket");
        return 1;
    }

    tear_event("trustd_start");

    while (1) {

        int client = accept(server, NULL, NULL);

        if (client < 0)
            continue;

        char buf[512];

        ssize_t n = read(client,
                         buf,
                         sizeof(buf) - 1);

        if (n <= 0) {
            close(client);
            continue;
        }

        buf[n] = '\0';

        if (strncmp(buf, "ENROLL", 6) == 0) {

            struct tear_model_manifest m;

            memset(&m, 0, sizeof(m));

            sscanf(buf,
                   "ENROLL %63s %d %31s %127s",
                   m.model_id,
                   &m.version,
                   m.backend,
                   m.model_hash);

            if (tear_trusted_state_store(TEAR_TRUSTED_STATE, &m) == 0) {
                tear_event("model_enroll");
                dprintf(client, "OK\n");
            } else {
                tear_event("model_enroll_failed");
                dprintf(client, "ERR\n");
            }

            tear_trusted_state_store(
                TEAR_TRUSTED_STATE,
                &m);
        }

        else if (strncmp(buf, "REPORT", 6) == 0) {

            struct tear_model_manifest m;

            if (tear_trusted_state_load(
                    TEAR_TRUSTED_STATE,
                    &m) == 0) {

                dprintf(client,
                        "STATE %s %d %s %s\n",
                        m.model_id,
                        m.version,
                        m.backend,
                        m.model_hash);
            }
        }

        close(client);
    }

    return 0;
}
