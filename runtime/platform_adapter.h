// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_PLATFORM_ADAPTER_H
#define TEAR_PLATFORM_ADAPTER_H

#include "profile.h"

#define TEAR_PLATFORM_KIND_MAX 32
#define TEAR_PLATFORM_PROFILE_MAX 64

struct tear_platform_context {
    char kind[TEAR_PLATFORM_KIND_MAX];
    char hardware_profile[TEAR_PLATFORM_PROFILE_MAX];
    int optee_present;
    int supports_mock;
    int supports_onnxruntime_cpu;
};

int tear_platform_detect(struct tear_platform_context *ctx);

int tear_platform_check_profile(const struct tear_platform_context *ctx,
                                const struct tear_profile *profile);

#endif
