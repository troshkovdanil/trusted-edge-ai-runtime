#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

LOG="${LOG:-build/telemetry.log}"
LAST_LINE=0

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

check_log_ordered() {
    local pattern="$1"
    local line

    line="$(grep -n "$pattern" "$LOG" | head -n1 | cut -d: -f1 || true)"

    if [[ -z "$line" ]]; then
        echo "error: expected pattern not found: $pattern"
        echo "---- telemetry log ----"
        cat "$LOG"
        echo "-----------------------"
        exit 1
    fi

    if (( line <= LAST_LINE )); then
        echo "error: pattern found out of order: $pattern"
        echo "previous line: $LAST_LINE"
        echo "current line:  $line"
        echo "---- telemetry log ----"
        cat "$LOG"
        echo "-----------------------"
        exit 1
    fi

    LAST_LINE="$line"
}

reset_order_check() {
    LAST_LINE=0
}

echo "TEAR verify: default hello workload"
QUIET=1 make --silent qemu-system

reset_order_check
check_log_ordered "TEAR: selected workload: /bin/tear-hello"
check_log_ordered "event=supervisor_start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=workload_start"
check_log_ordered "event=runtime_manager_start"
check_log_ordered "event=manifest_loaded"
check_log_ordered "model_id=demo-model"
check_log_ordered "version=1"
check_log_ordered "backend=mock"
check_log_ordered "model_hash=sha256-demo-model-v1"
check_log_ordered "event=model_enroll"
check_log_ordered "event=trust_enroll_done"
check_log_ordered "STATE demo-model 1 mock sha256-demo-model-v1"
check_log_ordered "event=trust_report_done"
check_log_ordered "TEAR: hello from aarch64 qemu"
check_log_ordered "event=runtime_workload_exit status=0"
check_log_ordered "event=runtime_manager_shutdown"
check_log_ordered "event=workload_exit status=0"
check_log_ordered "event=supervisor_shutdown"

echo "TEAR verify: mock model workload"
QUIET=1 WORKLOAD=/bin/demo-model make --silent qemu-system

reset_order_check
check_log_ordered "TEAR: selected workload: /bin/demo-model"
check_log_ordered "event=supervisor_start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=workload_start"
check_log_ordered "event=runtime_manager_start"
check_log_ordered "event=manifest_loaded"
check_log_ordered "model_id=demo-model"
check_log_ordered "version=1"
check_log_ordered "backend=mock"
check_log_ordered "model_hash=sha256-demo-model-v1"
check_log_ordered "event=model_enroll"
check_log_ordered "event=trust_enroll_done"
check_log_ordered "STATE demo-model 1 mock sha256-demo-model-v1"
check_log_ordered "event=trust_report_done"
check_log_ordered "event=model_init"
check_log_ordered "event=inference_start"
check_log_ordered "event=inference_done"
check_log_ordered "TEAR model: result=object:box confidence=0.87"
check_log_ordered "event=runtime_workload_exit status=0"
check_log_ordered "event=runtime_manager_shutdown"
check_log_ordered "event=workload_exit status=0"
check_log_ordered "event=supervisor_shutdown"

echo "TEAR verify: all checks passed"
