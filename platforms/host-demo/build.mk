# SPDX-License-Identifier: Apache-2.0

HOST_DEMO_ID := host-demo
HOST_DEMO_ARCH := x86_64
HOST_DEMO_BUILD_DIR := $(BUILD)/platforms/$(HOST_DEMO_ID)

HOST_DEMO_CC := gcc
HOST_DEMO_CFLAGS := -DTEAR_HOST_BUILD

HOST_DEMO_SECURE_BACKEND := mock
HOST_DEMO_ENABLE_ONNXRUNTIME := 1

HOST_DEMO_ORT_INCLUDE := external/onnxruntime/include
HOST_DEMO_ORT_LIB := external/onnxruntime/lib
HOST_DEMO_ORT_RPATH := '$$ORIGIN/../../../external/onnxruntime/lib'

HOST_DEMO_HELLO := $(HOST_DEMO_BUILD_DIR)/hello-host
HOST_DEMO_SUPERVISOR := $(HOST_DEMO_BUILD_DIR)/tear-supervisor-host
HOST_DEMO_DEMO_MODEL := $(HOST_DEMO_BUILD_DIR)/demo-model-host
HOST_DEMO_MNIST_MODEL := $(HOST_DEMO_BUILD_DIR)/mnist-model-host
HOST_DEMO_RUNTIME_MANAGER := $(HOST_DEMO_BUILD_DIR)/tear-runtime-manager-host
HOST_DEMO_TRUSTD := $(HOST_DEMO_BUILD_DIR)/tear-trustd-host
HOST_DEMO_TEARICTL := $(HOST_DEMO_BUILD_DIR)/tearictl-host
HOST_DEMO_OPTD := $(HOST_DEMO_BUILD_DIR)/tear-optd-host
