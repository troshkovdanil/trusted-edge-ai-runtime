#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOG="${LOG:-$ROOT_DIR/build/optee-normal-world.log}"
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

echo "TEAR verify: OP-TEE CA ping"

reset_order_check

echo "TEAR verify: OP-TEE CA_TEST"
check_log_ordered "TEAR_OPTEE_CA_TEST start"
check_log_ordered "TEAR_OPTEE_CA_PING_OK"
check_log_ordered "TEAR_OPTEE_CA_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_SELF_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_SELF_TEST start"
check_log_ordered "event=trustd_optee_backend_ping_ok"
check_log_ordered "TEAR_OPTEE_TRUSTD_SELF_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_ENROLL_SELF_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
check_log_ordered "event=trustd_optee_backend_enroll_ok"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_VERIFY_SELF_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
check_log_ordered "event=trustd_optee_backend_enroll_before_verify_ok"
check_log_ordered "event=trustd_optee_backend_verify_ok"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_ENROLL_SOCKET_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=optee_model_enroll"
check_log_ordered "event=tearictl_enroll_done"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_VERIFY_SOCKET_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=optee_model_enroll"
check_log_ordered "event=tearictl_enroll_done"
check_log_ordered "event=optee_model_verify_ok"
check_log_ordered "event=tearictl_verify_done"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST done"

echo "TEAR verify: OP-TEE TRUSTD_UPDATE_SOCKET_TEST"
check_log_ordered "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=optee_model_enroll"
check_log_ordered "event=tearictl_enroll_done"
check_log_ordered "event=optee_model_update_ok"
check_log_ordered "event=tearictl_update_model_done"
check_log_ordered "event=optee_model_update_rejected"
check_log_ordered "event=tearictl_update_model_failed"
check_log_ordered "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

echo "TEAR verify: mock model workload"
check_log_ordered "TEAR_OPTEE_QEMU_TEST start"
check_log_ordered "event=supervisor_start"
check_log_ordered "event=trustd_start"
check_log_ordered "event=provisioning_start"
check_log_ordered "event=model_enroll"
check_log_ordered "event=tearictl_enroll_done"
check_log_ordered "event=provisioning_done"
check_log_ordered "STATE demo-model 1 mock sha256-demo-model-v1"
check_log_ordered "event=tearictl_report_done"
check_log_ordered "event=provisioning_report_done"
check_log_ordered "event=model_update_start"
check_log_ordered "event=model_update_ok"
check_log_ordered "event=tearictl_update_model_done"
check_log_ordered "event=model_update_done"
check_log_ordered "event=rollback_validation_start"
check_log_ordered "event=model_rollback_rejected"
check_log_ordered "event=tearictl_update_model_failed"
check_log_ordered "event=rollback_validation_done"
check_log_ordered "event=workload_start"
check_log_ordered "event=runtime_manager_start"
check_log_ordered "event=manifest_loaded"
check_log_ordered "model_id=demo-model"
check_log_ordered "version=2"
check_log_ordered "backend=mock"
check_log_ordered "model_hash=sha256-demo-model-v2"
check_log_ordered "event=model_verify_ok"
check_log_ordered "event=manifest_verified"
check_log_ordered "event=model_init"
check_log_ordered "event=inference_start"
check_log_ordered "event=inference_done"
check_log_ordered "TEAR model: result=object:box confidence=0.87"
check_log_ordered "event=runtime_workload_exit status=0"
check_log_ordered "event=runtime_manager_shutdown"
check_log_ordered "event=workload_exit status=0"
check_log_ordered "event=supervisor_shutdown"

echo "TEAR verify: all checks passed"
