// SPDX-License-Identifier: Apache-2.0

#include "optimizer_policy.h"

void tear_optimizer_propose(const struct tear_inference_metrics *metrics,
                            struct tear_optimizer_proposal *proposal)
{
    if (metrics->input_density_x1000 > 100) {
        proposal->action = "reject_input";
        proposal->reason = "input_quality_bad";
        return;
    }

    if (metrics->confidence_margin_x1000 < 500) {
        proposal->action = "request_high_accuracy_profile";
        proposal->reason = "low_confidence";
        return;
    }

    proposal->action = "keep_current_profile";
    proposal->reason = "confidence_ok";
}
