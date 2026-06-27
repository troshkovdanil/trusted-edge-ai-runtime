#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

PLATFORM="${1:?usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>}"
PLAN="${2:?usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>}"

cd "$ROOT_DIR"

if [ ! -f "$PLAN" ]; then
    echo "TEAR: plan not found: $PLAN" >&2
    exit 1
fi

case "$PLATFORM" in
host-demo)
    echo "TEAR: platform=host-demo"
    echo "TEAR: plan=$PLAN"

    "$ROOT_DIR/scripts/run-host-demo.sh" "$PLAN"

    echo "TEAR: host-demo plan completed"
    ;;

qemu-optee)
    echo "TEAR: platform=qemu-optee"
    echo "TEAR: plan=$PLAN"

    OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"
    TARGET="$ROOT_DIR/$OPTEE_QEMU_DIR/out-br/target"

    mkdir -p "$TARGET/etc/tear"
    cp -v "$PLAN" "$TARGET/etc/tear/active.plan"

    TEAR_QEMU_PLAN="/etc/tear/active.plan" \
    VERIFY_SCRIPT="$ROOT_DIR/scripts/verify-qemu-optee.sh" \
        "$ROOT_DIR/scripts/run-qemu-optee.sh"

    echo "TEAR: qemu-optee plan completed"
    ;;

*)
    echo "TEAR: unknown platform: $PLATFORM" >&2
    echo "usage: run-tear-plan.sh <host-demo|qemu-optee> <plan>" >&2
    exit 1
    ;;
esac
