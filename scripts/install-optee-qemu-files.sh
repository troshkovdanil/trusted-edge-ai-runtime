#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${1:-external/optee-qemu-v8}"
TARGET="$OPTEE_QEMU_DIR/out-br/target"

TEAR_TA_UUID="7c9d7b3a-2f4e-4c8f-9a11-6b4454454152"
TEAR_TA="build/optee/tear_ta/${TEAR_TA_UUID}.ta"
TEAR_CA="build/optee/tear-optee-ca"

TEST_MODE="${2:-default}"

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
RUNTIME_MANAGER_EVENTS="/tmp/tear-runtime-manager-events.log"
WORKLOAD_EVENTS="/tmp/tear-runtime-manager-events.log-run-*"

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

check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_report_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_exit status=0"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_shutdown"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_start"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_loaded"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=demo-model"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_loaded"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=run-[0-9]+-[0-9]+"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_workload_exit status=0"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_shutdown"

check_glob_contains "$WORKLOAD_EVENTS" "event=model_init"
check_glob_contains "$WORKLOAD_EVENTS" "event=inference_start"
check_glob_contains "$WORKLOAD_EVENTS" "event=inference_done"
check_glob_contains "$WORKLOAD_EVENTS" "event=model_shutdown"

check_glob_contains "/tmp/tear-metric-demo-model-demo-default-*" \
    "TEAR_METRIC .*profile_id=demo-default .*artifact_id=demo-model .*name=confidence_x100"

echo "TEAR_QEMU_GUEST_VERIFY_OK"
EOS

cat > "$TARGET/bin/verify-tear-qemu-mnist-adaptive-run.sh" <<'EOS'
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

echo "TEAR_QEMU_MNIST_ADAPTIVE_GUEST_VERIFY start"

check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=provisioning_report_done"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_start"
check_file_contains "$SUPERVISOR_EVENTS" "event=workload_exit status=0"
check_file_contains "$SUPERVISOR_EVENTS" "event=supervisor_shutdown"

check_file_contains "$TRUSTD_EVENTS" "event=trustd_start"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_enroll"
check_file_contains "$TRUSTD_EVENTS" "event=optee_model_verify_ok"
check_file_contains "$TRUSTD_EVENTS" "event=optee_record_decision_ok"
check_file_contains "$TRUSTD_EVENTS" "event=optimization_decision_recorded"

check_file_contains "$OPTD_EVENTS" "event=optd_start"
check_file_contains "$OPTD_EVENTS" "event=optd_proposal"

check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_start"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_loaded"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "artifact_id=mnist-onnx-v1"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_loaded"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=run-[0-9]+-[0-9]+"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=manifest_verified"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_workload_exit status=0"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimizer_proposal_received"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=request_high_accuracy_profile"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=rejected"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=profile_unavailable"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimization_decision_recorded_by_runtime_manager"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=optimization_decision_reported_by_runtime_manager"
check_file_contains "$RUNTIME_MANAGER_EVENTS" "event=runtime_manager_shutdown"

check_glob_contains "$WORKLOAD_EVENTS" "event=mnist_inference_metrics"

check_glob_contains "/tmp/tear-metric-mnist-onnx-v1-mnist-default-*" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=confidence_margin_x1000"
check_glob_contains "/tmp/tear-metric-mnist-onnx-v1-mnist-default-*" \
    "TEAR_METRIC .*profile_id=mnist-default .*artifact_id=mnist-onnx-v1 .*name=input_density_x1000"

check_file_contains "$REPORTED_DECISION" \
    "DECISION run_id=run-[0-9]+-[0-9]+ artifact_id=mnist-onnx-v1 proposal=request_high_accuracy_profile decision=rejected reason=profile_unavailable value=0"

echo "TEAR_QEMU_MNIST_ADAPTIVE_GUEST_VERIFY_OK"
EOS

chmod +x "$TARGET/bin/verify-tear-qemu-run.sh"
chmod +x "$TARGET/bin/verify-tear-qemu-mnist-adaptive-run.sh"

if [ "$TEST_MODE" = "mnist-adaptive" ]; then
    cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_MNIST_ADAPTIVE_TEST start"
