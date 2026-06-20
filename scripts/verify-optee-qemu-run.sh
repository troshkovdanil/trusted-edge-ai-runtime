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
        echo "---- normal-world log ----"
        cat "$LOG"
        echo "--------------------------"
        exit 1
    fi

    LAST_LINE="$line"
}

echo "TEAR verify: OP-TEE QEMU host log"

check_log_ordered "TEAR_OPTEE_CA_TEST start"
check_log_ordered "TEAR_OPTEE_CA_PING_OK"
check_log_ordered "TEAR_OPTEE_CA_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_SELF_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_SELF_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST done"

check_log_ordered "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST start"
check_log_ordered "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

check_log_ordered "TEAR_OPTEE_QEMU_TEST start"
check_log_ordered "TEAR_OPTEE_QEMU_TEST exit=0"
check_log_ordered "TEAR_QEMU_GUEST_VERIFY start"
check_log_ordered "TEAR_QEMU_GUEST_VERIFY_OK"

echo "TEAR verify: OP-TEE QEMU host checks passed"
