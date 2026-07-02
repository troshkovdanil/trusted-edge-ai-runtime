# SPDX-License-Identifier: Apache-2.0

BUILD := build

include runtime/build.mk
include workloads/demo-model/build.mk
include workloads/mnist-model/build.mk
include platforms/host-mock/build.mk
include platforms/qemu-optee/build.mk

.PHONY: \
	host-mock-build host-mock-run host-mock-test \
	qemu-optee-build qemu-optee-run qemu-optee-test \
	build test clean clean-all

host-mock-build:
	$(call build-host-mock-prepare)
	$(call build-host-mock-runtime)
	$(call build-host-mock-workloads)
	$(call build-host-mock-secure-backend)
	$(call build-host-mock-finalize)

host-mock-run:
	$(call run-host-mock)

host-mock-test: host-mock-build
	$(call test-host-mock)

qemu-optee-build:
	$(call build-qemu-optee-prepare)
	$(call build-qemu-optee-runtime)
	$(call build-qemu-optee-workloads)
	$(call build-qemu-optee-secure-backend)
	$(call build-qemu-optee-finalize)

qemu-optee-run:
	$(call run-qemu-optee)

qemu-optee-test: qemu-optee-build
	$(call test-qemu-optee)

build: host-mock-build qemu-optee-build

test: host-mock-test qemu-optee-test

clean:
	rm -rf $(BUILD)

clean-all: clean
	rm -rf $(QEMU_OPTEE_DIR)
	rm -rf external/onnxruntime
	rm -rf external/onnxruntime-aarch64