rm -f /tmp/tear-trustd.sock \
      /tmp/tear-optd.sock \
      /tmp/tear-supervisor.sock \
      /tmp/tear-reported-decision.log \
      /tmp/tear-metric-mnist-onnx-v1-mnist-default-* \
      /tmp/tear-*-events.log*

TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor \
  --workload /bin/mnist-model \
  --manifest /etc/tear/mnist-model.json \
  --profile /etc/tear/mnist.profile \
  --args "--sample weak7" \
  --enable-optimizer || {
    echo "TEAR_OPTEE_MNIST_ADAPTIVE_TEST failed"
    poweroff -f
    exit 1
}

rm -f /tmp/tear-trustd.sock
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1

/bin/tearictl report-decision > /tmp/tear-reported-decision.log 2>&1 || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_MNIST_ADAPTIVE_REPORT_DECISION failed"
    poweroff -f
    exit 1
}

kill "$trustd_pid"

/bin/verify-tear-qemu-mnist-adaptive-run.sh || {
    echo "TEAR_OPTEE_MNIST_ADAPTIVE_VERIFY failed"
    poweroff -f
    exit 1
}

echo "TEAR_OPTEE_MNIST_ADAPTIVE_TEST done"

poweroff -f
EOS
else
    cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_CA_TEST start"
/bin/tear-optee-ca || {
    echo "TEAR_OPTEE_CA_TEST failed"
    poweroff -f
    exit 1
}
echo "TEAR_OPTEE_CA_TEST done"

rm -f /tmp/tear-trustd-events.log

echo "TEAR_OPTEE_TRUSTD_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test || {
    echo "TEAR_OPTEE_TRUSTD_SELF_TEST failed"
    poweroff -f
    exit 1
}
echo "TEAR_OPTEE_TRUSTD_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-enroll || {
    echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST failed"
    poweroff -f
    exit 1
}
echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-verify || {
    echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST failed"
    poweroff -f
    exit 1
}
echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST start"
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1
/bin/tearictl enroll /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST failed"
    poweroff -f
    exit 1
}
kill "$trustd_pid"
echo "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST done"

echo "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST start"
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1
/bin/tearictl enroll /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST enroll failed"
    poweroff -f
    exit 1
}
/bin/tearictl verify /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST verify failed"
    poweroff -f
    exit 1
}
kill "$trustd_pid"
echo "TEAR_OPTEE_TRUSTD_VERIFY_SOCKET_TEST done"

echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST start"
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1
/bin/tearictl enroll /etc/tear/model-v1.json || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST enroll failed"
    poweroff -f
    exit 1
}
/bin/tearictl update-model /etc/tear/model-v2.json || {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST update failed"
    poweroff -f
    exit 1
}
/bin/tearictl update-model /etc/tear/model-v1.json && {
    kill "$trustd_pid"
    echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST rollback accepted unexpectedly"
    poweroff -f
    exit 1
}
kill "$trustd_pid"
echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

rm -f /tmp/tear-trustd.sock \
      /tmp/tear-optd.sock \
      /tmp/tear-supervisor.sock \
      /tmp/tear-metric-demo-model-demo-default-* \
      /tmp/tear-supervisor-events.log \
      /tmp/tear-runtime-manager-events.log \
      /tmp/tear-runtime-manager-events.log-run-* \
      /tmp/tear-demo-model-events.log*

echo "TEAR_OPTEE_QEMU_TEST start"
TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor \
  --workload /bin/demo-model \
  --manifest /etc/tear/model-v2.json \
  --profile /etc/tear/demo.profile
rc=$?
echo "TEAR_OPTEE_QEMU_TEST exit=$rc"

if [ "$rc" -ne 0 ]; then
    echo "TEAR_OPTEE_QEMU_TEST failed"
    poweroff -f
    exit 1
fi

/bin/verify-tear-qemu-run.sh || {
    echo "TEAR_OPTEE_QEMU_VERIFY failed"
    poweroff -f
    exit 1
}

poweroff -f
EOS
fi

chmod +x "$TARGET/etc/init.d/S99tear-test"
