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

## MVP-1: qemu-system-aarch64

The project builds a minimal ARM64 initramfs containing TEAR runtime
components and boots it under `qemu-system-aarch64`.

Current target environment:

- qemu-system-aarch64
- virt machine
- minimal Linux kernel
- initramfs-based guest runtime

The qemu-system smoke test currently uses a prebuilt Debian ARM64 installer
kernel as a temporary boot substrate.

The kernel image is downloaded into:

```text
build/kernel/Image
```

and is not committed to the repository.

## MVP-2: Runtime supervisor and telemetry

TEAR boots into a minimal init process that starts a runtime supervisor.

The supervisor:

- emits structured `TEAR_EVENT` telemetry
- launches workload binaries
- waits for workload completion
- reports workload exit status
- powers off the guest cleanly

Guest console output is captured into:

```text
build/telemetry.log
```

Example telemetry:

```text
TEAR_EVENT ts_ms=1486 event=supervisor_start
TEAR_EVENT ts_ms=1490 event=workload_start
TEAR_EVENT ts_ms=4538 event=workload_exit status=0
TEAR_EVENT ts_ms=4539 event=supervisor_shutdown
```

## MVP-3: Workload abstraction

TEAR now supports runtime-selectable workloads through the Linux kernel
command line.

The init process reads:

```text
tear.workload=<path>
```

from `/proc/cmdline` and forwards the selected workload to the TEAR
supervisor.

Current example workloads:

- `/bin/tear-hello`
- `/bin/demo-model`

Default workload:

```bash
make qemu-system
```

Mock model workload:

```bash
WORKLOAD=/bin/demo-model make qemu-system
```

The mock model demonstrates:

- runtime workload abstraction
- inference-style lifecycle telemetry
- workload-specific execution flow
- runtime parameter propagation through QEMU kernel cmdline

Example model workload output:

```text
TEAR_EVENT ts_ms=1514 event=model_init
TEAR model: loading model metadata
TEAR model: backend=mock
TEAR_EVENT ts_ms=2521 event=inference_start
TEAR model: input=synthetic-frame
TEAR model: running inference
TEAR_EVENT ts_ms=4525 event=inference_done
TEAR model: result=object:box confidence=0.87
```

The validation flow now verifies both workloads automatically:

```bash
make verify
```

## Repository Structure

```text
runtime/
  hello.c
  demo_model.c
  supervisor.c
  telemetry.c
  telemetry.h

initramfs/
  init.c

scripts/
  install-deps-ubuntu.sh
  fetch-qemu-kernel.sh
  run-qemu-system.sh
  verify-qemu-run.sh
```

## License

Licensed under the Apache License, Version 2.0.

See [LICENSE](LICENSE) for details.
