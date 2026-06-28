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
cp -v scripts/verify-tear-plan-event-metric.sh \
      "$TARGET/bin/verify-tear-plan-event-metric.sh"
chmod +x "$TARGET/bin/verify-tear-plan-event-metric.sh"

cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh
set -eu

case "${1:-start}" in
    start)
        ;;
    stop|restart|reload)
        exit 0
        ;;
    *)
        exit 0
        ;;
esac

if [ -f /tmp/tear-optee-qemu-test-ran ]; then
    echo "TEAR_OPTEE_QEMU_TEST already ran"
    exit 0
fi

touch /tmp/tear-optee-qemu-test-ran

shutdown_guest() {
    sync
    poweroff
}

fail_guest() {
    echo "$1"
    exit 1
}

tear_plan_path() {
    for arg in $(cat /proc/cmdline); do
        case "$arg" in
            tear.plan=*)
                echo "${arg#tear.plan=}"
                return 0
                ;;
        esac
    done

    echo "/etc/tear/qemu-optee.plan"
}

wait_for_supervisor() {
    for i in 1 2 3 4 5; do
        [ -S /tmp/tear-supervisor.sock ] && return 0
        sleep 1
    done

    return 1
}

stop_supervisor() {
    pid="$1"

    kill -INT "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}

TEAR_PLAN="$(tear_plan_path)"

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
      /tmp/tear-optd-events.log \
      /tmp/tear-trustd-events.log

echo "TEAR_OPTEE_QEMU_TEST start"
echo "TEAR_OPTEE_QEMU_PLAN path=$TEAR_PLAN"

TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor &
supervisor_pid=$!

wait_for_supervisor || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_SUPERVISOR_SOCKET failed"
}

echo "TEAR_OPTEE_QEMU_PROVISION_TEST start"

/bin/tearictl provision /etc/tear/model-v1.json || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_PROVISION_TEST provision failed"
}

/bin/tearictl update-model /etc/tear/model-v2.json || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_PROVISION_TEST update failed"
}

/bin/tearictl update-model /etc/tear/model-v1.json && {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_PROVISION_TEST rollback accepted unexpectedly"
}

/bin/tearictl provision-plan "$TEAR_PLAN" || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_PROVISION_TEST provision-plan failed"
}

echo "TEAR_OPTEE_QEMU_PROVISION_TEST done"

echo "TEAR_OPTEE_QEMU_RUN_PLAN_TEST start"

/bin/tearictl run-plan "$TEAR_PLAN" || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_RUN_PLAN_TEST failed"
}

/bin/tearictl report-decision > /tmp/tear-reported-decision.log 2>&1 || {
    stop_supervisor "$supervisor_pid"
    fail_guest "TEAR_OPTEE_QEMU_REPORT_DECISION failed"
}

stop_supervisor "$supervisor_pid"

echo "TEAR_OPTEE_QEMU_RUN_PLAN_TEST done"

/bin/verify-tear-plan-event-metric.sh || \
    fail_guest "TEAR_OPTEE_QEMU_EVENT_METRIC_VERIFY failed"

echo "TEAR_OPTEE_QEMU_TEST done"

shutdown_guest
EOS

chmod +x "$TARGET/etc/init.d/S99tear-test"
