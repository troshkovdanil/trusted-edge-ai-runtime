# SPDX-License-Identifier: Apache-2.0

CC := aarch64-linux-gnu-gcc

BUILD := build
HOST_BUILD := $(BUILD)/host
HELLO := $(BUILD)/hello-aarch64
INITRAMFS := $(BUILD)/initramfs.cpio.gz
INIT := $(BUILD)/init-aarch64
KERNEL := $(BUILD)/kernel/Image
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

.PHONY: build test initramfs kernel-image qemu-system verify clean clean-all validate-agent host-build host-test host-supervisor-test mnist-assets host-mnist-test host-adaptive-supervisor-test full-verify optee-qemu-install optee-qemu-build optee-qemu-run optee-qemu-test optee-ta optee-ca optee-trustd

mnist-assets:
	./scripts/fetch-mnist-onnx.sh

build: mnist-assets
	mkdir -p $(BUILD)
	$(CC) -static -O2 -Wall -Wextra \
		-o $(HELLO) runtime/hello.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(INIT) initramfs/init.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(SUPERVISOR) runtime/supervisor.c runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(DEMO_MODEL) runtime/demo_model.c runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(RUNTIME_MANAGER) \
		runtime/runtime_manager_main.c \
		runtime/runtime_manager.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TRUSTD) \
		runtime/trustd.c \
		runtime/trusted_state.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TEARICTL) \
		runtime/tearictl.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(OPTD) \
		runtime/optd.c \
		runtime/optimizer_policy.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	$(CC) -O2 -Wall -Wextra \
		-I$(ORT_AARCH64_INCLUDE) \
		-o $(MNIST_MODEL) runtime/mnist_model.c runtime/telemetry.c \
		-L$(ORT_AARCH64_LIB) \
		-lonnxruntime \
		-Wl,-rpath,/usr/lib

host-build: mnist-assets
	mkdir -p $(HOST_BUILD)
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_HELLO) runtime/hello.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_SUPERVISOR) runtime/supervisor.c runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_DEMO_MODEL) runtime/demo_model.c runtime/telemetry.c
	gcc -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-Iexternal/onnxruntime/include \
		-o $(HOST_MNIST_MODEL) runtime/mnist_model.c runtime/telemetry.c \
		-Lexternal/onnxruntime/lib -lonnxruntime \
		-Wl,-rpath,'$$ORIGIN/../../external/onnxruntime/lib'
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_RUNTIME_MANAGER) \
		runtime/runtime_manager_main.c \
		runtime/runtime_manager.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TRUSTD) \
		runtime/trustd.c \
		runtime/trusted_state.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TEARICTL) \
		runtime/tearictl.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_OPTD) \
		runtime/optd.c \
		runtime/optimizer_policy.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c

test: build
	qemu-aarch64 ./$(HELLO)

host-mnist-test: host-build
	./$(HOST_MNIST_MODEL) --sample clean7 > $(HOST_BUILD)/mnist-clean7.log 2>&1
	grep -q "TEAR: MNIST workload start" $(HOST_BUILD)/mnist-clean7.log
	grep -q "TEAR: model_id=mnist-onnx-v1 backend=onnxruntime-cpu sample=clean7" $(HOST_BUILD)/mnist-clean7.log
	grep -q "TEAR: metric predicted_digit=" $(HOST_BUILD)/mnist-clean7.log
	grep -q "TEAR_EVENT .*event=mnist confidence_margin_x1000=" $(HOST_BUILD)/mnist-clean7.log
	grep -q "TEAR: MNIST workload finished" $(HOST_BUILD)/mnist-clean7.log
	./$(HOST_MNIST_MODEL) --sample weak7 > $(HOST_BUILD)/mnist-weak7.log 2>&1
	grep -q "TEAR: MNIST workload start" $(HOST_BUILD)/mnist-weak7.log
	grep -q "TEAR: model_id=mnist-onnx-v1 backend=onnxruntime-cpu sample=weak7" $(HOST_BUILD)/mnist-weak7.log
	grep -q "TEAR: metric predicted_digit=" $(HOST_BUILD)/mnist-weak7.log
	grep -q "TEAR_EVENT .*event=mnist confidence_margin_x1000=" $(HOST_BUILD)/mnist-weak7.log
	grep -q "TEAR: MNIST workload finished" $(HOST_BUILD)/mnist-weak7.log
	./$(HOST_MNIST_MODEL) --sample noise > $(HOST_BUILD)/mnist-noise.log 2>&1
	grep -q "TEAR: MNIST workload start" $(HOST_BUILD)/mnist-noise.log
	grep -q "TEAR: model_id=mnist-onnx-v1 backend=onnxruntime-cpu sample=noise" $(HOST_BUILD)/mnist-noise.log
	grep -q "TEAR: metric predicted_digit=" $(HOST_BUILD)/mnist-noise.log
	grep -q "TEAR_EVENT .*event=mnist confidence_margin_x1000=" $(HOST_BUILD)/mnist-noise.log
	grep -q "TEAR: MNIST workload finished" $(HOST_BUILD)/mnist-noise.log

