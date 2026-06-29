// SPDX-License-Identifier: Apache-2.0

#include "platform_adapter.h"

#include "observability.h"
#include "platform_contract.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEAR_COMPONENT "platform_adapter"

static void set_string(char *dst, size_t dst_size, const char *src)
{
    snprintf(dst, dst_size, "%s", src);
}

static int optee_device_present(void)
{
    return access("/dev/tee0", F_OK) == 0 ||
           access("/dev/teepriv0", F_OK) == 0;
}

static void emit_platform_event(const struct tear_platform_context *ctx,
                                const char *event)
{
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_INFO,
             "platform kind=%s hardware_profile=%s optee_present=%d "
             "supports_mock=%d supports_onnxruntime_cpu=%d event=%s",
             ctx->kind,
             ctx->hardware_profile,
             ctx->optee_present,
             ctx->supports_mock,
             ctx->supports_onnxruntime_cpu,
             event);

    tear_event(TEAR_COMPONENT, event);
}

int tear_platform_detect(struct tear_platform_context *ctx)
{
    if (!ctx)
        return -1;

    memset(ctx, 0, sizeof(*ctx));

    ctx->optee_present = optee_device_present();

#ifdef TEAR_HOST_BUILD
    set_string(ctx->kind,
               sizeof(ctx->kind),
               TEAR_PLATFORM_KIND_HOST_DEMO);
    set_string(ctx->hardware_profile,
               sizeof(ctx->hardware_profile),
               TEAR_PLATFORM_PROFILE_HOST_DEMO);
#else
    if (ctx->optee_present) {
        set_string(ctx->kind,
                   sizeof(ctx->kind),
                   TEAR_PLATFORM_KIND_OPTEE_QEMU);
        set_string(ctx->hardware_profile,
                   sizeof(ctx->hardware_profile),
                   TEAR_PLATFORM_PROFILE_OPTEE_QEMU);
    } else {
        set_string(ctx->kind,
                   sizeof(ctx->kind),
                   TEAR_PLATFORM_KIND_UNKNOWN);
        set_string(ctx->hardware_profile,
                   sizeof(ctx->hardware_profile),
                   TEAR_PLATFORM_PROFILE_UNKNOWN);
    }
#endif

    ctx->supports_mock = 1;
    ctx->supports_onnxruntime_cpu = 1;

    emit_platform_event(ctx, TEAR_PLATFORM_EVENT_DETECTED);

    if (ctx->optee_present)
        emit_platform_event(ctx, TEAR_PLATFORM_EVENT_OPTEE_PRESENT);
    else
        emit_platform_event(ctx, TEAR_PLATFORM_EVENT_OPTEE_ABSENT);

    return 0;
}

int tear_platform_check_profile(const struct tear_platform_context *ctx,
                                const struct tear_profile *profile)
{
    if (!ctx || !profile)
        return -1;

    tear_event_profile(TEAR_COMPONENT,
                       profile,
                       TEAR_PLATFORM_EVENT_PROFILE_VERIFIED);

    if (strcmp(profile->backend, TEAR_PLATFORM_BACKEND_MOCK) == 0 &&
        ctx->supports_mock) {
        tear_event_profile(TEAR_COMPONENT,
                           profile,
                           TEAR_PLATFORM_EVENT_BACKEND_AVAILABLE);
        return 0;
    }

    if (strcmp(profile->backend, TEAR_PLATFORM_BACKEND_ONNXRUNTIME_CPU) == 0 &&
        ctx->supports_onnxruntime_cpu) {
        tear_event_profile(TEAR_COMPONENT,
                           profile,
                           TEAR_PLATFORM_EVENT_BACKEND_AVAILABLE);
        return 0;
    }

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "backend %s is not available on platform %s",
             profile->backend,
             ctx->kind);

    tear_event_profile(TEAR_COMPONENT,
                       profile,
                       TEAR_PLATFORM_EVENT_BACKEND_REJECTED);

    return -1;
}
