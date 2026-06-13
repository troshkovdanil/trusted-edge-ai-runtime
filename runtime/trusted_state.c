// SPDX-License-Identifier: Apache-2.0

#include "trusted_state.h"

#include <stdio.h>
#include <string.h>

int tear_trusted_state_append_decision(const char *path,
                                       const char *artifact_id,
                                       const char *proposal,
                                       const char *decision,
                                       const char *reason,
                                       long value)
{
    FILE *f = fopen(path, "a");

    if (!f)
        return -1;

    fprintf(f,
            "artifact_id=%s proposal=%s decision=%s reason=%s value=%ld\n",
            artifact_id, proposal, decision, reason, value);

    fclose(f);
    return 0;
}

int tear_trusted_state_store(
    const char *path,
    const struct tear_model_manifest *manifest)
{
    FILE *f = fopen(path, "w");

    if (!f) {
        perror("fopen");
        return -1;
    }

    fprintf(f, "%s\n", manifest->artifact_id);
    fprintf(f, "%d\n", manifest->version);
    fprintf(f, "%s\n", manifest->backend);
    fprintf(f, "%s\n", manifest->model_hash);

    fclose(f);

    return 0;
}

int tear_trusted_state_load(
    const char *path,
    struct tear_model_manifest *manifest)
{
    FILE *f = fopen(path, "r");

    if (!f) {
        return -1;
    }

    memset(manifest, 0, sizeof(*manifest));

    if (!fgets(manifest->artifact_id,
               sizeof(manifest->artifact_id), f))
        goto fail;

    if (fscanf(f, "%d\n", &manifest->version) != 1)
        goto fail;

    if (!fgets(manifest->backend,
               sizeof(manifest->backend), f))
        goto fail;

    if (!fgets(manifest->model_hash,
               sizeof(manifest->model_hash), f))
        goto fail;

    manifest->artifact_id[
        strcspn(manifest->artifact_id, "\n")] = '\0';

    manifest->backend[
        strcspn(manifest->backend, "\n")] = '\0';

    manifest->model_hash[
        strcspn(manifest->model_hash, "\n")] = '\0';

    fclose(f);

    return 0;

fail:
    fclose(f);
    return -1;
}
