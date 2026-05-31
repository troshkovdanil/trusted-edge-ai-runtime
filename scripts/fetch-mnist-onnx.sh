#!/usr/bin/env bash
set -euo pipefail

ORT_VERSION="${ORT_VERSION:-1.26.0}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MODEL_DIR="$ROOT_DIR/models/mnist"
EXTERNAL_DIR="$ROOT_DIR/external"
ORT_DIR="$EXTERNAL_DIR/onnxruntime"

mkdir -p "$MODEL_DIR" "$EXTERNAL_DIR"

echo "Fetching MNIST ONNX model..."
curl -L \
  "https://github.com/onnx/models/raw/main/validated/vision/classification/mnist/model/mnist-8.onnx" \
  -o "$MODEL_DIR/mnist.onnx"

echo "Fetching ONNX Runtime ${ORT_VERSION}..."
curl -L \
  "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-linux-x64-${ORT_VERSION}.tgz" \
  -o "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz"

rm -rf "$ORT_DIR"
tar -xzf "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}.tgz" -C "$EXTERNAL_DIR"
mv "$EXTERNAL_DIR/onnxruntime-linux-x64-${ORT_VERSION}" "$ORT_DIR"

echo "Done."
echo "Model: $MODEL_DIR/mnist.onnx"
echo "ONNX Runtime: $ORT_DIR"
