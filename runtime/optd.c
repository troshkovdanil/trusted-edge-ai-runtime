// SPDX-License-Identifier: Apache-2.0

#include "optimizer_policy.h"
#include "observability.h"
#include "platform.h"
#include "runtime_paths.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef TEAR_HOST_BUILD
#define TEAR_PLATFORM_DIR "build/platforms/host-mock"
#else
#define TEAR_PLATFORM_DIR "/tmp"
#endif
#define DEFAULT_EVENT_PATH TEAR_PLATFORM_DIR "/tear-optd-events.log"

#define TEAR_COMPONENT "optd"

static void optd_event(const char *event)
{
    tear_event(TEAR_COMPONENT, event);
}

static void client_reply(int client, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vdprintf(client, fmt, ap);
    va_end(ap);
}

static void client_reply_no_proposal(int client)
{
    client_reply(client, "NO_PROPOSAL metrics_unavailable\n");
}

static void client_reply_proposal(int client,
                                  const struct tear_optimizer_proposal *p)
{
    client_reply(client, "PROPOSAL %s %s\n", p->action, p->reason);
}

static int create_socket(tear_platform_socket_t *server)
{
    return tear_platform_socket_listen(tear_optd_socket_path(), server);
}

static const char *parse_event_log(int argc, char **argv)
{
    const char *event_log = DEFAULT_EVENT_PATH;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--event-log") == 0 && i + 1 < argc) {
            event_log = argv[++i];
        } else {
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "usage: tear-optd [--event-log <path>]");
            return NULL;
        }
    }

    if (!event_log || event_log[0] == '\0') {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "missing --event-log <path>");
        return NULL;
    }

    return event_log;
}

static int parse_metric_line(const char *line,
                             const char *metric_name,
                             long *value)
{
    const char *record = strstr(line, "TEAR_METRIC");
    const char *name = strstr(line, "name=");
    const char *metric_value = strstr(line, "value=");

    if (!record || !name || !metric_value)
        return -1;

    if (strncmp(name + strlen("name="),
                metric_name,
                strlen(metric_name)) != 0)
        return -1;

    if (sscanf(metric_value, "value=%ld", value) != 1)
        return -1;

    return 0;
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

        if (parse_metric_line(line,
                              "confidence_margin_x1000",
                              &value) == 0) {
            metrics->confidence_margin_x1000 = value;
            have_margin = 1;
            continue;
        }

        if (parse_metric_line(line,
                              "input_density_x1000",
                              &value) == 0) {
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
        optd_event("optd_no_proposal");
        client_reply_no_proposal(client);
        return;
    }

    if (load_metrics(path, &metrics) < 0) {
        optd_event("optd_no_proposal");
        client_reply_no_proposal(client);
        return;
    }

    tear_optimizer_propose(&metrics, &proposal);

    optd_event("optd_proposal");
    client_reply_proposal(client, &proposal);
}

int main(int argc, char **argv)
{
    const char *event_log = parse_event_log(argc, argv);
    tear_platform_socket_t server;

    if (!event_log)
        return 1;

    if (tear_event_init(event_log) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize events");
        return 1;
    }

    if (create_socket(&server) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "optd socket failed: %s",
                 strerror(errno));
        tear_event_shutdown();
        return 1;
    }

    optd_event("optd_start");

    while (1) {
        tear_platform_socket_t client;
        char buf[512];
        ssize_t n;

        if (tear_platform_socket_accept(server, &client) < 0)
            continue;

        n = tear_platform_socket_read(client, buf, sizeof(buf) - 1);

        if (n <= 0) {
            tear_platform_socket_close(client);
            continue;
        }

        buf[n] = '\0';

        if (strncmp(buf, "PROPOSE", 7) == 0)
            handle_propose(client, buf);
        else
            client_reply_no_proposal(client);

        tear_platform_socket_close(client);
    }

    return 0;
}
