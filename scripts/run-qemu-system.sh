#!/usr/bin/env bash
set -euo pipefail

QUIET="${QUIET:-0}"
KERNEL="${KERNEL:-build/kernel/Image}"
INITRAMFS="${INITRAMFS:-build/initramfs.cpio.gz}"
TELEMETRY="${TELEMETRY:-build/telemetry.log}"

mkdir -p "$(dirname "$TELEMETRY")"
rm -f "$TELEMETRY"

if [ ! -f "$KERNEL" ]; then
    echo "error: kernel image not found: $KERNEL"
    exit 1
fi

if [ "$QUIET" = "1" ]; then
    qemu-system-aarch64 \
        -machine virt \
        -cpu cortex-a57 \
        -m 512M \
        -nographic \
        -no-reboot \
        -kernel "$KERNEL" \
        -initrd "$INITRAMFS" \
        -append "console=ttyAMA0 rdinit=/init panic=-1" \
	>"$TELEMETRY" 2>&1
else
    qemu-system-aarch64 \
        -machine virt \
        -cpu cortex-a57 \
        -m 512M \
        -nographic \
        -no-reboot \
        -kernel "$KERNEL" \
        -initrd "$INITRAMFS" \
        -append "console=ttyAMA0 rdinit=/init panic=-1" \
        2>&1 | tee "$TELEMETRY"
fi
