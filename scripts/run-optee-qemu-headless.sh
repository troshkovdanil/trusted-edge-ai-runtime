#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"

mkdir -p "$ROOT_DIR/build"
rm -f "$ROOT_DIR/build/optee-normal-world.log"
rm -f "$ROOT_DIR/build/optee-secure-world.log"

cd "$ROOT_DIR/$OPTEE_QEMU_DIR/out/bin"

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
  -append 'console=ttyAMA0,38400 keep_bootcon root=/dev/vda2 ' \
  -machine virt,acpi=off,secure=on,mte=off,gic-version=3,virtualization=false \
  -object rng-random,filename=/dev/urandom,id=rng0 \
  -device virtio-rng-pci,rng=rng0,max-bytes=1024,period=1000 \
  -netdev user,id=vmnic \
  -device virtio-net-device,netdev=vmnic \
  -serial file:"$ROOT_DIR/build/optee-normal-world.log" \
  -serial file:"$ROOT_DIR/build/optee-secure-world.log"

"$ROOT_DIR/scripts/verify-optee-qemu-run.sh"

echo "TEAR: OP-TEE QEMU headless test passed"
