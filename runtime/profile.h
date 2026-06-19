// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_PROFILE_H
#define TEAR_PROFILE_H

#include <stddef.h>

#define TEAR_PROFILE_ID_MAX          64
#define TEAR_PROFILE_ARTIFACT_ID_MAX 128
#define TEAR_PROFILE_BACKEND_MAX     64
#define TEAR_PROFILE_METRICS_PATH_MAX 256

struct tear_profile {
    char profile_id[TEAR_PROFILE_ID_MAX];
    char artifact_id[TEAR_PROFILE_ARTIFACT_ID_MAX];
    char backend[TEAR_PROFILE_BACKEND_MAX];
    char metrics_file_template[TEAR_PROFILE_METRICS_PATH_MAX];

    int allow_keep_current_profile;
    int allow_request_high_accuracy_profile;
    int allow_reject_input;
};

int tear_profile_load(const char *path,
                      struct tear_profile *profile);

void tear_profile_print(const struct tear_profile *profile);

#endif
