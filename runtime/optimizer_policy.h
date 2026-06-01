// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_OPTIMIZER_POLICY_H
#define TEAR_OPTIMIZER_POLICY_H

struct tear_inference_metrics {
    long confidence_margin_x1000;
    long input_density_x1000;
};

struct tear_optimizer_proposal {
    const char *action;
    const char *reason;
};

void tear_optimizer_propose(const struct tear_inference_metrics *metrics,
                            struct tear_optimizer_proposal *proposal);

#endif
