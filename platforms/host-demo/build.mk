# SPDX-License-Identifier: Apache-2.0

HOST_DEMO_ID := host-demo
HOST_DEMO_ARCH := x86_64
HOST_DEMO_CC := gcc
HOST_DEMO_CFLAGS := -DTEAR_HOST_BUILD
HOST_DEMO_TRUST_BACKEND := file
HOST_DEMO_ENABLE_OPTEE := 0
HOST_DEMO_ENABLE_ONNXRUNTIME := 1
HOST_DEMO_ORT_INCLUDE := external/onnxruntime/include
HOST_DEMO_ORT_LIB := external/onnxruntime/lib
HOST_DEMO_ORT_RPATH := '$$ORIGIN/../../external/onnxruntime/lib'
