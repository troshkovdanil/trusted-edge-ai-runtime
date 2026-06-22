// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_TRUSTED_STATE_H
#define TEAR_TRUSTED_STATE_H

#include "model_manifest.h"

#include <stddef.h>

int tear_trusted_state_store(
    const char *path,
    const struct tear_model_manifest *manifest);

int tear_trusted_state_load(
    const char *path,
    struct tear_model_manifest *manifest);

int tear_trusted_state_append_decision(
    const char *path,
    const char *run_id,
    const char *artifact_id,
    const char *proposal,
    const char *decision,
    const char *reason,
    long value);

int tear_trusted_state_report_decision(
    const char *path,
    char *decision,
    size_t decision_size);

#endif
