# SPDX-License-Identifier: Apache-2.0

BUILD := build

include runtime/build.mk
include workloads/demo-model/build.mk
include workloads/mnist-model/build.mk
include platforms/host-mock/build.mk
include platforms/qemu-optee/build.mk

define build-platform
	$(call build-$(1)-prepare)
	$(call build-$(1)-runtime)
	$(call build-$(1)-workloads)
	$(call build-$(1)-secure-backend)
	$(call build-$(1)-finalize)
endef

host-mock-build:
	$(call build-platform,host-mock)

host-mock-run:
	$(call run-host-mock)

host-mock-test: host-mock-build
	$(call test-host-mock)

qemu-optee-build:
	$(call build-platform,qemu-optee)

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

.PHONY: \
	host-mock-build host-mock-run host-mock-test \
	qemu-optee-build qemu-optee-run qemu-optee-test \
	build test clean clean-all
