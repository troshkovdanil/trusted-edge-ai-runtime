// SPDX-License-Identifier: Apache-2.0

#include "optimizer_policy.h"
#include "observability.h"
#include "runtime_paths.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int create_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    const char *socket_path = tear_optd_socket_path();

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

static int load_metrics(const char *path, struct tear_inference_metrics *metrics)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int have_margin = 0;
    int have_density = 0;

    if (!f)
        return -1;

    metrics->confidence_margin_x1000 = 0;
    metrics->input_density_x1000 = 0;

    while (fgets(line, sizeof(line), f)) {
        long value;

        if (sscanf(line,
                   "TEAR_EVENT %*s event=mnist confidence_margin_x1000=%ld",
                   &value) == 1) {
            metrics->confidence_margin_x1000 = value;
            have_margin = 1;
            continue;
        }

        if (sscanf(line,
                   "TEAR_EVENT %*s event=mnist input_density_x1000=%ld",
                   &value) == 1) {
            metrics->input_density_x1000 = value;
            have_density = 1;
            continue;
        }
    }

    fclose(f);

    return have_margin && have_density ? 0 : -1;
}

static void handle_propose(int client, const char *buf)
{
    char path[256];
    struct tear_inference_metrics metrics;
    struct tear_optimizer_proposal proposal;

    if (sscanf(buf, "PROPOSE %255s", path) != 1) {
        tear_event("optd_no_proposal");
        dprintf(client, "NO_PROPOSAL metrics_unavailable\n");
        return;
    }

    if (load_metrics(path, &metrics) < 0) {
        tear_event("optd_no_proposal");
        dprintf(client, "NO_PROPOSAL metrics_unavailable\n");
        return;
    }

    tear_optimizer_propose(&metrics, &proposal);

    tear_event("optd_proposal");
    dprintf(client, "PROPOSAL %s %s\n", proposal.action, proposal.reason);
}

int main(void)
{
    int server = create_socket();

    if (server < 0) {
        perror("optd socket");
        return 1;
    }

    tear_event("optd_start");

    while (1) {
        int client = accept(server, NULL, NULL);

        if (client < 0)
            continue;

        char buf[512];
        ssize_t n = read(client, buf, sizeof(buf) - 1);

        if (n <= 0) {
            close(client);
            continue;
        }

        buf[n] = '\0';

        if (strncmp(buf, "PROPOSE", 7) == 0)
            handle_propose(client, buf);
        else
            dprintf(client, "NO_PROPOSAL metrics_unavailable\n");

        close(client);
    }

    return 0;
}
