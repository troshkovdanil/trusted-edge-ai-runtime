# TEAR Safety Invariants

## Trusted Model Version Rollback
- **Invariant**: The trusted model version must not roll back silently.
- **Validation**: Ensure that rollback attempts are logged and rejected if they violate the trust policy.

## Testable Rollback Rejection
- **Invariant**: Rollback rejection must remain testable.
- **Validation**: Verify that rollback attempts are correctly identified and rejected in tests.

## Runtime Activation Dependency on Trusted State
- **Invariant**: Runtime activation must depend on trusted state validation.
- **Validation**: Ensure that workloads/models are only activated after successful verification by the runtime manager.

## Stable Telemetry Event Names
- **Invariant**: Telemetry event names must remain stable unless README/tests are updated.
- **Validation**: Check that telemetry events do not change unexpectedly and update documentation accordingly.

## Reproducible QEMU Validation
- **Invariant**: QEMU validation must remain reproducible.
- **Validation**: Ensure that the QEMU environment is consistent across different runs and that validation scripts produce predictable results.

## Build Artifacts Out of Git
- **Invariant**: Build artifacts must stay out of git unless explicitly intended.
- **Validation**: Verify that build outputs are not committed to the repository and update `.gitignore` if necessary.

## Simple Generated Code
- **Invariant**: Generated code must be simple C/shell/Makefile unless another language is explicitly approved.
- **Validation**: Ensure that generated code adheres to simplicity guidelines and does not introduce complex dependencies.

## No Hidden Network Dependencies
- **Invariant**: No hidden network dependency may be added to normal validation.
- **Validation**: Check that all network interactions are explicit and documented, and ensure they do not affect the reproducibility of validation.

## No External Service Requirement for `make verify`
- **Invariant**: No external service may be required for `make verify`.
- **Validation**: Ensure that the verification process does not depend on any external services and can run independently.
