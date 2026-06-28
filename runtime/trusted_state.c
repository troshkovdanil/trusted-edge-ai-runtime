// SPDX-License-Identifier: Apache-2.0

#include "trusted_state.h"

#include "observability.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define TEAR_COMPONENT "trusted_state"
#define TEAR_STATE_LINE_MAX 512
#define TEAR_STATE_TMP_PATH_MAX 512

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
    return fprintf(f,
                   "%s %d %s %s\n",
                   m->artifact_id,
                   m->version,
                   m->backend,
                   m->model_hash) < 0 ? -1 : 0;
}

static int trusted_state_parse_manifest_line(
    const char *line,
    struct tear_model_manifest *manifest)
{
    memset(manifest, 0, sizeof(*manifest));

    return sscanf(line,
                  "%63s %d %63s %127s",
                  manifest->artifact_id,
                  &manifest->version,
                  manifest->backend,
                  manifest->model_hash) == 4 ? 0 : -1;
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
    if (!f) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to open decision store %s: %s",
                 path,
                 strerror(errno));
        return -1;
    }

    ret = trusted_state_write_decision(f,
                                       run_id,
                                       artifact_id,
                                       proposal,
                                       decision,
                                       reason,
                                       value);

    fclose(f);

    if (ret < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to write decision store %s",
                 path);
    }

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
    if (!f) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to open decision store %s: %s",
                 path,
                 strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        if (trusted_state_copy_line(last, sizeof(last), line) < 0) {
            fclose(f);
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "decision record too long in %s",
                     path);
            return -1;
        }
    }

    fclose(f);

    if (last[0] == '\0') {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "decision store is empty: %s",
                 path);
        return -1;
    }

    last[strcspn(last, "\n")] = '\0';

    return trusted_state_copy_line(decision, decision_size, last);
}

int tear_trusted_state_store(
    const char *path,
    const struct tear_model_manifest *manifest)
{
    FILE *in;
    FILE *out;
    char tmp_path[TEAR_STATE_TMP_PATH_MAX];
    char line[TEAR_STATE_LINE_MAX];
    int found = 0;
    int ret = 0;

    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >=
        (int)sizeof(tmp_path)) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "trusted state path too long: %s",
                 path);
        return -1;
    }

    in = fopen(path, "r");
    out = fopen(tmp_path, "w");

    if (!out) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to open trusted state temp %s: %s",
                 tmp_path,
                 strerror(errno));
        if (in)
            fclose(in);
        return -1;
    }

    if (in) {
        while (fgets(line, sizeof(line), in)) {
            struct tear_model_manifest existing;

            if (trusted_state_parse_manifest_line(line, &existing) < 0)
                continue;

            if (strcmp(existing.artifact_id, manifest->artifact_id) == 0) {
                if (trusted_state_write_manifest(out, manifest) < 0)
                    ret = -1;
                found = 1;
            } else {
                if (trusted_state_write_manifest(out, &existing) < 0)
                    ret = -1;
            }
        }

        fclose(in);
    }

    if (!found) {
        if (trusted_state_write_manifest(out, manifest) < 0)
            ret = -1;
    }

    fclose(out);

    if (ret < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to write trusted state %s",
                 path);
        remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to replace trusted state %s: %s",
                 path,
                 strerror(errno));
        remove(tmp_path);
        return -1;
    }

    return 0;
}

int tear_trusted_state_load_artifact(
    const char *path,
    const char *artifact_id,
    struct tear_model_manifest *manifest)
{
    FILE *f;
    char line[TEAR_STATE_LINE_MAX];

    f = fopen(path, "r");
    if (!f) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to open trusted state %s: %s",
                 path,
                 strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        struct tear_model_manifest candidate;

        if (trusted_state_parse_manifest_line(line, &candidate) < 0)
            continue;

        if (strcmp(candidate.artifact_id, artifact_id) == 0) {
            *manifest = candidate;
            fclose(f);
            return 0;
        }
    }

    fclose(f);

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "trusted state artifact not found: %s",
             artifact_id);

    return -1;
}

int tear_trusted_state_load(
    const char *path,
    struct tear_model_manifest *manifest)
{
    FILE *f;
    char line[TEAR_STATE_LINE_MAX];

    f = fopen(path, "r");
    if (!f) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to open trusted state %s: %s",
                 path,
                 strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        if (trusted_state_parse_manifest_line(line, manifest) == 0) {
            fclose(f);
            return 0;
        }
    }

    fclose(f);

    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "invalid trusted state: %s",
             path);

    return -1;
}
