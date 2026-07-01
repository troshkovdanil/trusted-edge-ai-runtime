#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PLAN="${1:-plans/host-mock.plan}"

HOST_BUILD="$ROOT_DIR/build/platforms/host-mock"
SUPERVISOR="$HOST_BUILD/tear-supervisor-host"
TEARICTL="$HOST_BUILD/tearictl-host"

SUPERVISOR_LOG="$HOST_BUILD/host-mock-supervisor.log"
CLIENT_LOG="$HOST_BUILD/host-mock-client.log"
EVENT_METRIC_VERIFY_LOG="$HOST_BUILD/host-mock-verify.log"
VERIFY_SCRIPT="${VERIFY_SCRIPT:-$ROOT_DIR/scripts/verify-tear-plan.sh}"

cd "$ROOT_DIR"

mkdir -p "$HOST_BUILD"

rm -f /tmp/tear-trustd.sock \
      /tmp/tear-optd.sock \
      /tmp/tear-supervisor.sock \
      /tmp/tear-trusted-decisions \
      /tmp/tear-metric-* \
      "$HOST_BUILD"/tear-*-events.log* \
      "$SUPERVISOR_LOG" \
      "$CLIENT_LOG" \
      "$EVENT_METRIC_VERIFY_LOG"

echo "TEAR: host-mock plan: $PLAN"
echo "TEAR: supervisor log: $SUPERVISOR_LOG"
echo "TEAR: client log: $CLIENT_LOG"
echo "TEAR: verify log: $EVENT_METRIC_VERIFY_LOG"
echo "TEAR: host-mock test running..."

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

{
    echo "TEAR_HOST_MOCK_PROVISION_TEST start"

    "$TEARICTL" provision examples/model-v1.json
    "$TEARICTL" update-model examples/model-v2.json

    if "$TEARICTL" update-model examples/model-v1.json; then
        echo "TEAR_HOST_MOCK_PROVISION_TEST rollback accepted unexpectedly"
        exit 1
    fi

    "$TEARICTL" provision-plan "$PLAN"

    echo "TEAR_HOST_MOCK_PROVISION_TEST done"

    echo "TEAR_HOST_MOCK_RUN_PLAN_TEST start"

    "$TEARICTL" run-plan "$PLAN"

    echo "TEAR_HOST_MOCK_RUN_PLAN_TEST done"
} > "$CLIENT_LOG" 2>&1

cleanup
trap - EXIT INT TERM

TEAR_LOG_DIR="$HOST_BUILD" \
TEAR_DECISION_LOG="/tmp/tear-trusted-decisions" \
"$ROOT_DIR/scripts/verify-tear-plan-event-metric.sh" \
    > "$EVENT_METRIC_VERIFY_LOG" 2>&1

echo "TEAR: host-mock test running... OK"

echo "TEAR: running host-side verification..."
"$VERIFY_SCRIPT" host-mock

echo "TEAR: host-mock test passed"
