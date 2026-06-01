#!/usr/bin/env bash
set -euo pipefail

ORT_VERSION="${ORT_VERSION:-1.26.0}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MODEL_DIR="$ROOT_DIR/models/mnist"
EXTERNAL_DIR="$ROOT_DIR/external"
ORT_DIR="$EXTERNAL_DIR/onnxruntime"
MODEL_PATH="$MODEL_DIR/mnist.onnx"
ORT_HEADER="$ORT_DIR/include/onnxruntime_c_api.h"

mkdir -p "$MODEL_DIR" "$EXTERNAL_DIR"

if [ ! -f "$MODEL_PATH" ]; then
    echo "Fetching MNIST ONNX model..."
    curl -L \
      "https://github.com/onnx/models/raw/main/validated/vision/classification/mnist/model/mnist-8.onnx" \
      -o "$MODEL_PATH"
else
    echo "MNIST model already exists: $MODEL_PATH"
fi

if [ ! -f "$ORT_HEADER" ]; then
    echo "Fetching ONNX Runtime ${ORT_VERSION}..."
    curl -L \
      "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz" \
      -o "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz"

    rm -rf "$ORT_DIR"
    tar -xzf "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz" -C "$EXTERNAL_DIR"
    mv "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}" "$ORT_DIR"
else
    echo "ONNX Runtime already exists: $ORT_DIR"
fi

echo "Done."
echo "Model: $MODEL_PATH"
echo "ONNX Runtime: $ORT_DIR"
