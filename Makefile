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

define fetch-workload-assets
	$(call fetch-workload-assets-demo-model)
	$(call fetch-workload-assets-mnist-model)
endef

workload-assets:
	$(call fetch-workload-assets)

host-mock-build: workload-assets
	$(call build-host-mock-prepare)
	$(call build-host-mock-runtime)
	$(call build-host-mock-workloads)
	$(call build-host-mock-secure-backend)
	$(call build-host-mock-finalize)

host-mock-run:
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"

host-mock-test: host-mock-build
	./scripts/run-host-mock.sh "$(HOST_MOCK_PLAN)"

qemu-optee-build: workload-assets
	$(call build-qemu-optee-prepare)
	$(call build-qemu-optee-runtime)
	$(call build-qemu-optee-workloads)
	$(call build-qemu-optee-secure-backend)
	$(call build-qemu-optee-finalize)

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
