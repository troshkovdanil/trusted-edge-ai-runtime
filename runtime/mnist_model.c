// SPDX-License-Identifier: Apache-2.0

#include <onnxruntime_c_api.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MNIST_INPUT_SIZE (1 * 1 * 28 * 28)
#define MNIST_CLASSES 10

static const char *MODEL_PATH = "models/mnist/mnist.onnx";
static const char *MODEL_ID = "mnist-onnx-v1";
static const char *BACKEND = "onnxruntime-cpu";

static int64_t now_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return -1;
    }

    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

static void fill_simple_digit_sample(float input[MNIST_INPUT_SIZE])
{
    for (size_t i = 0; i < MNIST_INPUT_SIZE; ++i) {
        input[i] = 0.0f;
    }

    /*
     * Minimal built-in 28x28 "7-like" sample.
     * White foreground on black background, normalized to [0.0, 1.0].
     */
    for (int x = 6; x < 22; ++x) {
        input[7 * 28 + x] = 1.0f;
    }

    for (int i = 0; i < 14; ++i) {
        int y = 8 + i;
        int x = 21 - i;
        if (x >= 0 && x < 28 && y >= 0 && y < 28) {
            input[y * 28 + x] = 1.0f;
            if (x + 1 < 28) {
                input[y * 28 + x + 1] = 0.8f;
            }
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

static void print_digit(const float input[28 * 28])
{
    printf("\n");

    for (int y = 0; y < 28; y++) {
        for (int x = 0; x < 28; x++) {
            float v = input[y * 28 + x];
	    putchar(pixel(v));
        }

        putchar('\n');
    }

    printf("\n");
}

static void check_status(const OrtApi *api, OrtStatus *status, const char *what)
{
    if (status == NULL) {
        return;
    }

    fprintf(stderr, "ONNX Runtime error during %s: %s\n",
            what, api->GetErrorMessage(status));
    api->ReleaseStatus(status);
    exit(1);
}

static int argmax(const float *values, size_t count)
{
    int best_index = 0;
    float best_value = values[0];

    for (size_t i = 1; i < count; ++i) {
        if (values[i] > best_value) {
            best_value = values[i];
            best_index = (int)i;
        }
    }

    return best_index;
}

int main(void)
{
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

    fill_simple_digit_sample(input);
    print_digit(input);

    printf("TEAR: MNIST workload start\n");
    printf("TEAR: model_id=%s backend=%s\n", MODEL_ID, BACKEND);

    check_status(api, api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "tear-mnist", &env),
                 "CreateEnv");

    check_status(api, api->CreateSessionOptions(&session_options),
                 "CreateSessionOptions");

    check_status(api, api->SetIntraOpNumThreads(session_options, 1),
                 "SetIntraOpNumThreads");

    check_status(api, api->CreateSession(env, MODEL_PATH, session_options, &session),
                 "CreateSession");

    check_status(api,
                 api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
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

    int predicted_digit = argmax(output, MNIST_CLASSES);
    int64_t latency_us = end_us - start_us;

    printf("TEAR: predicted_digit=%d latency_us=%lld\n",
           predicted_digit, (long long)latency_us);
    printf("TEAR: MNIST workload finished\n");

    api->ReleaseValue(output_tensor);
    api->ReleaseValue(input_tensor);
    api->ReleaseMemoryInfo(memory_info);
    api->ReleaseSession(session);
    api->ReleaseSessionOptions(session_options);
    api->ReleaseEnv(env);

    return 0;
}
