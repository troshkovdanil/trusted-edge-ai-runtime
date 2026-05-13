#!/usr/bin/env bash

# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

TELEMETRY="${TELEMETRY:-build/telemetry.log}"

make qemu-system

grep -q "event=supervisor_start" "$TELEMETRY"
grep -q "event=workload_start" "$TELEMETRY"
grep -q "event=workload_exit status=0" "$TELEMETRY"
grep -q "event=supervisor_shutdown" "$TELEMETRY"

echo "TEAR: qemu-system telemetry validation passed"
