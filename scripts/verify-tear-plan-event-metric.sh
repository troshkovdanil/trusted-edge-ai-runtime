#!/usr/bin/env sh
set -eu

check_file_contains() {
    file="$1"
    pattern="$2"

    if [ ! -f "$file" ]; then
        echo "error: expected file not found: $file"
        exit 1
    fi

    if ! grep -Eq "$pattern" "$file"; then
        echo "error: expected pattern not found: $pattern"
        echo "---- $file ----"
        cat "$file"
        echo "--------------"
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
        echo "error: expected pattern not found in $glob: $pattern"
        for file in $glob; do
            [ -f "$file" ] || continue
            echo "---- $file ----"
            cat "$file"
            echo "--------------"
        done
        exit 1
    fi
}

TEAR_LOG_DIR="${TEAR_LOG_DIR:-/tmp}"

SUPERVISOR_EVENTS="$TEAR_LOG_DIR/tear-supervisor-events.log"
TRUSTD_EVENTS="$TEAR_LOG_DIR/tear-trustd-events.log"
OPTD_EVENTS="$TEAR_LOG_DIR/tear-optd-events.log"
RUNTIME_MANAGER_EVENTS="$TEAR_LOG_DIR/tear-runtime-manager-events.log"
WORKLOAD_EVENTS="$TEAR_LOG_DIR/tear-runtime-manager-events.log-run-*"
REPORTED_DECISION="${TEAR_DECISION_LOG:-/tmp/tear-reported-decision.log}"

DEMO_METRICS="${TEAR_DEMO_METRICS:-/tmp/tear-metric-demo-model-demo-default-*}"
MNIST_METRICS="${TEAR_MNIST_METRICS:-/tmp/tear-metric-mnist-onnx-v1-mnist-default-*}"

echo "TEAR_PLAN_EVENT_METRIC_VERIFY start"

check_file_contains "$TRUSTD_EVENTS" "event=trustd_start"
check_file_contains "$TRUSTD_EVENTS" "event=.*model_enroll"
check_file_contains "$TRUSTD_EVENTS" "event=.*model_verify_ok"
check_file_contains "$TRUSTD_EVENTS" "event=.*record_decision_ok"
check_file_contains "$TRUSTD_EVENTS" "event=optimization_decision_recorded"

check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_daemon_ready"
check_file_contains "$SUPERVISOR_EVENTS" "event=run_plan_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_selected"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_report_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_exit status=0"
check_file_contains "$SUPERVISOR_EVENTS" "event=run_plan_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_daemon_shutdown"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_shutdown"

check_file_contains "$OPTD_EVENTS" "event=optd_start"
check_file_contains "$OPTD_EVENTS" "event=optd_proposal"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_start"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_loaded"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=demo-model"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=mnist-onnx-v1"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_loaded"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "component=platform_adapter event=platform_detected"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "component=platform_adapter event=platform_(optee_present|optee_absent)"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "component=platform_adapter .*event=platform_profile_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "component=platform_adapter .*event=platform_backend_available"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=run-[0-9]+-[0-9]+"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_workload_exit status=0"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimizer_proposal_received"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=keep_current_profile"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=request_high_accuracy_profile"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=reject_input"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimization_decision_recorded_by_runtime_manager"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimization_decision_reported_by_runtime_manager"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_shutdown"

check_glob_contains "$WORKLOAD_EVENTS" "event=model_init"
check_glob_contains "$WORKLOAD_EVENTS" "event=inference_start"
check_glob_contains "$WORKLOAD_EVENTS" "event=inference_done"
check_glob_contains "$WORKLOAD_EVENTS" "event=model_shutdown"
check_glob_contains "$WORKLOAD_EVENTS" "event=mnist_inference_metrics"

check_glob_contains "$DEMO_METRICS" \
    "TEAR_METRIC .*profile_id=demo-default .*artifact_id=demo-model .*name=confidence_x100"

check_glob_contains "$MNIST_METRICS" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000"

check_glob_contains "$MNIST_METRICS" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=input_density_x1000"

check_file_contains "$REPORTED_DECISION" \
    "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=reject_input decision=approved reason=input_rejected value=0"

echo "TEAR_PLAN_EVENT_METRIC_VERIFY_OK"
