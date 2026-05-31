// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_TRUST_CLIENT_H
#define TEAR_TRUST_CLIENT_H

#include "model_manifest.h"

int tear_trust_enroll(
    const struct tear_model_manifest *manifest);

int tear_trust_verify(
    const struct tear_model_manifest *manifest);

int tear_trust_update_model(const struct tear_model_manifest *manifest);

int tear_trust_report(void);

int tear_trust_record_decision(const char *model_id,
                               const char *proposal,
                               const char *decision,
                               const char *reason,
                               long value);
#endif
