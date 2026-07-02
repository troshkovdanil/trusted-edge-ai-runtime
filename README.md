# TEAR — Trusted Edge AI Runtime

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

Trusted runtime infrastructure for adaptive edge AI systems.

TEAR focuses on:

- embedded runtime infrastructure
- trusted state handling
- OTA-aware execution
- telemetry and observability
- adaptive runtime optimization
- constrained edge deployment
- QEMU-first reproducible development

TEAR enables trusted adaptive edge AI systems:

- verified deployment
- runtime observability
- rollback protection
- telemetry-driven optimization
- policy-bounded AI-assisted runtime adaptation

Possible use cases:

- thermal throttling
- latency spikes
- battery/power mode
- camera FPS drop
- model rollback attempt
- OTA model update
- backend/runtime mismatch
- fleet anomaly detection

TEAR enables adaptive AI runtime optimization without giving optimizers unrestricted control over trusted deployment state.

The key safety idea:

- optimizer/agent suggests
- TEAR policy/trust layer enforces

Example adaptive control decisions inside TEAR:

```text
optimization request
  -> trusted policy validation
  -> runtime action
```

e.g.:

```text
switch inference backend CPU -> NPU
  -> backend allowed?
  -> apply backend switch

reduce camera input resolution
  -> policy allows quality reduction?
  -> apply lower resolution

lower inference frequency
  -> latency/FPS constraints satisfied?
  -> reduce inference rate

switch model fp32 -> int8
  -> target model trusted?
  -> activate int8 model

enter thermal-safe mode
  -> thermal threshold exceeded?
  -> enable safe execution profile

disable non-critical analytics
  -> workload priority policy allows?
  -> suspend auxiliary pipeline

apply OTA model update
  -> rollback-safe and signed?
  -> activate new model version

fallback NPU -> CPU
  -> accelerator unavailable?
  -> switch to CPU execution
```

## Development Approach

TEAR explores modern AI-assisted systems engineering workflows alongside
traditional embedded and runtime development practices.

The project emphasizes architectural transparency, reproducibility,
and human-directed infrastructure design.

## Development Setup (Ubuntu 24.04)

Install required build dependencies:

```bash
./scripts/install-deps-ubuntu.sh
```

Build all configured platforms:

```bash
make build
```

Run all configured platform tests:

```bash
make test
```

Build and test the host mock platform:

```bash
make host-mock-build
make host-mock-test
```

Build and test the OP-TEE QEMU platform:

```bash
make qemu-optee-build
make qemu-optee-test
```

Run an already-built OP-TEE QEMU image:

```bash
make qemu-optee-run
```

### Host Mock Development

For faster iteration on workloads and runtime components, TEAR provides a host-native x86 execution path using the mock secure backend.

This enables rapid development of:

- runtime infrastructure
- trusted state handling
- adaptive optimization logic
- workload integration

while OP-TEE QEMU remains the canonical trusted execution validation target.

Build host mock binaries:

```bash
make host-mock-build
```

Run host mock validation:

```bash
make host-mock-test
```

### Adaptive Optimization Demonstration

The host-native adaptive validation exercises the full optimization control loop:

```text
MNIST workload
  -> runtime metrics
  -> optimizer proposal
  -> policy validation
  -> trusted decision recording
```

Test scenarios currently include:

```text
clean7
  -> keep_current_profile

weak7
  -> request_high_accuracy_profile

noise
  -> reject_input
```

## Build Model: Platforms and Workloads

TEAR uses a small modular build model:

```text
runtime/build.mk
  -> common TEAR runtime binaries

workloads/<workload-name>-<workload-type>/build.mk
  -> workload source files, workload assets, and workload build rule

platforms/<platform>-<secure-backend>/build.mk
  -> platform toolchain, output paths, workload selection, secure backend,
     run/test integration
```

### Naming model

Platforms use:

```text
<platform>-<secure-backend>
```

