# SPDX-License-Identifier: Apache-2.0

HOST_MOCK_ID := host-mock
HOST_MOCK_ARCH := x86_64
HOST_MOCK_BUILD_DIR := $(BUILD)/platforms/$(HOST_MOCK_ID)

HOST_MOCK_CC := gcc
HOST_MOCK_CFLAGS := -DTEAR_HOST_BUILD

HOST_MOCK_SECURE_BACKEND := mock
HOST_MOCK_ENABLE_ONNXRUNTIME := 1

HOST_MOCK_ORT_INCLUDE := external/onnxruntime/include
HOST_MOCK_ORT_LIB := external/onnxruntime/lib
HOST_MOCK_ORT_RPATH := '$$ORIGIN/../../../external/onnxruntime/lib'

HOST_MOCK_HELLO := $(HOST_MOCK_BUILD_DIR)/hello-host
HOST_MOCK_SUPERVISOR := $(HOST_MOCK_BUILD_DIR)/tear-supervisor-host
HOST_MOCK_DEMO_MODEL := $(HOST_MOCK_BUILD_DIR)/demo-model-host
HOST_MOCK_MNIST_MODEL := $(HOST_MOCK_BUILD_DIR)/mnist-model-host
HOST_MOCK_RUNTIME_MANAGER := $(HOST_MOCK_BUILD_DIR)/tear-runtime-manager-host
HOST_MOCK_TRUSTD := $(HOST_MOCK_BUILD_DIR)/tear-trustd-host
HOST_MOCK_TEARICTL := $(HOST_MOCK_BUILD_DIR)/tearictl-host
HOST_MOCK_OPTD := $(HOST_MOCK_BUILD_DIR)/tear-optd-host
