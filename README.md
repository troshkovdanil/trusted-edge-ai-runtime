# trusted-edge-ai-runtime

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

Trusted adaptive runtime infrastructure for embedded edge AI systems.

TEAR (Trusted Edge AI Runtime) focuses on:

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
- NPU unavailable
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

Build:

```bash
make build
```

Run validation:

```bash
make test
make qemu-system
make verify
```

Run the mock model workload:

```bash
WORKLOAD=/bin/demo-model make qemu-system
```

### Host-Native Development

For faster iteration on workloads and runtime components, TEAR provides a host-native x86 build target. This allows quick development cycles, while QEMU ARM64 remains the canonical embedded validation path.

```bash
make host-build
make host-test
make host-supervisor-test
```

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

## Next

The long-term goal is trusted optimization control for constrained edge AI:
runtime adaptation driven by telemetry, deployment integrity, and trusted state.

Next vertical slice:

```text
native x86 build
  -> real MNIST workload
  -> runtime metrics
  -> adaptive rules
  -> trusted optimization decisions
```

## License

Licensed under the Apache License, Version 2.0.

See [LICENSE](LICENSE) for details.
