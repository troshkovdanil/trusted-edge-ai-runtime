#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"

NORMAL_LOG="$ROOT_DIR/build/optee-normal-world.log"
SECURE_LOG="$ROOT_DIR/build/optee-secure-world.log"
VERIFY_SCRIPT="${VERIFY_SCRIPT:-$ROOT_DIR/scripts/verify-qemu-optee.sh}"
GUEST_OK_MARKER="TEAR_QEMU_GUEST_VERIFY_OK"
TEAR_QEMU_PLAN="${TEAR_QEMU_PLAN:-/etc/tear/active.plan}"

mkdir -p "$ROOT_DIR/build"
rm -f "$NORMAL_LOG" "$SECURE_LOG"

echo "TEAR: normal-world log: $NORMAL_LOG"
echo "TEAR: secure-world log: $SECURE_LOG"
echo "TEAR: guest plan: $TEAR_QEMU_PLAN"
echo "TEAR: OP-TEE QEMU test running..."

cd "$ROOT_DIR/$OPTEE_QEMU_DIR/out/bin"

set +e
../../qemu/build/qemu-system-aarch64 \
  -nographic \
  -monitor none \
  -smp 2 \
  -cpu max,sme=on,pauth-impdef=on \
  -d unimp \
  -semihosting-config enable=on,target=native \
  -m 1057 \
  -bios bl1.bin \
  -initrd rootfs.cpio.gz \
  -kernel Image \
  -append "console=ttyAMA0,38400 keep_bootcon root=/dev/vda2 tear.plan=$TEAR_QEMU_PLAN " \
  -machine virt,acpi=off,secure=on,mte=off,gic-version=3,virtualization=false \
  -object rng-random,filename=/dev/urandom,id=rng0 \
  -device virtio-rng-pci,rng=rng0,max-bytes=1024,period=1000 \
  -netdev user,id=vmnic \
  -device virtio-net-device,netdev=vmnic \
  -serial file:"$NORMAL_LOG" \
  -serial file:"$SECURE_LOG"
qemu_rc=$?
set -e

if [ "$qemu_rc" -ne 0 ] && ! grep -q "$GUEST_OK_MARKER" "$NORMAL_LOG"; then
    echo "TEAR: QEMU exited with rc=$qemu_rc before successful guest verification"
    echo "TEAR: inspect log: $NORMAL_LOG"
    exit "$qemu_rc"
fi

if [ "$qemu_rc" -ne 0 ]; then
    echo "TEAR: QEMU exited with rc=$qemu_rc after successful guest verification"
fi

echo "TEAR: OP-TEE QEMU test running... OK"

echo "TEAR: running host-side verification..."
"$VERIFY_SCRIPT"

echo "TEAR: OP-TEE QEMU test passed"
