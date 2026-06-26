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

.PHONY: build clean clean-all host-build host-test host-supervisor-test host-mnist-test host-adaptive-supervisor-test host-plan-test full-verify mnist-assets optee-qemu-install optee-qemu-build optee-qemu-run optee-qemu-test optee-ta optee-ca optee-trustd

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

host-test: host-build
	./$(HOST_HELLO)

host-supervisor-test: host-build
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-supervisor.sock /tmp/tear-metric-demo-model-demo-default-* $(HOST_BUILD)/tear-*-events.log*
	./$(HOST_SUPERVISOR) > $(HOST_BUILD)/supervisor.log 2>&1 & \
	    supervisor_pid=$$!; \
	    sleep 2; \
	    ./$(HOST_TEARICTL) run "$(abspath $(HOST_DEMO_MODEL))" examples/model-v2.json profiles/demo.profile > $(HOST_BUILD)/supervisor-client.log 2>&1; \
	    rc=$$?; \
	    kill -INT $$supervisor_pid; \
	    wait $$supervisor_pid || true; \
	    exit $$rc
	grep -q "event=supervisor_start" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "event=workload_start" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "event=inference_done" $(HOST_BUILD)/tear-runtime-manager-events.log-run-*
	grep -q "event=workload_exit" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "TEAR_METRIC .*profile_id=demo-default .*artifact_id=demo-model .*name=confidence_x100" /tmp/tear-metric-demo-model-demo-default-*

host-mnist-test: host-build
	rm -f /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-clean7 $(HOST_BUILD)/tear-mnist-model-events.log-host-clean7
	./$(HOST_MNIST_MODEL) --profile profiles/mnist.profile --run-id host-clean7 --sample clean7 > $(HOST_BUILD)/mnist-clean7.log 2>&1
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-clean7
	grep -q "event=mnist_inference_metrics" $(HOST_BUILD)/tear-mnist-model-events.log-host-clean7
	rm -f /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-weak7 $(HOST_BUILD)/tear-mnist-model-events.log-host-weak7
	./$(HOST_MNIST_MODEL) --profile profiles/mnist.profile --run-id host-weak7 --sample weak7 > $(HOST_BUILD)/mnist-weak7.log 2>&1
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-weak7
	grep -q "event=mnist_inference_metrics" $(HOST_BUILD)/tear-mnist-model-events.log-host-weak7
	rm -f /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-noise $(HOST_BUILD)/tear-mnist-model-events.log-host-noise
	./$(HOST_MNIST_MODEL) --profile profiles/mnist.profile --run-id host-noise --sample noise > $(HOST_BUILD)/mnist-noise.log 2>&1
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-host-noise
	grep -q "event=mnist_inference_metrics" $(HOST_BUILD)/tear-mnist-model-events.log-host-noise

host-adaptive-supervisor-test: host-build
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-supervisor.sock /tmp/tear-trusted-decisions /tmp/tear-metric-mnist-onnx-v1-mnist-default-* $(HOST_BUILD)/tear-*-events.log*
	./$(HOST_SUPERVISOR) > $(HOST_BUILD)/adaptive-clean7.log 2>&1 & \
	    supervisor_pid=$$!; \
	    sleep 2; \
	    ./$(HOST_TEARICTL) run "$(abspath $(HOST_MNIST_MODEL))" examples/mnist-model.json profiles/mnist.profile -- --sample clean7 > $(HOST_BUILD)/adaptive-clean7-client.log 2>&1; \
	    rc=$$?; \
	    kill -INT $$supervisor_pid; \
	    wait $$supervisor_pid || true; \
	    exit $$rc
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-*
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/tear-runtime-manager-events.log
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=keep_current_profile decision=approved reason=policy_allows" /tmp/tear-trusted-decisions
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-supervisor.sock /tmp/tear-trusted-decisions /tmp/tear-metric-mnist-onnx-v1-mnist-default-* $(HOST_BUILD)/tear-*-events.log*
	./$(HOST_SUPERVISOR) > $(HOST_BUILD)/adaptive-weak7.log 2>&1 & \
	    supervisor_pid=$$!; \
	    sleep 2; \
	    ./$(HOST_TEARICTL) run "$(abspath $(HOST_MNIST_MODEL))" examples/mnist-model.json profiles/mnist.profile -- --sample weak7 > $(HOST_BUILD)/adaptive-weak7-client.log 2>&1; \
	    rc=$$?; \
	    kill -INT $$supervisor_pid; \
	    wait $$supervisor_pid || true; \
	    exit $$rc
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-*
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/tear-runtime-manager-events.log
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=request_high_accuracy_profile decision=rejected reason=profile_unavailable" /tmp/tear-trusted-decisions
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-supervisor.sock /tmp/tear-trusted-decisions /tmp/tear-metric-mnist-onnx-v1-mnist-default-* $(HOST_BUILD)/tear-*-events.log*
	./$(HOST_SUPERVISOR) > $(HOST_BUILD)/adaptive-noise.log 2>&1 & \
	    supervisor_pid=$$!; \
	    sleep 2; \
	    ./$(HOST_TEARICTL) run "$(abspath $(HOST_MNIST_MODEL))" examples/mnist-model.json profiles/mnist.profile -- --sample noise > $(HOST_BUILD)/adaptive-noise-client.log 2>&1; \
	    rc=$$?; \
	    kill -INT $$supervisor_pid; \
	    wait $$supervisor_pid || true; \
	    exit $$rc
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-*
	grep -q "event=optimizer_proposal_received" $(HOST_BUILD)/tear-runtime-manager-events.log
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=reject_input decision=approved reason=input_rejected" /tmp/tear-trusted-decisions

host-plan-test: host-build
	rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-supervisor.sock /tmp/tear-trusted-decisions /tmp/tear-metric-* $(HOST_BUILD)/tear-*-events.log*
	./$(HOST_SUPERVISOR) > $(HOST_BUILD)/plan.log 2>&1 & \
	    supervisor_pid=$$!; \
	    sleep 2; \
	    ./$(HOST_TEARICTL) run-plan $(HOST_PLAN) > $(HOST_BUILD)/plan-client.log 2>&1; \
	    rc=$$?; \
	    kill -INT $$supervisor_pid; \
	    wait $$supervisor_pid || true; \
	    exit $$rc
	grep -q "component=supervisor event=run_plan_start" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "component=supervisor event=workload_selected" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "component=supervisor event=run_plan_done" $(HOST_BUILD)/tear-supervisor-events.log
	grep -q "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default-*
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=keep_current_profile decision=approved reason=policy_allows" /tmp/tear-trusted-decisions
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=request_high_accuracy_profile decision=rejected reason=profile_unavailable" /tmp/tear-trusted-decisions
	grep -Eq "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=reject_input decision=approved reason=input_rejected" /tmp/tear-trusted-decisions

full-verify: host-test host-supervisor-test host-mnist-test host-adaptive-supervisor-test host-plan-test

optee-qemu-install: build optee-ta optee-ca optee-trustd
	./scripts/install-optee-qemu-files.sh $(OPTEE_QEMU_DIR)

optee-qemu-build: optee-qemu-install
	./scripts/optee-qemu.sh

optee-qemu-run:
	$(MAKE) -C $(OPTEE_QEMU_DIR)/build run-only

optee-qemu-test: optee-qemu-build
	./scripts/run-optee-qemu-headless.sh

optee-qemu-mnist-adaptive-test: optee-qemu-test

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
