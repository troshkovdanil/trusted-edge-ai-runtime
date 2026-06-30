#!/usr/bin/env bash
set -euo pipefail

OPTEE_QEMU_DIR="${OPTEE_QEMU_DIR:-external/optee-qemu-v8}"
ACTION="${1:-all}"

fetch_optee_qemu() {
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
}

prepare_optee_qemu() {
    fetch_optee_qemu

    echo "TEAR: installing OP-TEE toolchains"
    make -C "$OPTEE_QEMU_DIR/build" toolchains
}

build_optee_qemu() {
    echo "TEAR: building OP-TEE QEMU"
    make -C "$OPTEE_QEMU_DIR/build" all
}

case "$ACTION" in
    prepare)
        prepare_optee_qemu
        ;;

    build)
        build_optee_qemu
        ;;

    all)
        prepare_optee_qemu
        build_optee_qemu
        ;;

    *)
        echo "usage: $0 [prepare|build|all]" >&2
        exit 1
        ;;
esac
