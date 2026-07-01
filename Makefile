# SPDX-License-Identifier: Apache-2.0

BUILD := build
WORKLOAD_BUILD := $(BUILD)/workloads

include platforms/host-mock/build.mk
include platforms/qemu-optee/build.mk
include workloads/demo-model/build.mk
include workloads/mnist-model/build.mk

RUNTIME_PATHS_SRCS := runtime/runtime_paths.c
OBSERVABILITY_SRCS := runtime/observability.c
PROFILE_SRCS := runtime/profile.c
MANIFEST_SRCS := runtime/model_manifest.c
TRUST_CLIENT_SRCS := runtime/trust_client.c
TRUSTD_SRCS := runtime/trustd.c runtime/trusted_state.c
OPTD_SRCS := runtime/optd.c runtime/optimizer_policy.c
WORKLOAD_ADAPTER_SRCS := runtime/workload_adapter.c
PLATFORM_ADAPTER_SRCS := runtime/platform_adapter.c
RUNTIME_MANAGER_SRCS := runtime/runtime_manager_main.c runtime/runtime_manager.c
TEARICTL_SRCS := runtime/tearictl.c

.PHONY: workload-assets \
	host-mock-build host-mock-run host-mock-test \
	qemu-optee-build qemu-optee-run qemu-optee-test \
	build test clean clean-all

define build-platform-runtime
	mkdir -p $($(1)_BUILD_DIR)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_SUPERVISOR) runtime/supervisor.c $(RUNTIME_PATHS_SRCS) $(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_RUNTIME_MANAGER) \
		$(RUNTIME_MANAGER_SRCS) \
		$(WORKLOAD_ADAPTER_SRCS) \
		$(PLATFORM_ADAPTER_SRCS) \
		$(PROFILE_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_TEARICTL) \
		$(TEARICTL_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_OPTD) \
		$(OPTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
endef

define build-platform-demo-model
	mkdir -p $(WORKLOAD_BUILD)/$($(1)_ID)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $(WORKLOAD_BUILD)/$($(1)_ID)/$(DEMO_MODEL_ID)-$($(1)_ID) \
		$(DEMO_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS)
endef

define build-platform-mnist-model
	mkdir -p $(WORKLOAD_BUILD)/$($(1)_ID)
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-I$($(1)_ORT_INCLUDE) \
		-o $(WORKLOAD_BUILD)/$($(1)_ID)/$(MNIST_MODEL_ID)-$($(1)_ID) \
		$(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-L$($(1)_ORT_LIB) \
		-lonnxruntime \
		-Wl,-rpath,$($(1)_ORT_RPATH)
endef

define build-platform-workloads
	$(call build-platform-demo-model,$(1))
	$(if $(filter 1,$($(1)_ENABLE_ONNXRUNTIME)),$(call build-platform-mnist-model,$(1)),)
endef

define build-secure-backend-mock
	@true
endef

define build-secure-backend-optee
	mkdir -p $($(1)_OPTEE_TA_BUILD)
	$(MAKE) -C optee/ta/tear_ta \
		O=$(abspath $($(1)_OPTEE_TA_BUILD)) \
		TA_DEV_KIT_DIR=$($(1)_TA_DEV_KIT) \
		CROSS_COMPILE=$($(1)_CROSS_COMPILE)
	mkdir -p $($(1)_BUILD_DIR)/optee
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-I$($(1)_CLIENT_INCLUDE) \
		-Ioptee/ta/tear_ta/include \
		-Ioptee/ca \
		-o $($(1)_OPTEE_CA) \
		optee/ca/tear_ca.c \
		optee/ca/tear_optee_client.c \
		$(OBSERVABILITY_SRCS) \
		-L$($(1)_CLIENT_LIB) \
		-Wl,-rpath-link,$($(1)_CLIENT_LIB) \
		-lteec
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-DTEAR_ENABLE_OPTEE \
		-Iruntime \
		-Ioptee/ca \
		-Ioptee/ta/tear_ta/include \
		-I$($(1)_CLIENT_INCLUDE) \
		-o $($(1)_OPTEE_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS) \
		optee/ca/tear_optee_client.c \
		-L$($(1)_CLIENT_LIB) \
		-Wl,-rpath-link,$($(1)_CLIENT_LIB) \
		-lteec
endef

define build-secure-backend
$(if $(filter optee,$($(1)_SECURE_BACKEND)),$(call build-secure-backend-optee,$(1)),$(call build-secure-backend-mock,$(1)))
endef

define install-qemu-optee
	TEAR_PLATFORM_ID="$(QEMU_OPTEE_ID)" \
	TEAR_PLATFORM_BUILD_DIR="$(QEMU_OPTEE_BUILD_DIR)" \
	TEAR_SUPERVISOR_BIN="$(QEMU_OPTEE_SUPERVISOR)" \
	TEAR_TRUSTD_BIN="$(QEMU_OPTEE_TRUSTD)" \
	TEARICTL_BIN="$(QEMU_OPTEE_TEARICTL)" \
	TEAR_OPTD_BIN="$(QEMU_OPTEE_OPTD)" \
	TEAR_RUNTIME_MANAGER_BIN="$(QEMU_OPTEE_RUNTIME_MANAGER)" \
	TEAR_DEMO_MODEL_BIN="$(WORKLOAD_BUILD)/$(QEMU_OPTEE_ID)/$(DEMO_MODEL_ID)-$(QEMU_OPTEE_ID)" \
	TEAR_MNIST_MODEL_BIN="$(WORKLOAD_BUILD)/$(QEMU_OPTEE_ID)/$(MNIST_MODEL_ID)-$(QEMU_OPTEE_ID)" \
	TEAR_TA="$(QEMU_OPTEE_OPTEE_TA_BUILD)/7c9d7b3a-2f4e-4c8f-9a11-6b4454454152.ta" \
	TEAR_CA="$(QEMU_OPTEE_OPTEE_CA)" \
	TEAR_OPTEE_TRUSTD="$(QEMU_OPTEE_OPTEE_TRUSTD)" \
	./scripts/install-optee-qemu-files.sh $(QEMU_OPTEE_DIR)
endef

define fetch-workload-assets
	$(call fetch-workload-assets-demo-model)
	$(call fetch-workload-assets-mnist-model)
endef

workload-assets:
	$(call fetch-workload-assets)

host-mock-build: workload-assets
	mkdir -p $(HOST_MOCK_BUILD_DIR)
	$(call build-platform-runtime,HOST_MOCK)
	$(call build-platform-workloads,HOST_MOCK)
	$(call build-secure-backend,HOST_MOCK)

host-mock-run:
	$(HOST_MOCK_SUPERVISOR)

host-mock-test: host-mock-build
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"

qemu-optee-build: workload-assets
	./scripts/optee-qemu.sh prepare
	mkdir -p $(QEMU_OPTEE_BUILD_DIR)
	$(call build-platform-runtime,QEMU_OPTEE)
	$(call build-platform-workloads,QEMU_OPTEE)
	$(call build-secure-backend,QEMU_OPTEE)
	$(call install-qemu-optee)
	./scripts/optee-qemu.sh build

qemu-optee-run:
	$(MAKE) -C $(QEMU_OPTEE_DIR)/build run-only

qemu-optee-test: qemu-optee-build
	./scripts/run-qemu-optee.sh "$(QEMU_OPTEE_PLAN)"

build: host-mock-build qemu-optee-build

test: host-mock-test qemu-optee-test

clean:
	rm -rf $(BUILD)

clean-all: clean
	rm -rf $(QEMU_OPTEE_DIR)
	rm -rf external/onnxruntime
	rm -rf external/onnxruntime-aarch64