Current examples:

```text
host-mock
qemu-optee
```

Meaning:

- `host-mock` = host platform with mock secure backend
- `qemu-optee` = QEMU platform with OP-TEE secure backend

Workloads use:

```text
<workload-name>-<workload-type>
```

Current examples:

```text
demo-model
mnist-model
```

Future examples could be:

```text
camera-app
detector-model
sensor-fusion-app
```

This keeps build outputs readable:

```text
build/platforms/<platform>-<secure-backend>/
build/workloads/<platform>-<secure-backend>/<workload-name>-<workload-type>-<platform>-<secure-backend>
```

For example:

```text
build/platforms/host-mock/tear-supervisor-host
build/workloads/host-mock/mnist-model-host-mock
build/platforms/qemu-optee/tear-supervisor
build/workloads/qemu-optee/mnist-model-qemu-optee
```

### Platform lifecycle hooks

The top-level `Makefile` treats each platform as a target with a common lifecycle.

A platform must provide these hooks:

```text
build-<platform>-prepare
build-<platform>-runtime
build-<platform>-workloads
build-<platform>-secure-backend
build-<platform>-finalize
run-<platform>
test-<platform>
```

The top-level `Makefile` calls them through generic helpers:

```text
build-platform
run-platform
test-platform
```

This means adding a new platform mostly means implementing the same hook set in:

```text
platforms/<platform>-<secure-backend>/build.mk
```

### Adding a new platform

To add a new platform:

1. Create a new directory:

```text
platforms/<platform>-<secure-backend>/
```

2. Add:

```text
platforms/<platform>-<secure-backend>/build.mk
```

3. Define platform variables:

```text
<PLATFORM>_ID
<PLATFORM>_ARCH
<PLATFORM>_PLAN
<PLATFORM>_BUILD_DIR
<PLATFORM>_WORKLOAD_BUILD
<PLATFORM>_CC
<PLATFORM>_CFLAGS
<PLATFORM>_SUPERVISOR
<PLATFORM>_RUNTIME_MANAGER
<PLATFORM>_TRUSTD
<PLATFORM>_TEARICTL
<PLATFORM>_OPTD
```

4. Implement lifecycle hooks:

```text
build-<platform>-prepare
build-<platform>-runtime
build-<platform>-workloads
build-<platform>-secure-backend
build-<platform>-finalize
run-<platform>
test-<platform>
```

5. Include the platform file from the top-level `Makefile`:

```makefile
include platforms/<platform>-<secure-backend>/build.mk
```

6. Add top-level convenience targets:

```makefile
<platform>-<secure-backend>-build:
	$(call build-platform,<platform>-<secure-backend>)

<platform>-<secure-backend>-run:
	$(call run-platform,<platform>-<secure-backend>)

<platform>-<secure-backend>-test: <platform>-<secure-backend>-build
	$(call test-platform,<platform>-<secure-backend>)
```

### Adding a new workload

To add a new workload:

1. Create a new directory:

```text
workloads/<workload-name>-<workload-type>/
```

2. Add:

```text
workloads/<workload-name>-<workload-type>/build.mk
```

3. Define workload variables:

```text
<WORKLOAD>_ID
<WORKLOAD>_SRCS
```

4. Define an asset fetch hook:

```makefile
define fetch-workload-assets-<workload-name>-<workload-type>
	@true
endef
```

If the workload has external assets, fetch them there.

5. Define a platform-aware workload build hook:

```makefile
define workload-build-<workload-name>-<workload-type>
	mkdir -p $($(1)_WORKLOAD_BUILD)
	$($(1)_CC) ...
endef
```

6. Include the workload file from the top-level `Makefile`:

```makefile
include workloads/<workload-name>-<workload-type>/build.mk
```

7. Opt into the workload from each platform that supports it:

```makefile
define build-<platform>-workloads
	$(call workload-build-<workload-name>-<workload-type>,<PLATFORM_PREFIX>)
endef
```