initramfs: build
	rm -rf $(BUILD)/rootfs
	mkdir -p $(BUILD)/rootfs/bin $(BUILD)/rootfs/proc $(BUILD)/rootfs/tmp $(BUILD)/rootfs/etc/tear
	cp $(HELLO) $(BUILD)/rootfs/bin/tear-hello
	cp $(SUPERVISOR) $(BUILD)/rootfs/bin/tear-supervisor
	cp $(DEMO_MODEL) $(BUILD)/rootfs/bin/demo-model
	cp $(RUNTIME_MANAGER) $(BUILD)/rootfs/bin/tear-runtime-manager
	cp $(TRUSTD) $(BUILD)/rootfs/bin/tear-trustd
	cp $(TEARICTL) $(BUILD)/rootfs/bin/tearictl
	cp $(INIT) $(BUILD)/rootfs/init
	chmod +x $(BUILD)/rootfs/init
	mkdir -p $(BUILD)/rootfs/examples
	cp examples/model-v1.json examples/model-v2.json $(BUILD)/rootfs/examples/
	cp examples/model-v1.json examples/model-v2.json $(BUILD)/rootfs/etc/tear/
	cd $(BUILD)/rootfs && \
		find . | cpio --quiet -H newc -o | gzip > ../initramfs.cpio.gz

kernel-image:
	./scripts/fetch-qemu-kernel.sh $(KERNEL)

qemu-system: kernel-image initramfs
	./scripts/run-qemu-system.sh

verify:
	./scripts/verify-qemu-run.sh

clean:
	rm -rf $(BUILD)/rootfs
	rm -f $(HELLO) $(INIT) $(SUPERVISOR) $(DEMO_MODEL) $(RUNTIME_MANAGER) $(TRUSTD) $(TEARICTL) $(OPTD) $(MNIST_MODEL) $(INITRAMFS) $(BUILD)/telemetry.log
	rm -f $(BUILD)/optee-normal-world.log $(BUILD)/optee-secure-world.log
	rm -rf $(BUILD)/optee
	rm -rf $(BUILD)/host

clean-all:
	rm -rf $(BUILD)
	rm -rf $(OPTEE_QEMU_DIR)
	rm -rf external/onnxruntime
	rm -rf external/onnxruntime-aarch64

validate-agent: build test qemu-system verify

host-test: host-build
	./$(HOST_HELLO)

host-supervisor-test: host-build
	mkdir -p $(HOST_BUILD)
	ln -sf tear-trustd-host $(HOST_BUILD)/tear-trustd
	ln -sf tearictl-host $(HOST_BUILD)/tearictl
	ln -sf tear-runtime-manager-host $(HOST_BUILD)/tear-runtime-manager
	ln -sf demo-model-host $(HOST_BUILD)/demo-model
	PATH="$(abspath $(HOST_BUILD)):$$PATH" \
	./$(HOST_SUPERVISOR) --workload "$(abspath $(HOST_DEMO_MODEL))" > $(HOST_BUILD)/supervisor.log 2>&1
	grep -q "event=supervisor_start" $(HOST_BUILD)/supervisor.log
	grep -q "event=workload_start" $(HOST_BUILD)/supervisor.log
	grep -q "event=inference_done" $(HOST_BUILD)/supervisor.log
	grep -q "event=workload_exit" $(HOST_BUILD)/supervisor.log

