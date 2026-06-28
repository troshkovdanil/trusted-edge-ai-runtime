// SPDX-License-Identifier: Apache-2.0

#ifndef TEAR_MODEL_MANIFEST_H
#define TEAR_MODEL_MANIFEST_H

struct tear_model_manifest {
    char artifact_id[64];
    int version;
    char backend[32];
    char model_hash[128];
    int optimization_capable;
};

int tear_manifest_load(
    const char *path,
    struct tear_model_manifest *manifest);

void tear_manifest_print(
    const struct tear_model_manifest *manifest);

#endif
