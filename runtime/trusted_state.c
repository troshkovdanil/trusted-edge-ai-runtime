// SPDX-License-Identifier: Apache-2.0

#include "trusted_state.h"

#include <stdio.h>
#include <string.h>

static int trusted_state_write_decision(FILE *f,
                                        const char *run_id,
                                        const char *artifact_id,
                                        const char *proposal,
                                        const char *decision,
                                        const char *reason,
                                        long value)
{
    return fprintf(f,
                   "run_id=%s artifact_id=%s proposal=%s decision=%s reason=%s value=%ld\n",
                   run_id,
                   artifact_id,
                   proposal,
                   decision,
                   reason,
                   value) < 0 ? -1 : 0;
}

static int trusted_state_write_manifest(FILE *f,
                                        const struct tear_model_manifest *m)
{
    if (fprintf(f, "%s\n", m->artifact_id) < 0)
        return -1;

    if (fprintf(f, "%d\n", m->version) < 0)
        return -1;

    if (fprintf(f, "%s\n", m->backend) < 0)
        return -1;

    if (fprintf(f, "%s\n", m->model_hash) < 0)
        return -1;

    return 0;
}

static int trusted_state_copy_line(char *dst,
                                   size_t dst_size,
                                   const char *src)
{
    int n;

    if (!dst || dst_size == 0 || !src)
        return -1;

    n = snprintf(dst, dst_size, "%s", src);

    return n >= 0 && (size_t)n < dst_size ? 0 : -1;
}

int tear_trusted_state_append_decision(const char *path,
                                       const char *run_id,
                                       const char *artifact_id,
                                       const char *proposal,
                                       const char *decision,
                                       const char *reason,
                                       long value)
{
    FILE *f;
    int ret;

    f = fopen(path, "a");
    if (!f)
        return -1;

    ret = trusted_state_write_decision(f,
                                       run_id,
                                       artifact_id,
                                       proposal,
                                       decision,
                                       reason,
                                       value);

    fclose(f);

    return ret;
}

int tear_trusted_state_report_decision(const char *path,
                                       char *decision,
                                       size_t decision_size)
{
    FILE *f;
    char line[512];
    char last[512] = "";

    if (!decision || decision_size == 0)
        return -1;

    f = fopen(path, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        if (trusted_state_copy_line(last, sizeof(last), line) < 0) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    if (last[0] == '\0')
        return -1;

    last[strcspn(last, "\n")] = '\0';

    return trusted_state_copy_line(decision, decision_size, last);
}

int tear_trusted_state_store(
    const char *path,
    const struct tear_model_manifest *manifest)
{
    FILE *f;
    int ret;

    f = fopen(path, "w");
    if (!f)
        return -1;

    ret = trusted_state_write_manifest(f, manifest);

    fclose(f);

    return ret;
}

int tear_trusted_state_load(
    const char *path,
    struct tear_model_manifest *manifest)
{
    FILE *f = fopen(path, "r");

    if (!f)
        return -1;

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
