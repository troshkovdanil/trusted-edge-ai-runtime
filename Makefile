# SPDX-License-Identifier: Apache-2.0

BUILD := build
WORKLOAD_BUILD := $(BUILD)/workloads

include runtime/build.mk
include workloads/demo-model/build.mk
include workloads/mnist-model/build.mk
include platforms/host-mock/build.mk
include platforms/qemu-optee/build.mk

.PHONY: workload-assets \
	host-mock-build host-mock-run host-mock-test \
	qemu-optee-build qemu-optee-run qemu-optee-test \
	build test clean clean-all

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
	$(call build-host-mock-runtime)
	$(call build-host-mock-workloads)
	$(call build-host-mock-secure-backend)

host-mock-run:
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"

host-mock-test: host-mock-build
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"

qemu-optee-build: workload-assets
	./scripts/optee-qemu.sh prepare
	mkdir -p $(QEMU_OPTEE_BUILD_DIR)
	$(call build-qemu-optee-runtime)
	$(call build-qemu-optee-workloads)
	$(call build-qemu-optee-secure-backend)
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
