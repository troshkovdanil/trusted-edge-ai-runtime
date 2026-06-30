# SPDX-License-Identifier: Apache-2.0

BUILD := build
HOST_BUILD := $(BUILD)/host

include platforms/host-demo/build.mk
include platforms/qemu-optee/build.mk

SUPERVISOR := $(BUILD)/tear-supervisor
DEMO_MODEL := $(BUILD)/demo-model
RUNTIME_MANAGER := $(BUILD)/tear-runtime-manager
TRUSTD := $(BUILD)/tear-trustd
TEARICTL := $(BUILD)/tearictl
OPTD := $(BUILD)/tear-optd

HOST_HELLO := $(HOST_BUILD)/hello-host
HOST_SUPERVISOR := $(HOST_BUILD)/tear-supervisor-host
HOST_DEMO_MODEL := $(HOST_BUILD)/demo-model-host
HOST_MNIST_MODEL := $(HOST_BUILD)/mnist-model-host
HOST_RUNTIME_MANAGER := $(HOST_BUILD)/tear-runtime-manager-host
HOST_TRUSTD := $(HOST_BUILD)/tear-trustd-host
HOST_TEARICTL := $(HOST_BUILD)/tearictl-host
HOST_OPTD := $(HOST_BUILD)/tear-optd-host

HOST_DEMO_PLAN := plans/host-demo.plan
QEMU_OPTEE_PLAN := /etc/tear/qemu-optee.plan

MNIST_MODEL_FILE := models/mnist/mnist.onnx
ORT_INCLUDE := external/onnxruntime/include/onnxruntime_c_api.h

OPTEE_QEMU_DIR := external/optee-qemu-v8
OPTEE_TA_DEV_KIT := $(abspath $(OPTEE_QEMU_DIR)/optee_os/out/arm/export-ta_arm64)
OPTEE_TA_DEV_KIT_MK := $(OPTEE_TA_DEV_KIT)/mk/ta_dev_kit.mk
OPTEE_CROSS_COMPILE := $(abspath $(OPTEE_QEMU_DIR)/toolchains/aarch64/bin/aarch64-linux-gnu-)
TEAR_TA_BUILD := $(BUILD)/optee/tear_ta
TEAR_CA := $(BUILD)/optee/tear-optee-ca
OPTEE_CLIENT_INCLUDE := $(abspath $(OPTEE_QEMU_DIR)/optee_client/libteec/include)
OPTEE_CLIENT_LIB := $(abspath $(OPTEE_QEMU_DIR)/out-br/target/usr/lib)
OPTEE_TRUSTD := $(BUILD)/optee/tear-trustd-optee

MNIST_MODEL := $(BUILD)/mnist-model

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
	qemu-optee-runtime qemu-optee-install qemu-optee-build qemu-optee-run qemu-optee-test \
	optee-ta optee-ca optee-trustd

define build-runtime-binaries
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

