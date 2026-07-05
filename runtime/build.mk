# SPDX-License-Identifier: Apache-2.0

# Platform-neutral TEAR runtime build rules.
#
# This file should not know which platforms exist. It only knows how to build
# common TEAR runtime binaries for a platform prefix passed by the caller.
#
# Expected platform variables for prefix $(1):
#   $($(1)_CC)
#   $($(1)_CFLAGS)
#   $($(1)_BUILD_DIR)
#   $($(1)_SUPERVISOR)
#   $($(1)_RUNTIME_MANAGER)
#   $($(1)_TRUSTD)
#   $($(1)_TEARICTL)
#   $($(1)_OPTD)
#   $($(1)_PLATFORM_SRCS)

RUNTIME_PATHS_SRCS := runtime/runtime_paths.c
OBSERVABILITY_SRCS := runtime/observability.c
PROFILE_SRCS := runtime/profile.c
MANIFEST_SRCS := runtime/model_manifest.c
TRUST_CLIENT_SRCS := runtime/trust_client.c
TRUSTD_SRCS := runtime/trustd.c runtime/trusted_state.c
OPTD_SRCS := runtime/optd.c runtime/optimizer_policy.c
WORKLOAD_ADAPTER_SRCS := runtime/workload_adapter.c
PLATFORM_ADAPTER_SRCS := runtime/platform_adapter.c
RUNTIME_MANAGER_SRCS := runtime/runtime_manager_main.c runtime/runtime_manager.c
TEARICTL_SRCS := runtime/tearictl.c

define runtime-build-platform
	mkdir -p $($(1)_BUILD_DIR)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-Iplatforms/common \
		-o $($(1)_SUPERVISOR) runtime/supervisor.c $(RUNTIME_PATHS_SRCS) $(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-Iplatforms/common \
		-o $($(1)_RUNTIME_MANAGER) \
		$(RUNTIME_MANAGER_SRCS) \
		$(WORKLOAD_ADAPTER_SRCS) \
		$(PLATFORM_ADAPTER_SRCS) \
		$($(1)_PLATFORM_SRCS) \
		$(PROFILE_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-Iplatforms/common \
		-o $($(1)_TRUSTD) \
		$(TRUSTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-Iplatforms/common \
		-o $($(1)_TEARICTL) \
		$(TEARICTL_SRCS) \
		$(MANIFEST_SRCS) \
		$(TRUST_CLIENT_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
	$($(1)_CC) -static -O2 -Wall -Wextra $($(1)_CFLAGS) \
		-Iruntime \
		-Iplatforms/common \
		-o $($(1)_OPTD) \
		$(OPTD_SRCS) \
		$(RUNTIME_PATHS_SRCS) \
		$(OBSERVABILITY_SRCS)
endef