host-adaptive-supervisor-test: host-build
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-trusted-decisions /tmp/tear-mnist-metrics
	TEAR_MNIST_SAMPLE=clean7 \
	./$(HOST_SUPERVISOR) --workload "$(abspath $(HOST_MNIST_MODEL))" --manifest examples/mnist-model.json --enable-optimizer > $(HOST_BUILD)/adaptive-clean7.log 2>&1
	grep -q "event=mnist_inference_metrics" /tmp/tear-mnist-metrics
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/adaptive-clean7.log
	grep -q "proposal=keep_current_profile decision=approved reason=policy_allows" /tmp/tear-trusted-decisions
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-trusted-decisions /tmp/tear-mnist-metrics
	TEAR_MNIST_SAMPLE=weak7 \
	./$(HOST_SUPERVISOR) --workload "$(abspath $(HOST_MNIST_MODEL))" --manifest examples/mnist-model.json --enable-optimizer > $(HOST_BUILD)/adaptive-weak7.log 2>&1
	grep -q "event=mnist_inference_metrics" /tmp/tear-mnist-metrics
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/adaptive-weak7.log
	grep -q "proposal=request_high_accuracy_profile decision=rejected reason=profile_unavailable" /tmp/tear-trusted-decisions
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-trusted-decisions /tmp/tear-mnist-metrics
	TEAR_MNIST_SAMPLE=noise \
	./$(HOST_SUPERVISOR) --workload "$(abspath $(HOST_MNIST_MODEL))" --manifest examples/mnist-model.json --enable-optimizer > $(HOST_BUILD)/adaptive-noise.log 2>&1
	grep -q "event=mnist_inference_metrics" /tmp/tear-mnist-metrics
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/adaptive-noise.log
	grep -q "proposal=reject_input decision=approved reason=input_rejected" /tmp/tear-trusted-decisions

full-verify: validate-agent host-test host-supervisor-test host-mnist-test host-adaptive-supervisor-test

optee-qemu-install: build optee-ta optee-ca optee-trustd
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)

optee-qemu-mnist-adaptive-install: build optee-ta optee-ca optee-trustd
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR) mnist-adaptive

optee-qemu-build: optee-qemu-install
	./scripts/optee-qemu.sh

optee-qemu-mnist-adaptive-build: optee-qemu-mnist-adaptive-install
	./scripts/optee-qemu.sh

optee-qemu-run:
	$(MAKE) -C $(OPTEE_QEMU_DIR)/build run-only

optee-qemu-mnist-adaptive-run:
	$(MAKE) -C $(OPTEE_QEMU_DIR)/build run-only

optee-qemu-test: optee-qemu-build
	./scripts/run-optee-qemu-headless.sh

optee-qemu-mnist-adaptive-test: optee-qemu-mnist-adaptive-build
	VERIFY_SCRIPT="$(abspath scripts/verify-optee-qemu-mnist-adaptive-run.sh)" \
	./scripts/run-optee-qemu-headless.sh

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
		-I$(OPTEE_CLIENT_INCLUDE) \
		-Ioptee/ta/tear_ta/include \
		-Ioptee/ca \
		-o $(TEAR_CA) \
		optee/ca/tear_ca.c \
		optee/ca/tear_optee_client.c \
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
		runtime/trustd.c \
		runtime/trusted_state.c \
		$(RUNTIME_PATHS_SRCS) \
		runtime/telemetry.c \
		optee/ca/tear_optee_client.c \
		-L$(OPTEE_CLIENT_LIB) \
		-Wl,-rpath-link,$(OPTEE_CLIENT_LIB) \
		-lteec
