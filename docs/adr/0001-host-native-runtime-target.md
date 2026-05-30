# ADR 0001: Host-Native x86 Runtime Target

## Status

Accepted

## Context

TEAR aims to provide a trusted adaptive runtime infrastructure for embedded edge AI systems. The current development approach relies heavily on QEMU for ARM64 emulation, which is essential for maintaining reproducibility and ensuring that the runtime behaves as expected in an embedded environment.

However, developing and iterating on workloads and runtime components can be time-consuming when using QEMU. A host-native x86 target would allow developers to iterate more quickly on these components without the overhead of emulating ARM64.

## Decision

- **QEMU ARM64 remains canonical embedded validation path**: The existing QEMU-based validation will continue to be the primary method for ensuring that TEAR behaves correctly in an embedded environment.
  
- **Host-native x86 is for fast workload/runtime iteration**: A host-native x86 target will be added to allow developers to quickly iterate on workloads and runtime components. This target will facilitate faster development cycles without compromising the integrity of the trusted state or runtime management.

- **Host-native target must not bypass trusted-state/runtime-manager boundaries**: The host-native x86 environment must adhere to the same trust policies and runtime management rules as the QEMU ARM64 environment. This ensures that any changes made in the host-native environment can be safely ported to the embedded environment.

- **MNIST/adaptive workload development may start on host-native path before being ported into QEMU**: Initial development of workloads such as MNIST and adaptive runtime rules can begin on the host-native x86 target. Once these components are stable, they will be ported to the QEMU ARM64 environment for final validation.

## Consequences

- **Faster Development Cycles**: Developers can iterate more quickly on workloads and runtime components using the host-native x86 target.
  
- **Consistent Trust Policies**: The host-native environment adheres to the same trust policies as the QEMU ARM64 environment, ensuring that any changes made in the host-native environment are consistent with the embedded environment.

- **Separation of Development and Validation**: The host-native target is used for development and iteration, while QEMU remains the canonical validation path. This separation ensures that the integrity of the trusted state and runtime management is maintained.
  
- **Porting Workloads to QEMU**: Initial development on the host-native x86 target will be followed by porting workloads to the QEMU ARM64 environment for final validation.

## Next Steps

1. Implement the host-native x86 build configuration in the Makefile.
2. Ensure that the host-native environment adheres to the same trust policies and runtime management rules as the QEMU ARM64 environment.
3. Begin development of workloads such as MNIST on the host-native x86 target.
4. Port developed workloads to the QEMU ARM64 environment for final validation.

## References

- [TEAR Architecture Principles](AGENTS.md#architecture-principles)
- [TEAR Safety Invariants](docs/invariants.md)
