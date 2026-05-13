#!/usr/bin/env bash
set -euo pipefail

KERNEL="${KERNEL:-build/kernel/Image}"
INITRAMFS="${INITRAMFS:-build/initramfs.cpio.gz}"

if [ ! -f "$KERNEL" ]; then
    echo "error: kernel image not found: $KERNEL"
    echo
    echo "Set KERNEL=/path/to/arm64/Image or place it at build/kernel/Image"
    exit 1
fi

qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -m 512M \
    -nographic \
    -no-reboot \
    -kernel "$KERNEL" \
    -initrd "$INITRAMFS" \
    -append "console=ttyAMA0 rdinit=/init panic=-1"
