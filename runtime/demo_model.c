// SPDX-License-Identifier: Apache-2.0

#include "observability.h"
#include "profile.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEAR_COMPONENT "demo_model"
#define TEAR_METRICS_PATH_MAX 256
#define TEAR_EVENT_PATH_MAX 256

#ifdef TEAR_HOST_BUILD
#define DEFAULT_EVENT_PATH "build/host/tear-demo-model-events.log"
#else
#define DEFAULT_EVENT_PATH "/tmp/tear-demo-model-events.log"
#endif

static const char *parse_arg_value(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0 && i + 1 < argc)
            return argv[++i];
    }

    return NULL;
}

static int build_run_path(const char *base,
                          const char *run_id,
                          char *path,
                          size_t path_size)
{
    int n = snprintf(path,
                     path_size,
                     "%s-%s",
                     base,
                     run_id);

    return n >= 0 && (size_t)n < path_size ? 0 : -1;
}

static int build_metrics_path(const struct tear_profile *profile,
                              const char *run_id,
                              char *path,
                              size_t path_size)
{
    return build_run_path(profile->metrics_file_template,
                          run_id,
                          path,
                          path_size);
}

int main(int argc, char **argv)
{
    const char *profile_path = parse_arg_value(argc, argv, "--profile");
    const char *run_id = parse_arg_value(argc, argv, "--run-id");
    const char *event_log = parse_arg_value(argc, argv, "--event-log");
    struct tear_profile profile;
    char metrics_path[TEAR_METRICS_PATH_MAX];
    char default_event_path[TEAR_EVENT_PATH_MAX];

    if (!profile_path) {
        fprintf(stderr, "TEAR model: missing --profile <path>\n");
        return 1;
    }

    if (!run_id) {
        fprintf(stderr, "TEAR model: missing --run-id <id>\n");
        return 1;
    }

    if (tear_profile_load(profile_path, &profile) < 0) {
        fprintf(stderr, "TEAR model: failed to load profile %s\n",
                profile_path);
        return 1;
    }

    if (build_metrics_path(&profile,
                           run_id,
                           metrics_path,
                           sizeof(metrics_path)) < 0) {
        fprintf(stderr, "TEAR model: metrics path too long\n");
        return 1;
    }

    if (!event_log) {
        if (build_run_path(DEFAULT_EVENT_PATH,
                           run_id,
                           default_event_path,
                           sizeof(default_event_path)) < 0) {
            fprintf(stderr, "TEAR model: event path too long\n");
            return 1;
        }

        event_log = default_event_path;
    }

    if (tear_event_init(event_log) < 0) {
        fprintf(stderr, "TEAR model: failed to initialize events\n");
        return 1;
    }

    if (tear_metric_init(metrics_path) < 0) {
        fprintf(stderr, "TEAR model: failed to initialize metrics\n");
        tear_event_shutdown();
        return 1;
    }

    tear_event_ex(TEAR_COMPONENT,
                  profile.profile_id,
                  profile.artifact_id,
                  "model_init");

    printf("TEAR model: loading model metadata\n");
    printf("TEAR model: profile_id=%s\n", profile.profile_id);
    printf("TEAR model: artifact_id=%s\n", profile.artifact_id);
    printf("TEAR model: backend=%s\n", profile.backend);
    printf("TEAR model: run_id=%s\n", run_id);

    sleep(1);

    tear_event_ex(TEAR_COMPONENT,
                  profile.profile_id,
                  profile.artifact_id,
                  "inference_start");

    printf("TEAR model: input=synthetic-frame\n");

    sleep(1);

    printf("TEAR model: running inference\n");

    sleep(1);

    tear_metric_long(TEAR_COMPONENT,
                     profile.profile_id,
                     profile.artifact_id,
                     "confidence_x100",
                     87);

    tear_event_ex(TEAR_COMPONENT,
                  profile.profile_id,
                  profile.artifact_id,
                  "inference_done");

    printf("TEAR model: result=object:box confidence=0.87\n");

    tear_event_ex(TEAR_COMPONENT,
                  profile.profile_id,
                  profile.artifact_id,
                  "model_shutdown");

    tear_metric_shutdown();
    tear_event_shutdown();

    return 0;
}
