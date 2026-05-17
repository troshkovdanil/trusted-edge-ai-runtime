# TEAR Agent Rules

TEAR is a trusted adaptive runtime infrastructure project for embedded edge AI systems.

This file defines mandatory rules for AI coding agents working on this repository.

Agents are implementation assistants, not system architects.

## Current project shape

Current core areas:

- `initramfs/`
  - minimal guest init process
  - starts TEAR runtime inside QEMU initramfs

- `runtime/`
  - `supervisor.*`: runtime orchestration
  - `telemetry.*`: structured TEAR_EVENT emission
  - `runtime_manager.*`: workload/runtime verification and launch path
  - `trustd.*`: trusted state service simulation
  - `trust_client.*`: client access to trusted state
  - `trusted_state.*`: trusted model state and rollback rules
  - `model_manifest.*`: model manifest parsing/validation
  - `tearictl.c`: control CLI
  - `demo_model.c`: mock model workload

- `examples/`
  - model manifests used by tests and initramfs

- `scripts/`
  - QEMU kernel fetch/run/verification helpers

- `Makefile`
  - canonical build and validation entry point

The public repo currently contains the same main project structure: `runtime/`, `initramfs/`, `scripts/`, `examples/`, `Makefile`, `README.md`, and Apache-2.0 license. :contentReference[oaicite:0]{index=0}

## Role of AI agents

Agents may:

- implement small scoped changes
- add tests
- improve scripts
- improve diagnostics
- update documentation
- refactor locally when behavior is preserved

Agents must not:

- silently redesign architecture
- introduce new subsystem ownership
- bypass existing trust/runtime boundaries
- remove validation without replacing it
- make broad rewrites without explicit human approval

## Architecture principles

### Trusted state

`trustd` owns trusted persistent model/deployment state.

No other subsystem should directly own trusted state.

Current simulation may use local files or simple in-repo mechanisms, but the architecture must remain compatible with future OP-TEE/RPMB-backed storage.

### Runtime control

`runtime-manager` owns workload/model verification and runtime activation logic.

Other components must not bypass `runtime-manager` to launch trusted model workloads.

### Supervision

`supervisor` coordinates runtime execution:

- emits lifecycle telemetry
- starts workloads
- observes exit status
- shuts down the guest cleanly

### Telemetry

`telemetry` owns structured `TEAR_EVENT` formatting.

New runtime-visible behavior should emit explicit telemetry events when useful.

Telemetry must observe/report state. It must not mutate trusted state.

### Optimizer / future agent logic

Future optimizer or AI-agent components may suggest runtime actions.

They must not directly mutate trusted deployment state.

Core rule:

```text
optimizer/agent suggests
TEAR policy/trust layer enforces
runtime-manager applies allowed action
telemetry records result
```

### Safety invariants

Agents must preserve these invariants:

trusted model version must not roll back silently
rollback rejection must remain testable
runtime activation must depend on trusted state validation
telemetry event names must remain stable unless README/tests are updated
QEMU validation must remain reproducible
build artifacts must stay out of git unless explicitly intended
generated code must be simple C/shell/Makefile unless another language is explicitly approved
no hidden network dependency may be added to normal validation
no external service may be required for make verify
Forbidden changes

Do not introduce:

direct OP-TEE access outside future trustd/trust backend layer
duplicated trusted model state in supervisor/runtime-manager
duplicated telemetry formatting outside telemetry.*
hidden cross-subsystem coupling
background daemons not started through explicit runtime flow
new global mutable state without clear ownership
new dependencies without README/installer update
opaque generated code
large framework dependencies
autonomous self-modifying agent behavior
Required validation

### Before proposing a patch as complete, run:

make build
make test
make qemu-system
make verify

If a target fails, report:

exact command
relevant log lines
suspected cause
minimal proposed fix

Do not claim success without running validation.

### Patch style

Prefer small patches.

Good patch examples:

add one telemetry event
add one manifest field
add one verification check
add one QEMU test case
refactor one file with no behavior change
improve one script

Bad patch examples:

redesign trust model
replace build system
introduce large framework
rewrite supervisor/runtime/trustd together
add optimizer before policy boundary exists
Documentation rules

When behavior changes, update relevant documentation:

README.md for user-visible project behavior
AGENTS.md for agent workflow rules
future docs/architecture.md for subsystem design
future docs/invariants.md for safety rules
future docs/adr/ for architectural decisions
Commit discipline

Agents should not commit directly unless explicitly requested.

### Recommended workflow:

```
git checkout -b agent/<short-task-name>
# agent edits
make build
make test
make qemu-system
make verify
git diff
# human reviews
```

### Human authority

The human maintainer owns:

architecture
trust model
subsystem boundaries
security assumptions
release decisions
merge decisions

AI agents provide implementation acceleration only.
