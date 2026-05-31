# MNIST ONNX workload

This directory contains the host-native MNIST model used by the TEAR runtime
integration slice.

Files are fetched by:

```bash
./scripts/fetch-mnist-onnx.sh
```

Expected files:

```
models/mnist/mnist.onnx
external/onnxruntime/
```

The MNIST model is executed through ONNX Runtime CPU from
```
runtime/mnist_model.c
```
