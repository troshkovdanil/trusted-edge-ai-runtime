# SPDX-License-Identifier: Apache-2.0

DEMO_MODEL_ID := demo-model
DEMO_MODEL_SRCS := runtime/demo_model.c

define fetch-workload-assets-demo-model
	@true
endef

define workload-build-demo-model
	mkdir -p $(WORKLOAD_BUILD)/$($(1)_ID)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-o $(WORKLOAD_BUILD)/$($(1)_ID)/$(DEMO_MODEL_ID)-$($(1)_ID) \
		$(DEMO_MODEL_SRCS) $(PROFILE_SRCS) $(OBSERVABILITY_SRCS)
endef
