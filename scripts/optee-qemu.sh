#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"

mkdir -p "$OPTEE_QEMU_DIR"

if [ ! -d "$OPTEE_QEMU_DIR/.repo" ]; then
    echo "TEAR: fetching OP-TEE QEMU v8"
    (
        cd "$OPTEE_QEMU_DIR"
        repo init -u https://github.com/OP-TEE/manifest.git -m qemu_v8.xml < /dev/null
        repo sync --no-clone-bundle
    )
else
    echo "TEAR: OP-TEE QEMU already fetched"
fi

echo "TEAR: installing OP-TEE toolchains"
make -C "$OPTEE_QEMU_DIR/build" toolchains

echo "TEAR: building OP-TEE QEMU"
make -C "$OPTEE_QEMU_DIR/build" all
