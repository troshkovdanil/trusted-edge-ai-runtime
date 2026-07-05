// SPDX-License-Identifier: Apache-2.0

#include "observability.h"
#include "profile.h"

#include <onnxruntime_c_api.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEAR_COMPONENT "mnist_model"
#define MNIST_INPUT_SIZE (1 * 1 * 28 * 28)
#define MNIST_CLASSES 10
#define TEAR_METRICS_PATH_MAX 256
#define TEAR_EVENT_PATH_MAX 256

#ifdef TEAR_HOST_BUILD
#define TEAR_PLATFORM_DIR "build/platforms/host-mock"
#else
#define TEAR_PLATFORM_DIR "/tmp"
#endif
#define DEFAULT_EVENT_PATH TEAR_PLATFORM_DIR "/tear-mnist-model-events.log"

static const char *MODEL_PATH = "models/mnist/mnist.onnx";

enum mnist_sample_kind {
    SAMPLE_CLEAN7 = 0,
    SAMPLE_WEAK7 = 1,
    SAMPLE_NOISE = 2,
};

struct mnist_confidence {
    int top1_index;
    int top2_index;
    float top1_score;
    float top2_score;
    float margin;
};

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
    int n = snprintf(path, path_size, "%s-%s", base, run_id);

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

static const char *sample_name(enum mnist_sample_kind kind)
{
    switch (kind) {
    case SAMPLE_CLEAN7:
        return "clean7";
    case SAMPLE_WEAK7:
        return "weak7";
    case SAMPLE_NOISE:
        return "noise";
    }

    return "unknown";
}

static enum mnist_sample_kind parse_sample_kind(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--sample") == 0) {
            if (strcmp(argv[i + 1], "clean7") == 0)
                return SAMPLE_CLEAN7;

            if (strcmp(argv[i + 1], "weak7") == 0)
                return SAMPLE_WEAK7;

            if (strcmp(argv[i + 1], "noise") == 0)
                return SAMPLE_NOISE;
        }
    }

    const char *env_sample = getenv("TEAR_MNIST_SAMPLE");

    if (env_sample) {
        if (strcmp(env_sample, "clean7") == 0)
            return SAMPLE_CLEAN7;

        if (strcmp(env_sample, "weak7") == 0)
            return SAMPLE_WEAK7;

        if (strcmp(env_sample, "noise") == 0)
            return SAMPLE_NOISE;
    }

    return SAMPLE_CLEAN7;
}

static struct mnist_confidence mnist_get_confidence(const float *values,
                                                    size_t count)
{
    struct mnist_confidence c = {
        .top1_index = 0,
        .top2_index = 1,
        .top1_score = values[0],
        .top2_score = values[1],
        .margin = 0.0f,
    };

    if (c.top2_score > c.top1_score) {
        c.top1_index = 1;
        c.top2_index = 0;
        c.top1_score = values[1];
        c.top2_score = values[0];
    }

    for (size_t i = 2; i < count; ++i) {
        if (values[i] > c.top1_score) {
            c.top2_index = c.top1_index;
            c.top2_score = c.top1_score;
            c.top1_index = (int)i;
            c.top1_score = values[i];
        } else if (values[i] > c.top2_score) {
            c.top2_index = (int)i;
            c.top2_score = values[i];
        }
    }

    c.margin = c.top1_score - c.top2_score;
    return c;
}

static long input_density_x1000(const float input[MNIST_INPUT_SIZE])
{
    long active = 0;

    for (size_t i = 0; i < MNIST_INPUT_SIZE; ++i) {
        if (input[i] > 0.2f)
            active++;
    }

    return active * 1000 / MNIST_INPUT_SIZE;
}

static int64_t now_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return -1;

    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

static void fill_digit_sample(float input[MNIST_INPUT_SIZE],
                              enum mnist_sample_kind kind)
{
    for (size_t i = 0; i < MNIST_INPUT_SIZE; ++i)
        input[i] = 0.0f;

    if (kind == SAMPLE_NOISE) {
        for (size_t i = 0; i < MNIST_INPUT_SIZE; ++i)
            input[i] = (i % 7 == 0 || i % 19 == 0) ? 1.0f : 0.0f;

        return;
    }

    float intensity = kind == SAMPLE_WEAK7 ? 0.35f : 1.0f;

    for (int x = 6; x < 22; ++x)
        input[7 * 28 + x] = intensity;

    for (int i = 0; i < 14; ++i) {
        int y = 8 + i;
        int x = 21 - i;

        if (x >= 0 && x < 28 && y >= 0 && y < 28) {
            input[y * 28 + x] = intensity;

            if (x + 1 < 28)
                input[y * 28 + x + 1] = intensity * 0.8f;
        }
    }
}

static char pixel(float v)
{
    if (v > 0.8f)
        return '#';

    if (v > 0.5f)
        return 'X';

    if (v > 0.2f)
        return '.';

    return ' ';
}

static void print_digit(const float input[MNIST_INPUT_SIZE])
{
    char row[29];

    tear_log(TEAR_COMPONENT, TEAR_LOG_DEBUG, "input image:");

    for (int y = 0; y < 28; y++) {
        for (int x = 0; x < 28; x++)
            row[x] = pixel(input[y * 28 + x]);

        row[28] = '\0';
        tear_log(TEAR_COMPONENT, TEAR_LOG_DEBUG, "%s", row);
    }
}

