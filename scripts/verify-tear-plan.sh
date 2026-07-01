#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

PLATFORM="${1:?usage: verify-tear-plan.sh <host-mock|qemu-optee>}"
MARKER="TEAR_PLAN_EVENT_METRIC_VERIFY_OK"

case "$PLATFORM" in
host-mock)
    LOG="$ROOT_DIR/build/platforms/host-mock/host-mock-verify.log"
    ;;

qemu-optee)
    LOG="$ROOT_DIR/build/optee-normal-world.log"
    ;;

*)
    echo "TEAR verify: unknown platform: $PLATFORM" >&2
    echo "usage: verify-tear-plan.sh <host-mock|qemu-optee>" >&2
    exit 1
    ;;
esac

echo "TEAR verify: platform=$PLATFORM"
echo "TEAR verify: log=$LOG"

if [ ! -f "$LOG" ]; then
    echo "error: expected verification log not found: $LOG"
    exit 1
fi

if ! grep -q "$MARKER" "$LOG"; then
    echo
    echo "error: expected marker not found: $MARKER"
    echo "log: $LOG"
    echo
    echo "---- log ----"
    cat "$LOG"
    echo "-------------"
    exit 1
fi

echo "TEAR verify: PASS"
