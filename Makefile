# SPDX-License-Identifier: Apache-2.0

BUILD := build
HOST_BUILD := $(BUILD)/host

include platforms/host-demo/build.mk
include platforms/qemu-optee/build.mk

HOST_DEMO_PLAN := plans/host-demo.plan
QEMU_OPTEE_PLAN := /etc/tear/qemu-optee.plan

OPTEE_QEMU_DIR := external/optee-qemu-v8
OPTEE_TA_DEV_KIT := $(abspath $(OPTEE_QEMU_DIR)/optee_os/out/arm/export-ta_arm64)
OPTEE_TA_DEV_KIT_MK := $(OPTEE_TA_DEV_KIT)/mk/ta_dev_kit.mk
OPTEE_CROSS_COMPILE := $(abspath $(OPTEE_QEMU_DIR)/toolchains/aarch64/bin/aarch64-linux-gnu-)
OPTEE_CLIENT_INCLUDE := $(abspath $(OPTEE_QEMU_DIR)/optee_client/libteec/include)
OPTEE_CLIENT_LIB := $(abspath $(OPTEE_QEMU_DIR)/out-br/target/usr/lib)

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
DEMO_MODEL_SRCS := runtime/demo_model.c
MNIST_MODEL_SRCS := runtime/mnist_model.c

.PHONY: build test full-verify clean clean-all mnist-assets \
	host-demo-build host-demo-test host-test \
	qemu-optee-build qemu-optee-run qemu-optee-test

define build-platform-runtime
	mkdir -p $($(1)_BUILD_DIR)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_SUPERVISOR) runtime/supervisor.c $(RUNTIME_PATHS_SRCS) $(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $($(1)_DEMO_MODEL) $(DEMO_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS)
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

define build-platform-mnist
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-I$($(1)_ORT_INCLUDE) \
		-o $($(1)_MNIST_MODEL) $(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-L$($(1)_ORT_LIB) \
		-lonnxruntime \
		-Wl,-rpath,$($(1)_ORT_RPATH)
endef

define build-secure-backend-mock
	@true
endef

define build-secure-backend-optee
	mkdir -p $($(1)_OPTEE_TA_BUILD)
	$(MAKE) -C optee/ta/tear_ta \
		O=$(abspath $($(1)_OPTEE_TA_BUILD)) \
		TA_DEV_KIT_DIR=$(OPTEE_TA_DEV_KIT) \
		CROSS_COMPILE=$(OPTEE_CROSS_COMPILE)
	mkdir -p $($(1)_BUILD_DIR)/optee
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-I$(OPTEE_CLIENT_INCLUDE) \
		-Ioptee/ta/tear_ta/include \
		-Ioptee/ca \
		-o $($(1)_OPTEE_CA) \
		optee/ca/tear_ca.c \
		optee/ca/tear_optee_client.c \
		$(OBSERVABILITY_SRCS) \
		-L$(OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(OPTEE_CLIENT_LIB) \
		-lteec
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-DTEAR_ENABLE_OPTEE \
		-Iruntime \
		-Ioptee/ca \
		-Ioptee/ta/tear_ta/include \
		-I$(OPTEE_CLIENT_INCLUDE) \
		-o $($(1)_OPTEE_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS) \
		optee/ca/tear_optee_client.c \
		-L$(OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(OPTEE_CLIENT_LIB) \
		-lteec
endef

define build-secure-backend
$(if $(filter optee,$($(1)_SECURE_BACKEND)),$(call build-secure-backend-optee,$(1)),$(call build-secure-backend-mock,$(1)))
endef

define stage-host-demo-compat
	rm -rf $(HOST_BUILD)
	ln -s platforms/$(HOST_DEMO_ID) $(HOST_BUILD)
endef

define stage-qemu-optee-compat
	ln -sf platforms/$(QEMU_OPTEE_ID)/tear-supervisor $(BUILD)/tear-supervisor
	ln -sf platforms/$(QEMU_OPTEE_ID)/demo-model $(BUILD)/demo-model
	ln -sf platforms/$(QEMU_OPTEE_ID)/mnist-model $(BUILD)/mnist-model
	ln -sf platforms/$(QEMU_OPTEE_ID)/tear-runtime-manager $(BUILD)/tear-runtime-manager
	ln -sf platforms/$(QEMU_OPTEE_ID)/tear-trustd $(BUILD)/tear-trustd
	ln -sf platforms/$(QEMU_OPTEE_ID)/tearictl $(BUILD)/tearictl
	ln -sf platforms/$(QEMU_OPTEE_ID)/tear-optd $(BUILD)/tear-optd
	mkdir -p $(BUILD)/optee
	ln -sf ../platforms/$(QEMU_OPTEE_ID)/optee/tear-optee-ca $(BUILD)/optee/tear-optee-ca
	ln -sf ../platforms/$(QEMU_OPTEE_ID)/optee/tear-trustd-optee $(BUILD)/optee/tear-trustd-optee
endef

mnist-assets:
	./scripts/fetch-mnist-onnx.sh

#
# Platform: host-demo
#
host-demo-build: mnist-assets
	mkdir -p $(HOST_DEMO_BUILD_DIR)
	$(HOST_DEMO_CC) -static -O2 -Wall -Wextra $(HOST_DEMO_CFLAGS) \
		-o $(HOST_DEMO_HELLO) runtime/hello.c
	$(call build-platform-runtime,HOST_DEMO)
ifeq ($(HOST_DEMO_ENABLE_ONNXRUNTIME),1)
	$(call build-platform-mnist,HOST_DEMO)
endif
	$(call build-secure-backend,HOST_DEMO)
	$(call stage-host-demo-compat)

host-demo-test: host-demo-build
	./scripts/run-host-demo.sh "$(HOST_DEMO_PLAN)"

host-test: host-demo-test

#
# Platform: qemu-optee
#
$(OPTEE_TA_DEV_KIT_MK):
	./scripts/optee-qemu.sh

qemu-optee-build: mnist-assets $(OPTEE_TA_DEV_KIT_MK)
	mkdir -p $(QEMU_OPTEE_BUILD_DIR)
	$(call build-platform-runtime,QEMU_OPTEE)
ifeq ($(QEMU_OPTEE_ENABLE_ONNXRUNTIME),1)
	$(call build-platform-mnist,QEMU_OPTEE)
endif
	$(call build-secure-backend,QEMU_OPTEE)
	$(call stage-qemu-optee-compat)
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)
	./scripts/optee-qemu.sh

qemu-optee-run:
	$(MAKE) -C $(OPTEE_QEMU_DIR)/build run-only

qemu-optee-test: qemu-optee-build
	./scripts/run-qemu-optee.sh "$(QEMU_OPTEE_PLAN)"

#
# main targets
#
build: host-demo-build qemu-optee-build

test: host-demo-test qemu-optee-test

clean:
	rm -rf $(BUILD)/rootfs
	rm -f $(BUILD)/tear-supervisor $(BUILD)/demo-model $(BUILD)/mnist-model
	rm -f $(BUILD)/tear-runtime-manager $(BUILD)/tear-trustd $(BUILD)/tearictl $(BUILD)/tear-optd
	rm -f $(BUILD)/optee-normal-world.log $(BUILD)/optee-secure-world.log
	rm -rf $(BUILD)/optee
	rm -rf $(BUILD)/host
	rm -rf $(BUILD)/platforms

clean-all:
	rm -rf $(BUILD)
	rm -rf $(OPTEE_QEMU_DIR)
	rm -rf external/onnxruntime
	rm -rf external/onnxruntime-aarch64