static void check_status(const OrtApi *api, OrtStatus *status,
                         const char *what)
{
    if (status == NULL)
        return;

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "ONNX Runtime error during %s: %s",
             what,
             api->GetErrorMessage(status));

    api->ReleaseStatus(status);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *profile_path = parse_arg_value(argc, argv, "--profile");
    const char *run_id = parse_arg_value(argc, argv, "--run-id");
    const char *event_log = parse_arg_value(argc, argv, "--event-log");
    struct tear_profile profile;
    char metrics_path[TEAR_METRICS_PATH_MAX];
    char default_event_path[TEAR_EVENT_PATH_MAX];
    enum mnist_sample_kind sample_kind = parse_sample_kind(argc, argv);
    const OrtApi *api = OrtGetApiBase()->GetApi(ORT_API_VERSION);

    OrtEnv *env = NULL;
    OrtSessionOptions *session_options = NULL;
    OrtSession *session = NULL;
    OrtMemoryInfo *memory_info = NULL;
    OrtValue *input_tensor = NULL;
    OrtValue *output_tensor = NULL;

    float input[MNIST_INPUT_SIZE];
    int64_t input_shape[] = {1, 1, 28, 28};

    const char *input_names[] = {"Input3"};
    const char *output_names[] = {"Plus214_Output_0"};

    if (!profile_path) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST missing --profile <path>");
        return 1;
    }

    if (!run_id) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST missing --run-id <id>");
        return 1;
    }

    if (tear_profile_load(profile_path, &profile) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST failed to load profile %s",
                 profile_path);
        return 1;
    }

    if (build_metrics_path(&profile,
                           run_id,
                           metrics_path,
                           sizeof(metrics_path)) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST metrics path too long");
        return 1;
    }

    if (!event_log) {
        if (build_run_path(DEFAULT_EVENT_PATH,
                           run_id,
                           default_event_path,
                           sizeof(default_event_path)) < 0) {
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "TEAR: MNIST event path too long");
            return 1;
        }

        event_log = default_event_path;
    }

    if (tear_event_init(event_log) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST failed to initialize events");
        return 1;
    }

    if (tear_metric_init(metrics_path) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "TEAR: MNIST failed to initialize metrics");
        tear_event_shutdown();
        return 1;
    }

    fill_digit_sample(input, sample_kind);
    print_digit(input);

    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "TEAR: MNIST workload start");
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "TEAR: profile_id=%s artifact_id=%s backend=%s sample=%s",
             profile.profile_id,
             profile.artifact_id,
             profile.backend,
             sample_name(sample_kind));
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "TEAR: run_id=%s", run_id);

    check_status(api, api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                     "tear-mnist", &env),
                 "CreateEnv");

    check_status(api, api->CreateSessionOptions(&session_options),
                 "CreateSessionOptions");

    check_status(api, api->SetIntraOpNumThreads(session_options, 1),
                 "SetIntraOpNumThreads");

    check_status(api, api->CreateSession(env, MODEL_PATH,
                                         session_options, &session),
                 "CreateSession");

    check_status(api,
                 api->CreateCpuMemoryInfo(OrtArenaAllocator,
                                          OrtMemTypeDefault,
                                          &memory_info),
                 "CreateCpuMemoryInfo");

    check_status(api,
                 api->CreateTensorWithDataAsOrtValue(
                     memory_info,
                     input,
                     sizeof(input),
                     input_shape,
                     4,
                     ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                     &input_tensor),
                 "CreateTensorWithDataAsOrtValue");

    int64_t start_us = now_us();

    check_status(api,
                 api->Run(session,
                          NULL,
                          input_names,
                          (const OrtValue *const *)&input_tensor,
                          1,
                          output_names,
                          1,
                          &output_tensor),
                 "Run");

    int64_t end_us = now_us();

    float *output = NULL;

    check_status(api,
                 api->GetTensorMutableData(output_tensor, (void **)&output),
                 "GetTensorMutableData");

    struct mnist_confidence confidence =
        mnist_get_confidence(output, MNIST_CLASSES);

    int predicted_digit = confidence.top1_index;
    int64_t latency_us = end_us - start_us;
    long density_x1000 = input_density_x1000(input);

    long top1_score_x1000 = (long)(confidence.top1_score * 1000.0f);
    long top2_score_x1000 = (long)(confidence.top2_score * 1000.0f);
    long confidence_margin_x1000 = (long)(confidence.margin * 1000.0f);

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "TEAR: metric predicted_digit=%d",
             predicted_digit);
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "TEAR: metric top1_score=%.6f top2_score=%.6f confidence_margin=%.6f",
             confidence.top1_score,
             confidence.top2_score,
             confidence.margin);
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "TEAR: metric input_density_x1000=%ld",
             density_x1000);
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "TEAR: predicted_digit=%d latency_us=%lld",
             predicted_digit,
             (long long)latency_us);

    tear_event_profile(TEAR_COMPONENT,
                       &profile,
                       "mnist_inference_metrics");

    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "sample_kind",
                     sample_kind);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "predicted_digit",
                     predicted_digit);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "top1_score_x1000",
                     top1_score_x1000);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "top2_score_x1000",
                     top2_score_x1000);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "confidence_margin_x1000",
                     confidence_margin_x1000);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "input_density_x1000",
                     density_x1000);
    tear_metric_long(TEAR_COMPONENT,
                     &profile,
                     "latency_us",
                     (long)latency_us);

    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "TEAR: MNIST workload finished");

    api->ReleaseValue(output_tensor);
    api->ReleaseValue(input_tensor);
    api->ReleaseMemoryInfo(memory_info);
    api->ReleaseSession(session);
    api->ReleaseSessionOptions(session_options);
    api->ReleaseEnv(env);

    tear_metric_shutdown();
    tear_event_shutdown();

    return 0;
}
