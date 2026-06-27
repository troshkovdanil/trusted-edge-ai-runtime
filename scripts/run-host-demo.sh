#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PLAN="${1:-plans/host-demo.plan}"

HOST_BUILD="$ROOT_DIR/build/host"
SUPERVISOR="$HOST_BUILD/tear-supervisor-host"
TEARICTL="$HOST_BUILD/tearictl-host"

SUPERVISOR_LOG="$HOST_BUILD/host-demo-supervisor.log"
CLIENT_LOG="$HOST_BUILD/host-demo-client.log"
VERIFY_SCRIPT="${VERIFY_SCRIPT:-$ROOT_DIR/scripts/verify-host-demo.sh}"

cd "$ROOT_DIR"

mkdir -p "$HOST_BUILD"

rm -f /tmp/tear-trustd.sock \
      /tmp/tear-optd.sock \
      /tmp/tear-supervisor.sock \
      /tmp/tear-trusted-decisions \
      /tmp/tear-metric-* \
      "$HOST_BUILD"/tear-*-events.log* \
      "$SUPERVISOR_LOG" \
      "$CLIENT_LOG"

echo "TEAR: host-demo plan: $PLAN"
echo "TEAR: supervisor log: $SUPERVISOR_LOG"
echo "TEAR: client log: $CLIENT_LOG"
echo "TEAR: host-demo test running..."

"$SUPERVISOR" > "$SUPERVISOR_LOG" 2>&1 &
supervisor_pid=$!

cleanup() {
    if kill -0 "$supervisor_pid" 2>/dev/null; then
        kill -INT "$supervisor_pid"
        wait "$supervisor_pid" || true
    fi
}

trap cleanup EXIT INT TERM

for _ in 1 2 3 4 5; do
    [ -S /tmp/tear-supervisor.sock ] && break
    sleep 1
done

if [ ! -S /tmp/tear-supervisor.sock ]; then
    echo "TEAR: supervisor socket did not appear"
    echo "TEAR: inspect log: $SUPERVISOR_LOG"
    exit 1
fi

"$TEARICTL" run-plan "$PLAN" > "$CLIENT_LOG" 2>&1

cleanup
trap - EXIT INT TERM

echo "TEAR: host-demo test running... OK"

echo "TEAR: running host-side verification..."
"$VERIFY_SCRIPT"

echo "TEAR: host-demo test passed"
