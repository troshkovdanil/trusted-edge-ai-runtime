#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NORMAL_LOG="$ROOT_DIR/build/optee-normal-world.log"

echo "TEAR verify: OP-TEE QEMU host log"
echo "TEAR verify: log = $NORMAL_LOG"

check_log() {
    local pattern="$1"

    if ! grep -q "$pattern" "$NORMAL_LOG"; then
        echo
        echo "error: expected pattern not found: $pattern"
        echo "Full guest log:"
        echo "  $NORMAL_LOG"
        echo
        echo "---- normal-world log ----"
        cat "$NORMAL_LOG"
        echo "--------------------------"
        exit 1
    fi
}

check_log "TEAR_OPTEE_CA_TEST start"
check_log "TEAR_OPTEE_CA_TEST done"

check_log "TEAR_OPTEE_TRUSTD_SELF_TEST start"
check_log "TEAR_OPTEE_TRUSTD_SELF_TEST done"

check_log "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
check_log "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

check_log "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
check_log "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

check_log "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST start"
check_log "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

check_log "TEAR_OPTEE_QEMU_RUN_PLAN_TEST start"
check_log "TEAR_OPTEE_QEMU_RUN_PLAN_TEST done"

check_log "TEAR_QEMU_GUEST_VERIFY start"
check_log "TEAR_QEMU_GUEST_VERIFY_OK"

echo "TEAR verify: PASS"
