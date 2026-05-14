// SPDX-License-Identifier: Apache-2.0

#include "runtime_manager.h"

#include "model_manifest.h"
#include "telemetry.h"
#include "trust_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct runtime_config {
    const char *workload;
    const char *manifest;
};

static struct runtime_config parse_args(int argc, char **argv)
{
    struct runtime_config cfg = {
        .workload = "/bin/tear-hello",
        .manifest = NULL,
    };

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--workload") == 0 &&
            i + 1 < argc) {
            cfg.workload = argv[++i];
        }

        else if (strcmp(argv[i], "--manifest") == 0 &&
                 i + 1 < argc) {
            cfg.manifest = argv[++i];
        }
    }

    return cfg;
}

int tear_runtime_manager_main(int argc, char **argv)
{
    struct runtime_config cfg = parse_args(argc, argv);

    tear_event("runtime_manager_start");

    if (cfg.manifest) {

        struct tear_model_manifest manifest;

        if (tear_manifest_load(cfg.manifest,
                               &manifest) < 0) {

            fprintf(stderr,
                    "TEAR: failed to load manifest\n");

            tear_event("manifest_load_failed");

            return 1;
        }

        tear_event("manifest_loaded");

        tear_manifest_print(&manifest);

        if (tear_trust_enroll(&manifest) < 0) {
            fprintf(stderr, "TEAR: trust enroll failed\n");
            tear_event("trust_enroll_failed");
            return 1;
        }

        tear_event("trust_enroll_done");

        if (tear_trust_report() < 0) {
            fprintf(stderr, "TEAR: trust report failed\n");
            tear_event("trust_report_failed");
            return 1;
        }

        tear_event("trust_report_done");
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        tear_event("runtime_manager_fork_failed");
        return 1;
    }

    if (pid == 0) {

        execl(cfg.workload,
              cfg.workload,
              NULL);

        perror("execl");
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        tear_event("runtime_manager_wait_failed");
        return 1;
    }

    if (WIFEXITED(status)) {

        tear_event_kv("runtime_workload_exit",
                      "status",
                      WEXITSTATUS(status));
    }

    tear_event("runtime_manager_shutdown");

    return 0;
}
