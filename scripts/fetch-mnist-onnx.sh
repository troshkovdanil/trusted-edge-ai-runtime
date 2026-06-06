#!/usr/bin/env bash
set -euo pipefail

ORT_VERSION="${ORT_VERSION:-1.26.0}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MODEL_DIR="$ROOT_DIR/models/mnist"
EXTERNAL_DIR="$ROOT_DIR/external"

ORT_X64_DIR="$EXTERNAL_DIR/onnxruntime"
ORT_AARCH64_DIR="$EXTERNAL_DIR/onnxruntime-aarch64"

MODEL_PATH="$MODEL_DIR/mnist.onnx"
ORT_X64_HEADER="$ORT_X64_DIR/include/onnxruntime_c_api.h"
ORT_AARCH64_HEADER="$ORT_AARCH64_DIR/include/onnxruntime_c_api.h"

mkdir -p "$MODEL_DIR" "$EXTERNAL_DIR"

if [ ! -f "$MODEL_PATH" ]; then
    echo "Fetching MNIST ONNX model..."
    curl -L \
      "https://github.com/onnx/models/raw/main/validated/vision/classification/mnist/model/mnist-8.onnx" \
      -o "$MODEL_PATH"
else
    echo "MNIST model already exists: $MODEL_PATH"
fi

if [ ! -f "$ORT_X64_HEADER" ]; then
    echo "Fetching ONNX Runtime x64 ${ORT_VERSION}..."
    curl -L \
      "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz" \
      -o "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz"

    rm -rf "$ORT_X64_DIR"
    tar -xzf "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz" -C "$EXTERNAL_DIR"
    mv "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}" "$ORT_X64_DIR"
    rm -f "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz"
else
    echo "ONNX Runtime x64 already exists: $ORT_X64_DIR"
fi

if [ ! -f "$ORT_AARCH64_HEADER" ]; then
    echo "Fetching ONNX Runtime aarch64 ${ORT_VERSION}..."
    curl -L \
      "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-aarch64-${ORT_VERSION}.tgz" \
      -o "$EXTERNAL_DIR/onnxruntime-linux-aarch64-${ORT_VERSION}.tgz"

    rm -rf "$ORT_AARCH64_DIR"
    tar -xzf "$EXTERNAL_DIR/onnxruntime-linux-aarch64-${ORT_VERSION}.tgz" -C "$EXTERNAL_DIR"
    mv "$EXTERNAL_DIR/onnxruntime-linux-aarch64-${ORT_VERSION}" "$ORT_AARCH64_DIR"
    rm -f "$EXTERNAL_DIR/onnxruntime-linux-aarch64-${ORT_VERSION}.tgz"
else
    echo "ONNX Runtime aarch64 already exists: $ORT_AARCH64_DIR"
fi

echo "Done."
echo "Model: $MODEL_PATH"
echo "ONNX Runtime x64: $ORT_X64_DIR"
echo "ONNX Runtime aarch64: $ORT_AARCH64_DIR"