define build-runtime-mnist
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-I$($(1)_ORT_INCLUDE) \
		-o $($(1)_MNIST_MODEL) $(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-L$($(1)_ORT_LIB) \
		-lonnxruntime \
		-Wl,-rpath,$($(1)_ORT_RPATH)
endef

mnist-assets:
	./scripts/fetch-mnist-onnx.sh

#
# Platform: host-demo
#
HOST_DEMO_BUILD_DIR := $(HOST_BUILD)
HOST_DEMO_SUPERVISOR := $(HOST_SUPERVISOR)
HOST_DEMO_DEMO_MODEL := $(HOST_DEMO_MODEL)
HOST_DEMO_MNIST_MODEL := $(HOST_MNIST_MODEL)
HOST_DEMO_RUNTIME_MANAGER := $(HOST_RUNTIME_MANAGER)
HOST_DEMO_TRUSTD := $(HOST_TRUSTD)
HOST_DEMO_TEARICTL := $(HOST_TEARICTL)
HOST_DEMO_OPTD := $(HOST_OPTD)

host-demo-build: mnist-assets
	mkdir -p $(HOST_BUILD)
	$(HOST_DEMO_CC) -static -O2 -Wall -Wextra $(HOST_DEMO_CFLAGS) \
		-o $(HOST_HELLO) runtime/hello.c
	$(call build-runtime-binaries,HOST_DEMO)
ifeq ($(HOST_DEMO_ENABLE_ONNXRUNTIME),1)
	$(call build-runtime-mnist,HOST_DEMO)
endif

host-demo-test: host-demo-build
	./scripts/run-host-demo.sh "$(HOST_DEMO_PLAN)"

host-test: host-demo-test

#
# Platform: qemu-optee
#
QEMU_OPTEE_BUILD_DIR := $(BUILD)
QEMU_OPTEE_SUPERVISOR := $(SUPERVISOR)
QEMU_OPTEE_DEMO_MODEL := $(DEMO_MODEL)
QEMU_OPTEE_MNIST_MODEL := $(MNIST_MODEL)
QEMU_OPTEE_RUNTIME_MANAGER := $(RUNTIME_MANAGER)
QEMU_OPTEE_TRUSTD := $(TRUSTD)
QEMU_OPTEE_TEARICTL := $(TEARICTL)
QEMU_OPTEE_OPTD := $(OPTD)

$(OPTEE_TA_DEV_KIT_MK):
	./scripts/optee-qemu.sh

qemu-optee-runtime: mnist-assets
	$(call build-runtime-binaries,QEMU_OPTEE)
ifeq ($(QEMU_OPTEE_ENABLE_ONNXRUNTIME),1)
	$(call build-runtime-mnist,QEMU_OPTEE)
endif

optee-ta: $(OPTEE_TA_DEV_KIT_MK)
	mkdir -p $(TEAR_TA_BUILD)
	$(MAKE) -C optee/ta/tear_ta \
		O=$(abspath $(TEAR_TA_BUILD)) \
		TA_DEV_KIT_DIR=$(OPTEE_TA_DEV_KIT) \
		CROSS_COMPILE=$(OPTEE_CROSS_COMPILE)

optee-ca:
	mkdir -p $(BUILD)/optee
	$(QEMU_OPTEE_CC) -O2 -Wall -Wextra $(QEMU_OPTEE_CFLAGS) \
		-Iruntime \
		-I$(OPTEE_CLIENT_INCLUDE) \
		-Ioptee/ta/tear_ta/include \
		-Ioptee/ca \
		-o $(TEAR_CA) \
		optee/ca/tear_ca.c \
		optee/ca/tear_optee_client.c \
		$(OBSERVABILITY_SRCS) \
		-L$(OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(OPTEE_CLIENT_LIB) \
		-lteec

optee-trustd: optee-ca
	mkdir -p $(BUILD)/optee
	$(QEMU_OPTEE_CC) -O2 -Wall -Wextra $(QEMU_OPTEE_CFLAGS) \
		-DTEAR_ENABLE_OPTEE \
		-Iruntime \
		-Ioptee/ca \
		-Ioptee/ta/tear_ta/include \
		-I$(OPTEE_CLIENT_INCLUDE) \
		-o $(OPTEE_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS) \
		optee/ca/tear_optee_client.c \
		-L$(OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(OPTEE_CLIENT_LIB) \
		-lteec

ifeq ($(QEMU_OPTEE_TRUST_BACKEND),optee)
qemu-optee-install: qemu-optee-runtime optee-ta optee-ca optee-trustd
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)
else
qemu-optee-install: qemu-optee-runtime
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)
endif

qemu-optee-build: qemu-optee-install
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
	rm -f $(SUPERVISOR) $(DEMO_MODEL) $(RUNTIME_MANAGER) $(TRUSTD) $(TEARICTL) $(OPTD) $(MNIST_MODEL)
	rm -f $(BUILD)/optee-normal-world.log $(BUILD)/optee-secure-world.log
	rm -rf $(BUILD)/optee
	rm -rf $(BUILD)/host

clean-all:
	rm -rf $(BUILD)
	rm -rf $(OPTEE_QEMU_DIR)
	rm -rf external/onnxruntime
	rm -rf external/onnxruntime-aarch64
