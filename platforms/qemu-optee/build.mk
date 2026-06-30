# SPDX-License-Identifier: Apache-2.0

QEMU_OPTEE_ID := qemu-optee
QEMU_OPTEE_ARCH := aarch64
QEMU_OPTEE_CC := aarch64-linux-gnu-gcc
QEMU_OPTEE_CFLAGS :=
QEMU_OPTEE_TRUST_BACKEND := optee
QEMU_OPTEE_ENABLE_OPTEE := 1
QEMU_OPTEE_ENABLE_ONNXRUNTIME := 1
QEMU_OPTEE_ORT_INCLUDE := external/onnxruntime-aarch64/include
QEMU_OPTEE_ORT_LIB := external/onnxruntime-aarch64/lib
QEMU_OPTEE_ORT_RPATH := /usr/lib
