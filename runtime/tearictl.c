// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "telemetry.h"
#include "trust_client.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage:\n"
            "  %s enroll <manifest>\n"
            "  %s report\n",
            prog,
            prog);
}

static int cmd_enroll(const char *path)
{
    struct tear_model_manifest manifest;

    if (tear_manifest_load(path, &manifest) < 0) {
        fprintf(stderr, "TEAR: failed to load manifest: %s\n", path);
        return 1;
    }

    if (tear_trust_enroll(&manifest) < 0) {
        fprintf(stderr, "TEAR: enroll failed\n");
        tear_event("tearictl_enroll_failed");
        return 1;
    }

    tear_event("tearictl_enroll_done");

    return 0;
}

static int cmd_report(void)
{
    if (tear_trust_report() < 0) {
        fprintf(stderr, "TEAR: report failed\n");
        tear_event("tearictl_report_failed");
        return 1;
    }

    tear_event("tearictl_report_done");

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

    if (strcmp(argv[1], "report") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 1;
        }

        return cmd_report();
    }

    usage(argv[0]);

    return 1;
}
