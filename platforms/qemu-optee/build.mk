# SPDX-License-Identifier: Apache-2.0

# Platform naming convention:
#   <platform>-<secure-backend>
#
# This platform is:
#   qemu-optee
#
# Meaning:
#   QEMU platform with OP-TEE secure backend.

QEMU_OPTEE_ID := qemu-optee
QEMU_OPTEE_ARCH := aarch64
QEMU_OPTEE_PLAN := /etc/tear/qemu-optee.plan
QEMU_OPTEE_BUILD_DIR := $(BUILD)/platforms/$(QEMU_OPTEE_ID)
QEMU_OPTEE_WORKLOAD_BUILD := $(BUILD)/workloads/$(QEMU_OPTEE_ID)

QEMU_OPTEE_CC := aarch64-linux-gnu-gcc
QEMU_OPTEE_CFLAGS :=

QEMU_OPTEE_SECURE_BACKEND := optee
QEMU_OPTEE_ENABLE_ONNXRUNTIME := 1

QEMU_OPTEE_ORT_INCLUDE := external/onnxruntime-aarch64/include
QEMU_OPTEE_ORT_LIB := external/onnxruntime-aarch64/lib
QEMU_OPTEE_ORT_RPATH := /usr/lib

QEMU_OPTEE_DIR := external/optee-qemu-v8
QEMU_OPTEE_TA_DEV_KIT := $(abspath $(QEMU_OPTEE_DIR)/optee_os/out/arm/export-ta_arm64)
QEMU_OPTEE_CROSS_COMPILE := $(abspath $(QEMU_OPTEE_DIR)/toolchains/aarch64/bin/aarch64-linux-gnu-)
QEMU_OPTEE_CLIENT_INCLUDE := $(abspath $(QEMU_OPTEE_DIR)/optee_client/libteec/include)
QEMU_OPTEE_CLIENT_LIB := $(abspath $(QEMU_OPTEE_DIR)/out-br/target/usr/lib)

QEMU_OPTEE_SUPERVISOR := $(QEMU_OPTEE_BUILD_DIR)/tear-supervisor
QEMU_OPTEE_RUNTIME_MANAGER := $(QEMU_OPTEE_BUILD_DIR)/tear-runtime-manager
QEMU_OPTEE_TRUSTD := $(QEMU_OPTEE_BUILD_DIR)/tear-trustd
QEMU_OPTEE_TEARICTL := $(QEMU_OPTEE_BUILD_DIR)/tearictl
QEMU_OPTEE_OPTD := $(QEMU_OPTEE_BUILD_DIR)/tear-optd

QEMU_OPTEE_OPTEE_TA_BUILD := $(QEMU_OPTEE_BUILD_DIR)/optee/tear_ta
QEMU_OPTEE_OPTEE_CA := $(QEMU_OPTEE_BUILD_DIR)/optee/tear-optee-ca
QEMU_OPTEE_OPTEE_TRUSTD := $(QEMU_OPTEE_BUILD_DIR)/optee/tear-trustd-optee

define build-qemu-optee-prepare
	$(call fetch-workload-assets-demo-model)
	$(call fetch-workload-assets-mnist-model)
	./scripts/optee-qemu.sh prepare
	mkdir -p $(QEMU_OPTEE_BUILD_DIR)
	mkdir -p $(QEMU_OPTEE_WORKLOAD_BUILD)
endef

define build-qemu-optee-runtime
	$(call runtime-build-platform,QEMU_OPTEE)
endef

define build-qemu-optee-workloads
	$(call workload-build-demo-model,QEMU_OPTEE)
	$(call workload-build-mnist-model,QEMU_OPTEE)
endef

define build-qemu-optee-secure-backend
	mkdir -p $(QEMU_OPTEE_OPTEE_TA_BUILD)
	$(MAKE) -C optee/ta/tear_ta \
		O=$(abspath $(QEMU_OPTEE_OPTEE_TA_BUILD)) \
		TA_DEV_KIT_DIR=$(QEMU_OPTEE_TA_DEV_KIT) \
		CROSS_COMPILE=$(QEMU_OPTEE_CROSS_COMPILE)
	mkdir -p $(QEMU_OPTEE_BUILD_DIR)/optee
	$(QEMU_OPTEE_CC) -O2 -Wall -Wextra $(QEMU_OPTEE_CFLAGS) \
		-Iruntime \
		-I$(QEMU_OPTEE_CLIENT_INCLUDE) \
		-Ioptee/ta/tear_ta/include \
		-Ioptee/ca \
		-o $(QEMU_OPTEE_OPTEE_CA) \
		optee/ca/tear_ca.c \
		optee/ca/tear_optee_client.c \
		$(OBSERVABILITY_SRCS) \
		-L$(QEMU_OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(QEMU_OPTEE_CLIENT_LIB) \
		-lteec
	$(QEMU_OPTEE_CC) -O2 -Wall -Wextra $(QEMU_OPTEE_CFLAGS) \
		-DTEAR_ENABLE_OPTEE \
		-Iruntime \
		-Ioptee/ca \
		-Ioptee/ta/tear_ta/include \
		-I$(QEMU_OPTEE_CLIENT_INCLUDE) \
		-o $(QEMU_OPTEE_OPTEE_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS) \
		optee/ca/tear_optee_client.c \
		-L$(QEMU_OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(QEMU_OPTEE_CLIENT_LIB) \
		-lteec
endef

define install-qemu-optee
	TEAR_PLATFORM_ID="$(QEMU_OPTEE_ID)" \
	TEAR_PLATFORM_BUILD_DIR="$(QEMU_OPTEE_BUILD_DIR)" \
	TEAR_SUPERVISOR_BIN="$(QEMU_OPTEE_SUPERVISOR)" \
	TEAR_TRUSTD_BIN="$(QEMU_OPTEE_TRUSTD)" \
	TEARICTL_BIN="$(QEMU_OPTEE_TEARICTL)" \
	TEAR_OPTD_BIN="$(QEMU_OPTEE_OPTD)" \
	TEAR_RUNTIME_MANAGER_BIN="$(QEMU_OPTEE_RUNTIME_MANAGER)" \
	TEAR_DEMO_MODEL_BIN="$(QEMU_OPTEE_WORKLOAD_BUILD)/$(DEMO_MODEL_ID)-$(QEMU_OPTEE_ID)" \
	TEAR_MNIST_MODEL_BIN="$(QEMU_OPTEE_WORKLOAD_BUILD)/$(MNIST_MODEL_ID)-$(QEMU_OPTEE_ID)" \
	TEAR_TA="$(QEMU_OPTEE_OPTEE_TA_BUILD)/7c9d7b3a-2f4e-4c8f-9a11-6b4454454152.ta" \
	TEAR_CA="$(QEMU_OPTEE_OPTEE_CA)" \
	TEAR_OPTEE_TRUSTD="$(QEMU_OPTEE_OPTEE_TRUSTD)" \
	./scripts/install-optee-qemu-files.sh $(QEMU_OPTEE_DIR)
endef

define build-qemu-optee-finalize
	$(call install-qemu-optee)
	./scripts/optee-qemu.sh build
endef

define run-qemu-optee
	$(MAKE) -C $(QEMU_OPTEE_DIR)/build run-only
endef

define test-qemu-optee
	./scripts/run-qemu-optee.sh "$(QEMU_OPTEE_PLAN)"
endef
