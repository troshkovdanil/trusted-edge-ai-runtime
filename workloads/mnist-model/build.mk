# SPDX-License-Identifier: Apache-2.0

MNIST_MODEL_ID := mnist-model
MNIST_MODEL_SRCS := runtime/mnist_model.c

define fetch-workload-assets-mnist-model
	./scripts/fetch-mnist-onnx.sh
endef
