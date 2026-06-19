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

if [ "$TEST_MODE" = "mnist-adaptive" ]; then
    cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_MNIST_ADAPTIVE_TEST start"
rm -f /tmp/tear-trustd.sock /tmp/tear-optd.sock /tmp/tear-trusted-decisions /tmp/tear-metric-mnist-onnx-v1-mnist-default

TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor \
  --workload /bin/mnist-model \
  --manifest /etc/tear/mnist-model.json \
  --profile /etc/tear/mnist.profile \
  --args "--sample weak7" \
  --enable-optimizer || exit 1

grep -q "TEAR_METRIC .*name=confidence_margin_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default || exit 1
grep -q "TEAR_METRIC .*name=input_density_x1000" /tmp/tear-metric-mnist-onnx-v1-mnist-default || exit 1
echo "TEAR_OPTEE_MNIST_ADAPTIVE_TEST done"

poweroff -f
EOS
else
    cat > "$TARGET/etc/init.d/S99tear-test" <<'EOS'
#!/bin/sh

echo "TEAR_OPTEE_CA_TEST start"
/bin/tear-optee-ca || exit 1
echo "TEAR_OPTEE_CA_TEST done"

echo "TEAR_OPTEE_TRUSTD_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test || exit 1
echo "TEAR_OPTEE_TRUSTD_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-enroll || exit 1
echo "TEAR_OPTEE_TRUSTD_ENROLL_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST start"
/bin/tear-trustd-optee --backend optee --self-test-verify || exit 1
echo "TEAR_OPTEE_TRUSTD_VERIFY_SELF_TEST done"

echo "TEAR_OPTEE_TRUSTD_ENROLL_SOCKET_TEST start"
/bin/tear-trustd-optee --backend optee &
trustd_pid=$!
sleep 1
/bin/tearictl enroll /etc/tear/model-v1.json || {
    kill "$trustd_pid"
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
    exit 1
}
/bin/tearictl verify /etc/tear/model-v1.json || {
    kill "$trustd_pid"
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
    exit 1
}
/bin/tearictl update-model /etc/tear/model-v2.json || {
    kill "$trustd_pid"
    exit 1
}
/bin/tearictl update-model /etc/tear/model-v1.json && {
    kill "$trustd_pid"
    exit 1
}
kill "$trustd_pid"
echo "TEAR_OPTEE_TRUSTD_UPDATE_SOCKET_TEST done"

echo "TEAR_OPTEE_QEMU_TEST start"
TEAR_TRUSTD_PATH=/bin/tear-trustd-optee \
TEAR_TRUSTD_BACKEND=optee \
/bin/tear-supervisor \
  --workload /bin/demo-model \
  --manifest /etc/tear/model-v2.json \
  --profile /etc/tear/demo.profile
rc=$?
echo "TEAR_OPTEE_QEMU_TEST exit=$rc"

poweroff -f
EOS
fi

chmod +x "$TARGET/etc/init.d/S99tear-test"
