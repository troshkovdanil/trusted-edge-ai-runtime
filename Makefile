# SPDX-License-Identifier: Apache-2.0

CC := aarch64-linux-gnu-gcc

BUILD := build
HOST_BUILD := $(BUILD)/host
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

HOST_PLAN := plans/host-demo.plan
QEMU_OPTEE_PLAN := plans/qemu-optee.plan

MNIST_MODEL_FILE := models/mnist/mnist.onnx
ORT_INCLUDE := external/onnxruntime/include/onnxruntime_c_api.h

OPTEE_QEMU_DIR := external/optee-qemu-v8
OPTEE_TA_DEV_KIT := $(abspath $(OPTEE_QEMU_DIR)/optee_os/out/arm/export-ta_arm64)
OPTEE_TA_DEV_KIT_MK := $(OPTEE_TA_DEV_KIT)/mk/ta_dev_kit.mk
OPTEE_CROSS_COMPILE := $(abspath $(OPTEE_QEMU_DIR)/toolchains/aarch64/bin/aarch64-linux-gnu-)
TEAR_TA_BUILD := $(BUILD)/optee/tear_ta
TEAR_TA := $(TEAR_TA_BUILD)/7c9d7b3a-2f4e-4c8f-9a11-6b4454454152.ta
TEAR_CA := $(BUILD)/optee/tear-optee-ca
OPTEE_CLIENT_INCLUDE := $(abspath $(OPTEE_QEMU_DIR)/optee_client/libteec/include)
OPTEE_CLIENT_LIB := $(abspath $(OPTEE_QEMU_DIR)/out-br/target/usr/lib)
OPTEE_TRUSTD := $(BUILD)/optee/tear-trustd-optee

MNIST_MODEL := $(BUILD)/mnist-model
ORT_AARCH64_DIR := external/onnxruntime-aarch64
ORT_AARCH64_INCLUDE := $(ORT_AARCH64_DIR)/include
ORT_AARCH64_LIB := $(ORT_AARCH64_DIR)/lib

RUNTIME_PATHS_SRCS := runtime/runtime_paths.c
OBSERVABILITY_SRCS := runtime/observability.c
PROFILE_SRCS := runtime/profile.c
MANIFEST_SRCS := runtime/model_manifest.c
TRUST_CLIENT_SRCS := runtime/trust_client.c
TRUSTD_SRCS := runtime/trustd.c runtime/trusted_state.c
OPTD_SRCS := runtime/optd.c runtime/optimizer_policy.c
RUNTIME_MANAGER_SRCS := runtime/runtime_manager_main.c runtime/runtime_manager.c
TEARICTL_SRCS := runtime/tearictl.c
DEMO_MODEL_SRCS := runtime/demo_model.c
MNIST_MODEL_SRCS := runtime/mnist_model.c

.PHONY: build clean clean-all mnist-assets \
	host-build host-test host-demo-build host-demo-test \
	qemu-optee-install qemu-optee-build qemu-optee-run qemu-optee-test \
	optee-ta optee-ca optee-trustd full-verify

mnist-assets:
	./scripts/fetch-mnist-onnx.sh

build: mnist-assets
	mkdir -p $(BUILD)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(SUPERVISOR) runtime/supervisor.c $(RUNTIME_PATHS_SRCS) $(OBSERVABILITY_SRCS)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(DEMO_MODEL) $(DEMO_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(RUNTIME_MANAGER) \
		$(RUNTIME_MANAGER_SRCS) \
		$(PROFILE_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TEARICTL) \
		$(TEARICTL_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(OPTD) \
		$(OPTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$(CC) -O2 -Wall -Wextra \
		-I$(ORT_AARCH64_INCLUDE) \
		-o $(MNIST_MODEL) $(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-L$(ORT_AARCH64_LIB) \
		-lonnxruntime \
		-Wl,-rpath,/usr/lib

host-build: mnist-assets
	mkdir -p $(HOST_BUILD)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_HELLO) runtime/hello.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_SUPERVISOR) runtime/supervisor.c $(RUNTIME_PATHS_SRCS) $(OBSERVABILITY_SRCS)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_DEMO_MODEL) $(DEMO_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS)
	gcc -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-Iexternal/onnxruntime/include \
		-o $(HOST_MNIST_MODEL) $(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-Lexternal/onnxruntime/lib -lonnxruntime \
		-Wl,-rpath,'$$ORIGIN/../../external/onnxruntime/lib'
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_RUNTIME_MANAGER) \
		$(RUNTIME_MANAGER_SRCS) \
		$(PROFILE_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TEARICTL) \
		$(TEARICTL_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_OPTD) \
		$(OPTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)

host-demo-build: host-build

host-demo-test: host-demo-build
	./scripts/run-tear-plan.sh host-demo "$(HOST_PLAN)"

host-test: host-demo-test

full-verify: host-demo-test qemu-optee-test

qemu-optee-install: build optee-ta optee-ca optee-trustd
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)

qemu-optee-build: qemu-optee-install
	./scripts/optee-qemu.sh

qemu-optee-run:
	$(MAKE) -C $(OPTEE_QEMU_DIR)/build run-only

qemu-optee-test: qemu-optee-build
	./scripts/run-tear-plan.sh qemu-optee "$(QEMU_OPTEE_PLAN)"

$(OPTEE_TA_DEV_KIT_MK):
	./scripts/optee-qemu.sh

optee-ta: $(OPTEE_TA_DEV_KIT_MK)
	mkdir -p $(TEAR_TA_BUILD)
	$(MAKE) -C optee/ta/tear_ta \
		O=$(abspath $(TEAR_TA_BUILD)) \
		TA_DEV_KIT_DIR=$(OPTEE_TA_DEV_KIT) \
		CROSS_COMPILE=$(OPTEE_CROSS_COMPILE)

optee-ca:
	mkdir -p $(BUILD)/optee
	$(CC) -O2 -Wall -Wextra \
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
	$(CC) -O2 -Wall -Wextra \
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
