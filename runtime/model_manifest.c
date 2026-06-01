// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int extract_string(
    const char *buf,
    const char *key,
    char *out,
    size_t out_size)
{
    char pattern[64];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(buf, pattern);
    if (!p)
        return -1;

    p = strchr(p, ':');
    if (!p)
        return -1;

    p = strchr(p, '"');
    if (!p)
        return -1;

    p++;

    const char *end = strchr(p, '"');
    if (!end)
        return -1;

    size_t len = end - p;

    if (len >= out_size)
        len = out_size - 1;

    memcpy(out, p, len);
    out[len] = '\0';

    return 0;
}

static int extract_int(
    const char *buf,
    const char *key,
    int *value)
{
    char pattern[64];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(buf, pattern);
    if (!p)
        return -1;

    p = strchr(p, ':');
    if (!p)
        return -1;

    p++;

    *value = atoi(p);

    return 0;
}

static int extract_bool_optional(const char *buf,
                                 const char *key,
                                 int *value)
{
    char pattern[64];

    *value = 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(buf, pattern);
    if (!p)
        return 0;

    p = strchr(p, ':');
    if (!p)
        return -1;

    p++;

    while (*p == ' ' || *p == '\t' || *p == '\n')
        p++;

    if (strncmp(p, "true", 4) == 0) {
        *value = 1;
        return 0;
    }

    if (strncmp(p, "false", 5) == 0) {
        *value = 0;
        return 0;
    }

    return -1;
}

int tear_manifest_load(
    const char *path,
    struct tear_model_manifest *manifest)
{
    FILE *f;
    long size;
    char *buf;

    memset(manifest, 0, sizeof(*manifest));

    f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);

    buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }

    size_t nread = fread(buf, 1, size, f);

    if (nread != (size_t)size) {
        free(buf);
        fclose(f);
        return -1;
    }

    buf[size] = '\0';

    fclose(f);

    if (extract_string(buf, "model_id",
                       manifest->model_id,
                       sizeof(manifest->model_id)) < 0)
        goto fail;

    if (extract_int(buf, "version",
                    &manifest->version) < 0)
        goto fail;

    if (extract_string(buf, "backend",
                       manifest->backend,
                       sizeof(manifest->backend)) < 0)
        goto fail;

    if (extract_string(buf, "model_hash",
                       manifest->model_hash,
                       sizeof(manifest->model_hash)) < 0)
        goto fail;

    if (extract_bool_optional(buf, "optimization_capable",
                              &manifest->optimization_capable) < 0)
        goto fail;

    free(buf);

    return 0;

fail:
    free(buf);
    return -1;
}

void tear_manifest_print(
    const struct tear_model_manifest *manifest)
{
    printf("TEAR manifest:\n");
    printf("  model_id=%s\n", manifest->model_id);
    printf("  version=%d\n", manifest->version);
    printf("  backend=%s\n", manifest->backend);
    printf("  model_hash=%s\n", manifest->model_hash);
    printf("  optimization_capable=%s\n",
           manifest->optimization_capable ? "true" : "false");
}
