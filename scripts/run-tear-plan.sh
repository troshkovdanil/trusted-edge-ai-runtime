#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

PLATFORM="${1:?usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>}"
PLAN="${2:?usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>}"

case "$PLATFORM" in
host-demo)
    echo "TEAR: platform=host-demo"
    echo "TEAR: plan=$PLAN"

    mkdir -p "$ROOT_DIR/build/host"

    rm -f \
        /tmp/tear-trustd.sock \
        /tmp/tear-optd.sock \
        /tmp/tear-supervisor.sock \
        /tmp/tear-trusted-decisions \
        /tmp/tear-metric-* \
        "$ROOT_DIR"/build/host/tear-*-events.log*

    "$ROOT_DIR"/build/host/tear-supervisor-host \
        > "$ROOT_DIR"/build/host/plan.log 2>&1 &
    supervisor_pid=$!

    cleanup() {
        kill -INT "$supervisor_pid" 2>/dev/null || true
        wait "$supervisor_pid" 2>/dev/null || true
    }

    trap cleanup EXIT

    for _ in 1 2 3 4 5; do
        [ -S /tmp/tear-supervisor.sock ] && break
        sleep 1
    done

    "$ROOT_DIR"/build/host/tearictl-host run-plan "$PLAN"

    echo "TEAR: host-demo plan completed"
    ;;

qemu-optee)
    echo "TEAR: platform=qemu-optee"
    echo "TEAR: plan=$PLAN"

    if [ ! -f "$PLAN" ]; then
        echo "TEAR: plan not found: $PLAN" >&2
        exit 1
    fi

    OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"
    TARGET="$ROOT_DIR/$OPTEE_QEMU_DIR/out-br/target"

    mkdir -p "$TARGET/etc/tear"
    cp -v "$PLAN" "$TARGET/etc/tear/active.plan"

    TEAR_QEMU_PLAN="/etc/tear/active.plan" \
    VERIFY_SCRIPT="$ROOT_DIR/scripts/verify-optee-qemu-run.sh" \
        "$ROOT_DIR/scripts/run-optee-qemu-headless.sh"

    echo "TEAR: qemu-optee plan completed"
    ;;

*)
    echo "TEAR: unknown platform: $PLATFORM" >&2
    echo "usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>" >&2
    exit 1
    ;;
esac
