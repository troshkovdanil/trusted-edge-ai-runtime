# trusted-edge-ai-runtime

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

Run smoke test:

```bash
make test
make initramfs
file build/initramfs.cpio.gz
```

## MVP-1: qemu-system-aarch64

The project now builds a minimal initramfs containing the TEAR smoke-test binary.

The intended target is:

- qemu-system-aarch64
- virt machine
- minimal Linux kernel
- initramfs-based guest smoke test

The qemu-system smoke test currently uses a prebuilt Debian ARM64 installer kernel
as a temporary boot substrate. The kernel image is downloaded into `build/kernel/Image`
and is not committed to the repository.
