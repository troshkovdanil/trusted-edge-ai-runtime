# SPDX-License-Identifier: Apache-2.0

HOST_MOCK_ID := host-mock
HOST_MOCK_ARCH := x86_64
HOST_MOCK_PLAN := plans/host-mock.plan
HOST_MOCK_BUILD_DIR := $(BUILD)/platforms/$(HOST_MOCK_ID)

HOST_MOCK_CC := gcc
HOST_MOCK_CFLAGS := -DTEAR_HOST_BUILD

HOST_MOCK_SECURE_BACKEND := mock
HOST_MOCK_ENABLE_ONNXRUNTIME := 1

HOST_MOCK_ORT_INCLUDE := external/onnxruntime/include
HOST_MOCK_ORT_LIB := external/onnxruntime/lib
HOST_MOCK_ORT_RPATH := '$$ORIGIN/../../../external/onnxruntime/lib'

HOST_MOCK_SUPERVISOR := $(HOST_MOCK_BUILD_DIR)/tear-supervisor-host
HOST_MOCK_RUNTIME_MANAGER := $(HOST_MOCK_BUILD_DIR)/tear-runtime-manager-host
HOST_MOCK_TRUSTD := $(HOST_MOCK_BUILD_DIR)/tear-trustd-host
HOST_MOCK_TEARICTL := $(HOST_MOCK_BUILD_DIR)/tearictl-host
HOST_MOCK_OPTD := $(HOST_MOCK_BUILD_DIR)/tear-optd-host

define build-host-mock-prepare
	mkdir -p $(HOST_MOCK_BUILD_DIR)
endef

define build-host-mock-runtime
	$(call runtime-build-platform,HOST_MOCK)
endef

define build-host-mock-workloads
	$(call workload-build-demo-model,HOST_MOCK)
	$(call workload-build-mnist-model,HOST_MOCK)
endef

define build-host-mock-secure-backend
	@true
endef

define build-host-mock-finalize
	@true
endef

define run-host-mock
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"
endef

define test-host-mock
	$(call run-host-mock)
endef
