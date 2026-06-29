// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_PLATFORM_CONTRACT_H
#define TEAR_PLATFORM_CONTRACT_H

#define TEAR_PLATFORM_KIND_HOST_DEMO  "host-demo"
#define TEAR_PLATFORM_KIND_OPTEE_QEMU "optee-qemu"
#define TEAR_PLATFORM_KIND_UNKNOWN    "unknown"

#define TEAR_PLATFORM_BACKEND_MOCK            "mock"
#define TEAR_PLATFORM_BACKEND_ONNXRUNTIME_CPU "onnxruntime-cpu"

#define TEAR_PLATFORM_PROFILE_HOST_DEMO  "profile-host-demo.platform"
#define TEAR_PLATFORM_PROFILE_OPTEE_QEMU "profile-optee-qemu.platform"
#define TEAR_PLATFORM_PROFILE_UNKNOWN    "profile-unknown.platform"

#define TEAR_PLATFORM_EVENT_DETECTED          "platform_detected"
#define TEAR_PLATFORM_EVENT_OPTEE_PRESENT     "platform_optee_present"
#define TEAR_PLATFORM_EVENT_OPTEE_ABSENT      "platform_optee_absent"
#define TEAR_PLATFORM_EVENT_PROFILE_VERIFIED  "platform_profile_verified"
#define TEAR_PLATFORM_EVENT_BACKEND_AVAILABLE "platform_backend_available"
#define TEAR_PLATFORM_EVENT_BACKEND_REJECTED  "platform_backend_rejected"

#endif
