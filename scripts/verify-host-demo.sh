#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
HOST_BUILD="$ROOT_DIR/build/host"

SUPERVISOR_EVENTS="$HOST_BUILD/tear-supervisor-events.log"
RUNTIME_MANAGER_EVENTS="$HOST_BUILD/tear-runtime-manager-events.log"
WORKLOAD_EVENTS="$HOST_BUILD/tear-runtime-manager-events.log-run-*"
DECISIONS="/tmp/tear-trusted-decisions"

echo "TEAR verify: host-demo events"
echo "TEAR verify: supervisor events = $SUPERVISOR_EVENTS"
echo "TEAR verify: runtime events = $RUNTIME_MANAGER_EVENTS"

check_file_contains() {
    file="$1"
    pattern="$2"

    if [ ! -f "$file" ]; then
        echo "error: expected file not found: $file"
        exit 1
    fi

    if ! grep -Eq "$pattern" "$file"; then
        echo
        echo "error: expected pattern not found: $pattern"
        echo "file: $file"
        echo
        echo "---- log ----"
        cat "$file"
        echo "-------------"
        exit 1
    fi
}

check_glob_contains() {
    glob="$1"
    pattern="$2"
    found=0

    for file in $glob; do
        [ -f "$file" ] || continue

        if grep -Eq "$pattern" "$file"; then
            found=1
            break
        fi
    done

    if [ "$found" -ne 1 ]; then
        echo
        echo "error: expected pattern not found in $glob: $pattern"
        exit 1
    fi
}

check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_daemon_ready"
check_file_contains "$SUPERVISOR_EVENTS" "event=run_plan_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_selected"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_exit status=0"
check_file_contains "$SUPERVISOR_EVENTS" "event=run_plan_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_daemon_shutdown"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_shutdown"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_start"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=demo-model"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=mnist-onnx-v1"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimizer_proposal_received"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_shutdown"

check_glob_contains "$WORKLOAD_EVENTS" "event=model_init"
check_glob_contains "$WORKLOAD_EVENTS" "event=mnist_inference_metrics"

check_glob_contains "/tmp/tear-metric-demo-model-demo-default-*" \
    "TEAR_METRIC .*name=confidence_x100"

check_glob_contains "/tmp/tear-metric-mnist-onnx-v1-mnist-default-*" \
    "TEAR_METRIC .*name=confidence_margin_x1000"

check_file_contains "$DECISIONS" "proposal=keep_current_profile decision=approved"
check_file_contains "$DECISIONS" "proposal=request_high_accuracy_profile decision=rejected"
check_file_contains "$DECISIONS" "proposal=reject_input decision=approved"

echo "TEAR verify: PASS"
