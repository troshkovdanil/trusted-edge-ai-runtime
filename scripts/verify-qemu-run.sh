#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

LOG="${LOG:-build/telemetry.log}"

check_log_contains() {
    local pattern="$1"

    if ! grep -q "$pattern" "$LOG"; then
        echo "error: expected pattern not found: $pattern"
        echo "---- telemetry log ----"
        cat "$LOG"
        echo "-----------------------"
        exit 1
    fi
}

echo "TEAR verify: default hello workload"
QUIET=1 make --silent qemu-system

check_log_contains "TEAR: selected workload: /bin/tear-hello"
check_log_contains "event=runtime_manager_start"
check_log_contains "event=manifest_loaded"
check_log_contains "model_id=demo-model"
check_log_contains "version=1"
check_log_contains "backend=mock"
check_log_contains "model_hash=sha256-demo-model-v1"
check_log_contains "TEAR: hello from aarch64 qemu"
check_log_contains "event=runtime_workload_exit status=0"
check_log_contains "event=runtime_manager_shutdown"
check_log_contains "event=workload_exit status=0"

echo "TEAR verify: mock model workload"
QUIET=1 WORKLOAD=/bin/demo-model make --silent qemu-system

check_log_contains "TEAR: selected workload: /bin/demo-model"
check_log_contains "event=runtime_manager_start"
check_log_contains "event=manifest_loaded"
check_log_contains "model_id=demo-model"
check_log_contains "version=1"
check_log_contains "backend=mock"
check_log_contains "model_hash=sha256-demo-model-v1"
check_log_contains "event=model_init"
check_log_contains "event=inference_start"
check_log_contains "event=inference_done"
check_log_contains "TEAR model: result=object:box confidence=0.87"
check_log_contains "event=runtime_workload_exit status=0"
check_log_contains "event=runtime_manager_shutdown"
check_log_contains "event=workload_exit status=0"

echo "TEAR verify: all checks passed"
