# SPDX-License-Identifier: Apache-2.0

# Workload naming convention:
#   <workload-name>-<workload-type>
#
# This workload is:
#   mnist-model

MNIST_MODEL_ID := mnist-model
MNIST_MODEL_SRCS := runtime/mnist_model.c

define fetch-workload-assets-mnist-model
	./scripts/fetch-mnist-onnx.sh
endef

define workload-build-mnist-model
	mkdir -p $($(1)_WORKLOAD_BUILD)
	$($(1)_CC) -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-I$($(1)_ORT_INCLUDE) \
		-o $($(1)_WORKLOAD_BUILD)/$(MNIST_MODEL_ID)-$($(1)_ID) \
		$(MNIST_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS) \
		-L$($(1)_ORT_LIB) \
		-lonnxruntime \
		-Wl,-rpath,$($(1)_ORT_RPATH)
endef