This keeps workloads independent from platforms while allowing each platform to decide which workloads it supports.

## MVP-1: qemu-system-aarch64

The project builds a minimal ARM64 initramfs containing TEAR runtime
components and boots it under `qemu-system-aarch64`.

## MVP-2: Runtime supervisor and telemetry

TEAR boots into a minimal init process that starts a runtime supervisor.

The supervisor:

- emits structured `TEAR_EVENT` telemetry
- launches workload binaries
- waits for workload completion
- reports workload exit status
- powers off the guest cleanly

## MVP-3: Workload abstraction

TEAR now supports runtime-selectable workloads through the Linux kernel
command line.

The mock model demonstrates:

- runtime workload abstraction
- inference-style lifecycle telemetry
- workload-specific execution flow
- runtime parameter propagation through QEMU kernel cmdline

## MVP-4: Trusted model state and rollback rejection

TEAR now has a thin end-to-end trusted model flow:

```text
manifest v1
  -> tearictl enroll
  -> trustd trusted state
  -> tearictl update-model v2
  -> rollback attempt to v1 rejected
  -> runtime manager verifies and runs v2
```

This simulates the trusted deployment control path that future adaptive runtime decisions will rely on.

## MVP-5: Adaptive optimization control loop

TEAR now includes a host-native adaptive optimization demonstration.

The adaptive flow is:

```text
MNIST workload
  -> runtime metrics
  -> optimizer proposal
  -> policy validation
  -> trusted decision recording
```

Current optimization scenarios:

```text
clean7
  -> keep_current_profile
  -> approved

weak7
  -> request_high_accuracy_profile
  -> rejected (profile unavailable)

noise
  -> reject_input
  -> approved
```

The optimization engine does not directly modify runtime state.

Instead:

```text
optimizer proposal
  -> policy validation
  -> trusted decision log
```

This demonstrates the core TEAR architecture:

```text
workload
  -> telemetry
  -> optimizer
  -> trusted policy
  -> recorded decision
```

The implementation currently runs in host-native mode using ONNX Runtime and a small MNIST workload while preserving the same trusted-control architecture intended for future embedded deployments.

## MVP-6: OP-TEE trusted state backend

TEAR now supports a trusted-state backend implemented using OP-TEE.

The trusted flow is:

```text
tearictl
  -> trustd
  -> OP-TEE CA
  -> TEAR TA
  -> secure persistent storage
```

Trusted operations currently include:

- model enrollment
- model verification
- model reporting
- model updates
- rollback rejection

The rollback policy is enforced inside the Trusted Application rather than
in normal-world runtime components.

TEAR validation now includes an end-to-end OP-TEE QEMU environment exercising
trusted model lifecycle operations through the secure world.

### MVP-7: OP-TEE QEMU adaptive MNIST

TEAR can run the adaptive MNIST sample inside the OP-TEE QEMU normal world.

This flow validates:

- real ARM64 ONNX Runtime MNIST inference
- optimizer daemon proposal handling
- runtime policy decision events
- OP-TEE-backed optimization decision recording
- read-back of the latest optimization decision from the TA

Run:

```bash
make qemu-optee-test
```

The test boots OP-TEE QEMU, runs the MNIST workload, records the optimization decision through the OP-TEE TA, reports it back, and verifies the event sequence.

## Roadmap

Near term:

- consolidate OP-TEE tests into a single QEMU boot
- support multiple workloads per supervisor run
- reduce QEMU test execution time
- simplify and clean up legacy initramfs/QEMU test infrastructure
- improve separation between host-native and OP-TEE execution paths

Future:

- hardware deployment
- RPMB-backed storage
- trusted optimization history
- signed optimization evidence
- fleet telemetry integration

## License

Licensed under the Apache License, Version 2.0.

See [LICENSE](LICENSE) for details.
