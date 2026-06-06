#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOG="${LOG:-$ROOT_DIR/build/optee-normal-world.log}"
LAST_LINE=0

check_log_ordered() {
    local pattern="$1"
    local line

    line="$(
    awk -v pat="$pattern" -v last="$LAST_LINE" '
        NR > last && $0 ~ pat { print NR; exit }
    ' "$LOG"
    )"

    if [[ -z "$line" ]]; then
        echo "error: expected pattern not found: $pattern"
        echo "---- telemetry log ----"
        cat "$LOG"
        echo "-----------------------"
        exit 1
    fi

    LAST_LINE="$line"
}

echo "TEAR verify: OP-TEE adaptive MNIST workload"

check_log_ordered "TEAR_OPTEE_MNIST_ADAPTIVE_TEST start"
check_log_ordered "event=supervisor_start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=provisioning_start"
check_log_ordered "event=optee_model_enroll"
check_log_ordered "event=tearictl_enroll_done"
check_log_ordered "event=provisioning_done"
check_log_ordered "STATE mnist-onnx-v1 1 onnxruntime-cpu sha256-mnist-onnx-v1"
check_log_ordered "event=provisioning_report_done"
check_log_ordered "event=workload_start"
check_log_ordered "event=runtime_manager_start"
check_log_ordered "event=manifest_loaded"
check_log_ordered "model_id=mnist-onnx-v1"
check_log_ordered "backend=onnxruntime-cpu"
check_log_ordered "optimization_capable=true"
check_log_ordered "event=optee_model_verify_ok"
check_log_ordered "event=manifest_verified"
check_log_ordered "TEAR: MNIST workload start"
check_log_ordered "TEAR: model_id=mnist-onnx-v1 backend=onnxruntime-cpu sample=weak7"
check_log_ordered "TEAR: MNIST workload finished"
check_log_ordered "event=runtime_workload_exit status=0"
check_log_ordered "event=optd_proposal"
check_log_ordered "event=optimizer_proposal_received"
check_log_ordered "event=request_high_accuracy_profile"
check_log_ordered "event=rejected"
check_log_ordered "event=profile_unavailable"
check_log_ordered "event=optimization_decision_recorded"
check_log_ordered "event=optimization_decision_recorded_by_runtime_manager"
check_log_ordered "event=runtime_manager_shutdown"
check_log_ordered "event=workload_exit status=0"
check_log_ordered "event=supervisor_shutdown"

echo "TEAR verify: OP-TEE adaptive MNIST checks passed"
