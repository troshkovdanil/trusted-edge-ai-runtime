// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "observability.h"
#include "trust_client.h"

#include <string.h>

#define TEAR_COMPONENT "tearictl"

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
             "usage: %s enroll <manifest> | %s verify <manifest> | "
             "%s update-model <manifest> | %s report | %s report-decision",
             prog,
             prog,
             prog,
             prog,
             prog);
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

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "enroll") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }

        return cmd_enroll(argv[2]);
    }

    if (strcmp(argv[1], "verify") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }

        return cmd_verify(argv[2]);
    }

    if (strcmp(argv[1], "report") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }

        return cmd_report();
    }

    if (strcmp(argv[1], "report-decision") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }

        return cmd_report_decision();
    }

    if (strcmp(argv[1], "update-model") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 1;
        }

        return cmd_update_model(argv[2]);
    }

    usage(argv[0]);

    return 1;
}
