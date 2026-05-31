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

HOST_HELLO := $(HOST_BUILD)/hello-host
HOST_SUPERVISOR := $(HOST_BUILD)/tear-supervisor-host
HOST_DEMO_MODEL := $(HOST_BUILD)/demo-model-host
HOST_MNIST_MODEL := $(HOST_BUILD)/mnist-model-host
HOST_RUNTIME_MANAGER := $(HOST_BUILD)/tear-runtime-manager-host
HOST_TRUSTD := $(HOST_BUILD)/tear-trustd-host
HOST_TEARICTL := $(HOST_BUILD)/tearictl-host

MNIST_MODEL_FILE := models/mnist/mnist.onnx
ORT_INCLUDE := external/onnxruntime/include/onnxruntime_c_api.h

.PHONY: build test initramfs kernel-image qemu-system verify clean clean-all validate-agent host-build host-test host-supervisor-test mnist-assets host-mnist-test full-verify

mnist-assets:
	./scripts/fetch-mnist-onnx.sh

build:
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
		runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TRUSTD) \
		runtime/trustd.c \
		runtime/trusted_state.c \
		runtime/telemetry.c
	$(CC) -static -O2 -Wall -Wextra \
		-o $(TEARICTL) \
		runtime/tearictl.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		runtime/telemetry.c

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
		runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TRUSTD) \
		runtime/trustd.c \
		runtime/trusted_state.c \
		runtime/telemetry.c
	gcc -static -O2 -Wall -Wextra -DTEAR_HOST_BUILD \
		-o $(HOST_TEARICTL) \
		runtime/tearictl.c \
		runtime/model_manifest.c \
		runtime/trust_client.c \
		runtime/telemetry.c

test: build
	qemu-aarch64 ./$(HELLO)

host-mnist-test: host-build
	./$(HOST_MNIST_MODEL) > $(HOST_BUILD)/mnist.log 2>&1
	grep -q "TEAR: MNIST workload start" $(HOST_BUILD)/mnist.log
	grep -q "TEAR: model_id=mnist-onnx-v1 backend=onnxruntime-cpu" $(HOST_BUILD)/mnist.log
	grep -q "TEAR: predicted_digit=" $(HOST_BUILD)/mnist.log
	grep -q "TEAR: MNIST workload finished" $(HOST_BUILD)/mnist.log

full-verify: validate-agent host-test host-supervisor-test host-mnist-test

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
	rm -f $(HELLO) $(INIT) $(SUPERVISOR) $(DEMO_MODEL) $(RUNTIME_MANAGER) $(TRUSTD) $(TEARICTL) $(INITRAMFS) $(BUILD)/telemetry.log

clean-all:
	rm -rf $(BUILD)

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
