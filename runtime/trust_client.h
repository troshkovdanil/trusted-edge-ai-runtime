// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_TRUST_CLIENT_H
#define TEAR_TRUST_CLIENT_H

#include "model_manifest.h"

int tear_trust_enroll(
    const struct tear_model_manifest *manifest);

int tear_trust_report(void);

#endif
