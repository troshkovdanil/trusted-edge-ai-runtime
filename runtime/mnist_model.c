// SPDX-License-Identifier: Apache-2.0

#include "observability.h"

#include <onnxruntime_c_api.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MNIST_INPUT_SIZE (1 * 1 * 28 * 28)
#define MNIST_CLASSES 10

static const char *MODEL_PATH = "models/mnist/mnist.onnx";
static const char *MODEL_ID = "mnist-onnx-v1";
static const char *BACKEND = "onnxruntime-cpu";

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
    if (argc >= 3 && strcmp(argv[1], "--sample") == 0) {
        if (strcmp(argv[2], "clean7") == 0)
            return SAMPLE_CLEAN7;

        if (strcmp(argv[2], "weak7") == 0)
            return SAMPLE_WEAK7;

        if (strcmp(argv[2], "noise") == 0)
            return SAMPLE_NOISE;
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
    printf("\n");

    for (int y = 0; y < 28; y++) {
        for (int x = 0; x < 28; x++)
            putchar(pixel(input[y * 28 + x]));

        putchar('\n');
    }

    printf("\n");
}

static void check_status(const OrtApi *api, OrtStatus *status,
                         const char *what)
{
    if (status == NULL)
        return;

    fprintf(stderr, "ONNX Runtime error during %s: %s\n",
            what, api->GetErrorMessage(status));

    api->ReleaseStatus(status);
    exit(1);
}

int main(int argc, char **argv)
{
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

    fill_digit_sample(input, sample_kind);
    print_digit(input);

    printf("TEAR: MNIST workload start\n");
    printf("TEAR: artifact_id=%s backend=%s sample=%s\n",
           MODEL_ID, BACKEND, sample_name(sample_kind));

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

    printf("TEAR: metric predicted_digit=%d\n", predicted_digit);
    printf("TEAR: metric top1_score=%.6f top2_score=%.6f "
           "confidence_margin=%.6f\n",
           confidence.top1_score,
           confidence.top2_score,
           confidence.margin);
    printf("TEAR: metric input_density_x1000=%ld\n", density_x1000);
    printf("TEAR: predicted_digit=%d latency_us=%lld\n",
           predicted_digit, (long long)latency_us);

    tear_event("mnist_inference_metrics");
    tear_event_kv("mnist", "sample_kind", sample_kind);
    tear_event_kv("mnist", "predicted_digit", predicted_digit);
    tear_event_kv("mnist", "top1_score_x1000", top1_score_x1000);
    tear_event_kv("mnist", "top2_score_x1000", top2_score_x1000);
    tear_event_kv("mnist", "confidence_margin_x1000",
                  confidence_margin_x1000);
    tear_event_kv("mnist", "input_density_x1000", density_x1000);
    tear_event_kv("mnist", "latency_us", (long)latency_us);

    printf("TEAR: MNIST workload finished\n");

    api->ReleaseValue(output_tensor);
    api->ReleaseValue(input_tensor);
    api->ReleaseMemoryInfo(memory_info);
    api->ReleaseSession(session);
    api->ReleaseSessionOptions(session_options);
    api->ReleaseEnv(env);

    return 0;
}
