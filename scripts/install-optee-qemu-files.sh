#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${1:-external/optee-qemu-v8}"
TARGET="$OPTEE_QEMU_DIR/out-br/target"

TEAR_TA_UUID="7c9d7b3a-2f4e-4c8f-9a11-6b4454454152"
TEAR_TA="build/optee/tear_ta/${TEAR_TA_UUID}.ta"
TEAR_CA="build/optee/tear-optee-ca"

mkdir -p "$TARGET/bin" "$TARGET/etc/tear" "$TARGET/lib/optee_armtz"

make optee-ta
make optee-ca
make optee-trustd

cp -v "$TEAR_TA" "$TARGET/lib/optee_armtz/"
cp -v "$TEAR_CA" "$TARGET/bin/tear-optee-ca"

cp -v build/tear-supervisor "$TARGET/bin/"
cp -v build/tear-trustd "$TARGET/bin/"
cp -v build/tearictl "$TARGET/bin/"
cp -v build/tear-optd "$TARGET/bin/"
cp -v build/demo-model "$TARGET/bin/"
cp -v build/tear-runtime-manager "$TARGET/bin/"
cp -v build/optee/tear-trustd-optee "$TARGET/bin/"

cp -v build/mnist-model "$TARGET/bin/"
cp -v examples/mnist-model.json "$TARGET/etc/tear/"

mkdir -p "$TARGET/models/mnist"
cp -v models/mnist/mnist.onnx "$TARGET/models/mnist/"
cp -v external/onnxruntime-aarch64/lib/libonnxruntime.so* "$TARGET/usr/lib/"

cp -v examples/model-v1.json "$TARGET/etc/tear/"
cp -v examples/model-v2.json "$TARGET/etc/tear/"

cp -v profiles/demo.profile "$TARGET/etc/tear/"
cp -v profiles/mnist.profile "$TARGET/etc/tear/"

cp -v plans/qemu-optee.plan "$TARGET/etc/tear/"

cat > "$TARGET/bin/verify-tear-qemu-run.sh" <<'EOS'
#!/bin/sh
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

SUPERVISOR_EVENTS="/tmp/tear-supervisor-events.log"
TRUSTD_EVENTS="/tmp/tear-trustd-events.log"
OPTD_EVENTS="/tmp/tear-optd-events.log"
RUNTIME_MANAGER_EVENTS="/tmp/tear-runtime-manager-events.log"
WORKLOAD_EVENTS="/tmp/tear-runtime-manager-events.log-run-*"
REPORTED_DECISION="/tmp/tear-reported-decision.log"

echo "TEAR_QEMU_GUEST_VERIFY start"

check_file_contains "$TRUSTD_EVENTS" "event=trustd_optee_backend_ping_ok"
check_file_contains "$TRUSTD_EVENTS" "event=trustd_optee_backend_enroll_ok"
check_file_contains "$TRUSTD_EVENTS" "event=trustd_optee_backend_enroll_before_verify_ok"
check_file_contains "$TRUSTD_EVENTS" "event=trustd_optee_backend_verify_ok"

check_file_contains "$TRUSTD_EVENTS" "event=trustd_start"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_enroll"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_verify_ok"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_update_ok"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_rollback_rejected"
check_file_contains "$TRUSTD_EVENTS" "event=optee_record_decision_ok"
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

check_glob_contains "/tmp/tear-metric-demo-model-demo-default-*" \
    "TEAR_METRIC .*profile_id=demo-default .*artifact_id=demo-model .*name=confidence_x100"

check_glob_contains "/tmp/tear-metric-mnist-onnx-v1-mnist-default-*" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000"

check_glob_contains "/tmp/tear-metric-mnist-onnx-v1-mnist-default-*" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=input_density_x1000"

check_file_contains "$REPORTED_DECISION" \
    "run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=reject_input decision=approved reason=input_rejected value=0"

echo "TEAR_QEMU_GUEST_VERIFY_OK"
EOS

chmod +x "$TARGET/bin/verify-tear-qemu-run.sh"

cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh
set -eu

shutdown_guest() {
    sync
    poweroff
}

fail_guest() {
    echo "$1"
    exit 1
}

echo "TEAR_OPTEE_CA_TEST start"
/bin/tear-optee-ca || fail_guest "TEAR_OPTEE_CA_TEST failed"
echo "TEAR_OPTEE_CA_TEST done"

rm -f /tmp/tear-trustd-events.log

echo "TEAR_OPTEE_TRUSTD_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test || fail_guest "TEAR_OPTEE_TRUSTD_SELF_TEST failed"
echo "TEAR_OPTEE_TRUSTD_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-enroll || fail_guest "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST failed"
echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-verify || fail_guest "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST failed"
echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST start"
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1

/bin/tearictl enroll /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    fail_guest "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST enroll failed"
}

/bin/tearictl verify /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    fail_guest "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST verify failed"
}

/bin/tearictl update-model /etc/tear/model-v2.json || {
    kill "$trustd_pid"
    fail_guest "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST update failed"
}

/bin/tearictl update-model /etc/tear/model-v1.json && {
    kill "$trustd_pid"
    fail_guest "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST rollback accepted unexpectedly"
}

kill "$trustd_pid"
echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

rm -f /tmp/tear-trustd.sock \
      /tmp/tear-optd.sock \
      /tmp/tear-supervisor.sock \
      /tmp/tear-reported-decision.log \
      /tmp/tear-metric-* \
      /tmp/tear-supervisor-events.log \
      /tmp/tear-runtime-manager-events.log \
      /tmp/tear-runtime-manager-events.log-run-* \
      /tmp/tear-demo-model-events.log* \
      /tmp/tear-mnist-model-events.log* \
      /tmp/tear-optd-events.log

echo "TEAR_OPTEE_QEMU_RUN_PLAN_TEST start"

TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor &
supervisor_pid=$!

for i in 1 2 3 4 5; do
    [ -S /tmp/tear-supervisor.sock ] && break
    sleep 1
done

/bin/tearictl run-plan /etc/tear/qemu-optee.plan || {
    kill -INT "$supervisor_pid"
    wait "$supervisor_pid" || true
    fail_guest "TEAR_OPTEE_QEMU_RUN_PLAN_TEST failed"
}

kill -INT "$supervisor_pid"
wait "$supervisor_pid" || true

echo "TEAR_OPTEE_QEMU_RUN_PLAN_TEST done"

rm -f /tmp/tear-trustd.sock
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1

/bin/tearictl report-decision > /tmp/tear-reported-decision.log 2>&1 || {
    kill "$trustd_pid"
    fail_guest "TEAR_OPTEE_QEMU_REPORT_DECISION failed"
}

kill "$trustd_pid"

/bin/verify-tear-qemu-run.sh || fail_guest "TEAR_OPTEE_QEMU_VERIFY failed"

shutdown_guest
EOS

chmod +x "$TARGET/etc/init.d/S99tear-test"
