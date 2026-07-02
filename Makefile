# SPDX-License-Identifier: Apache-2.0

BUILD := build

# Common runtime build rules.
include runtime/build.mk

# Workload build rules.
include workloads/demo-model/build.mk
include workloads/mnist-model/build.mk

# Platform build rules.
include platforms/host-mock/build.mk
include platforms/qemu-optee/build.mk

# Generic platform lifecycle.
#
# A platform named "<platform>-<secure-backend>" must provide:
#   build-<platform>-<secure-backend>-prepare
#   build-<platform>-<secure-backend>-runtime
#   build-<platform>-<secure-backend>-workloads
#   build-<platform>-<secure-backend>-secure-backend
#   build-<platform>-<secure-backend>-finalize
define build-platform
	$(call build-$(1)-prepare)
	$(call build-$(1)-runtime)
	$(call build-$(1)-workloads)
	$(call build-$(1)-secure-backend)
	$(call build-$(1)-finalize)
endef

# Generic platform run hook:
#   run-<platform>-<secure-backend>
define run-platform
	$(call run-$(1))
endef

# Generic platform test hook:
#   test-<platform>-<secure-backend>
define test-platform
	$(call test-$(1))
endef

host-mock-build:
	$(call build-platform,host-mock)

host-mock-run:
	$(call run-platform,host-mock)

host-mock-test: host-mock-build
	$(call test-platform,host-mock)

qemu-optee-build:
	$(call build-platform,qemu-optee)

qemu-optee-run:
	$(call run-platform,qemu-optee)

qemu-optee-test: qemu-optee-build
	$(call test-platform,qemu-optee)

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
