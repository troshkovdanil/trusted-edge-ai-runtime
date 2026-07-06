// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "observability.h"
#include "platform.h"
#include "runtime_paths.h"
#include "trust_client.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEAR_COMPONENT "tearictl"

#ifdef TEAR_HOST_BUILD
#define TEAR_PLATFORM_DIR "build/platforms/host-mock"
#else
#define TEAR_PLATFORM_DIR "/tmp"
#endif
#define DEFAULT_EVENT_PATH TEAR_PLATFORM_DIR "/tearictl-events.log"

static void tearictl_event(const char *event)
{
    tear_event(TEAR_COMPONENT, event);
}

static void tearictl_manifest_event(const struct tear_model_manifest *manifest,
                                    const char *event)
{
    tear_event_manifest(TEAR_COMPONENT, manifest, event);
}

static void usage(const char *prog)
{
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "usage: %s enroll <manifest> | "
             "%s verify <manifest> | "
             "%s update-model <manifest> | "
             "%s report | "
             "%s report-decision | "
             "%s status | "
             "%s provision <manifest> | "
             "%s provision-plan <plan> | "
             "%s run <workload> <manifest> <profile> [-- <args>] | "
             "%s run-plan <plan>",
             prog,
             prog,
             prog,
             prog,
             prog,
             prog,
             prog,
             prog,
             prog,
             prog);
}

static int supervisor_command(const char *fmt, ...)
{
    char command[512];
    char reply[512];
    va_list ap;
    ssize_t n;
    tear_platform_socket_t fd;

    va_start(ap, fmt);
    vsnprintf(command, sizeof(command), fmt, ap);
    va_end(ap);

    if (tear_platform_socket_connect(tear_supervisor_socket_path(), &fd) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to connect supervisor socket %s: %s",
                 tear_supervisor_socket_path(),
                 strerror(errno));
        return 1;
    }

    dprintf(fd, "%s\n", command);

    n = tear_platform_socket_read(fd, reply, sizeof(reply) - 1);
    tear_platform_socket_close(fd);

    if (n <= 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to read supervisor reply");
        return 1;
    }

    reply[n] = '\0';
    reply[strcspn(reply, "\n")] = '\0';

    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "%s", reply);

    return strncmp(reply, "OK", 2) == 0 ||
           strncmp(reply, "PONG", 4) == 0 ||
           strncmp(reply, "STATUS", 6) == 0 ? 0 : 1;
}

static int cmd_enroll(const char *path)
{
    struct tear_model_manifest manifest;

    if (tear_manifest_load(path, &manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to load manifest: %s",
                 path);
        return 1;
    }

    if (tear_trust_enroll(&manifest) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR, "enroll failed");
        tearictl_manifest_event(&manifest, "tearictl_enroll_failed");
        return 1;
    }

    tearictl_manifest_event(&manifest, "tearictl_enroll_done");

    return 0;
}

static int cmd_verify(const char *path)
{
    struct tear_model_manifest manifest;

    if (tear_manifest_load(path, &manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to load manifest: %s",
                 path);
        return 1;
    }

    if (tear_trust_verify(&manifest) < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR, "verify failed");
        tearictl_manifest_event(&manifest, "tearictl_verify_failed");
        return 1;
    }

    tearictl_manifest_event(&manifest, "tearictl_verify_done");

    return 0;
}

static int cmd_report(void)
{
    if (tear_trust_report() < 0) {
        tear_log(TEAR_COMPONENT, TEAR_LOG_ERROR, "report failed");
        tearictl_event("tearictl_report_failed");
        return 1;
    }

    tearictl_event("tearictl_report_done");

    return 0;
}

static int cmd_report_decision(void)
{
    char decision[512];

    if (tear_trust_report_decision(decision, sizeof(decision)) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "report decision failed");
        tearictl_event("tearictl_report_decision_failed");
        return 1;
    }

    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "DECISION %s", decision);
    tearictl_event("tearictl_report_decision_done");

    return 0;
}

static int cmd_update_model(const char *path)
{
    struct tear_model_manifest manifest;

    if (tear_manifest_load(path, &manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to load manifest: %s",
                 path);
        return 1;
    }

    if (tear_trust_update_model(&manifest) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "model update rejected");
        tearictl_manifest_event(&manifest, "tearictl_update_model_failed");
        return 1;
    }

    tearictl_manifest_event(&manifest, "tearictl_update_model_done");

    return 0;
}

static int cmd_status(void)
{
    return supervisor_command("STATUS");
}

static int cmd_provision(const char *path)
{
    int ret = supervisor_command("PROVISION %s", path);

    tearictl_event(ret == 0 ? "tearictl_provision_done" :
                            "tearictl_provision_failed");

    return ret;
}

static int cmd_provision_plan(const char *path)
{
    int ret = supervisor_command("PROVISION_PLAN %s", path);

    tearictl_event(ret == 0 ? "tearictl_provision_plan_done" :
                            "tearictl_provision_plan_failed");

    return ret;
}

static int cmd_run_plan(const char *path)
{
    int ret = supervisor_command("RUN_PLAN %s", path);

    tearictl_event(ret == 0 ? "tearictl_run_plan_done" :
                            "tearictl_run_plan_failed");

    return ret;
}

static int cmd_run(int argc, char **argv)
{
    char command[512];
    size_t pos = 0;
    int ret;

    if (argc < 5)
        return 1;

    ret = snprintf(command,
                   sizeof(command),
                   "RUN %s %s %s",
                   argv[2],
                   argv[3],
                   argv[4]);

    if (ret < 0 || (size_t)ret >= sizeof(command))
        return 1;

    pos = (size_t)ret;

    for (int i = 5; i < argc; i++) {
        ret = snprintf(command + pos,
                       sizeof(command) - pos,
                       " %s",
                       argv[i]);

        if (ret < 0 || (size_t)ret >= sizeof(command) - pos)
            return 1;

        pos += (size_t)ret;
    }

    ret = supervisor_command("%s", command);

    tearictl_event(ret == 0 ? "tearictl_run_done" :
                            "tearictl_run_failed");

    return ret;
}

int main(int argc, char **argv)
{
    int ret = 1;

    if (tear_event_init(DEFAULT_EVENT_PATH) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize tearictl events");
        return 1;
    }

    if (argc < 2) {
        usage(argv[0]);
        goto out;
    }

    if (strcmp(argv[1], "enroll") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_enroll(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "verify") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_verify(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "report") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_report();
        goto out;
    }

    if (strcmp(argv[1], "report-decision") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_report_decision();
        goto out;
    }

    if (strcmp(argv[1], "update-model") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_update_model(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "status") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_status();
        goto out;
    }

    if (strcmp(argv[1], "provision") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_provision(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "provision-plan") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_provision_plan(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "run-plan") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            goto out;
        }

        ret = cmd_run_plan(argv[2]);
        goto out;
    }

    if (strcmp(argv[1], "run") == 0) {
        ret = cmd_run(argc, argv);
        goto out;
    }

    usage(argv[0]);

out:
    tear_event_shutdown();
    return ret;
}
