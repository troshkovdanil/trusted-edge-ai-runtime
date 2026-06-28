// SPDX-License-Identifier: Apache-2.0

#include "profile.h"
#include "observability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEAR_COMPONENT "profile"

static void trim_newline(char *s)
{
    size_t len = strlen(s);

    while (len > 0 &&
           (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static void parse_bool(int *dst, const char *value)
{
    *dst = (strcmp(value, "allowed") == 0);
}

static int append_str(char *dst,
                      size_t dst_size,
                      size_t *pos,
                      const char *src)
{
    size_t len = strlen(src);

    if (*pos + len >= dst_size)
        return -1;

    memcpy(dst + *pos, src, len);
    *pos += len;
    dst[*pos] = '\0';

    return 0;
}

static int substitute_metrics_template(struct tear_profile *profile,
                                       const char *template)
{
    char result[sizeof(profile->metrics_file_template)];
    size_t pos = 0;
    const char *p = template;

    result[0] = '\0';

    while (*p) {
        if (strncmp(p, "{artifact_id}", strlen("{artifact_id}")) == 0) {
            if (append_str(result,
                           sizeof(result),
                           &pos,
                           profile->artifact_id) < 0)
                return -1;

            p += strlen("{artifact_id}");
            continue;
        }

        if (strncmp(p, "{profile_id}", strlen("{profile_id}")) == 0) {
            if (append_str(result,
                           sizeof(result),
                           &pos,
                           profile->profile_id) < 0)
                return -1;

            p += strlen("{profile_id}");
            continue;
        }

        if (pos + 1 >= sizeof(result))
            return -1;

        result[pos++] = *p++;
        result[pos] = '\0';
    }

    memcpy(profile->metrics_file_template,
           result,
           sizeof(profile->metrics_file_template));

    return 0;
}

int tear_profile_load(const char *path,
                      struct tear_profile *profile)
{
    FILE *fp;
    char line[512];
    char metrics_template[sizeof(profile->metrics_file_template)] = "";

    memset(profile, 0, sizeof(*profile));

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        char *key;
        char *value;
        char *eq;

        trim_newline(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';

        key = line;
        value = eq + 1;

        if (strcmp(key, "profile_id") == 0) {
            strncpy(profile->profile_id,
                    value,
                    sizeof(profile->profile_id) - 1);
        } else if (strcmp(key, "artifact_id") == 0) {
            strncpy(profile->artifact_id,
                    value,
                    sizeof(profile->artifact_id) - 1);
        } else if (strcmp(key, "backend") == 0) {
            strncpy(profile->backend,
                    value,
                    sizeof(profile->backend) - 1);
        } else if (strcmp(key, "metrics_file_template") == 0) {
            strncpy(metrics_template,
                    value,
                    sizeof(metrics_template) - 1);
        } else if (strcmp(key,
                          "adaptation.keep_current_profile") == 0) {
            parse_bool(&profile->allow_keep_current_profile,
                       value);
        } else if (strcmp(key,
                          "adaptation.request_high_accuracy_profile") == 0) {
            parse_bool(&profile->allow_request_high_accuracy_profile,
                       value);
        } else if (strcmp(key,
                          "adaptation.reject_input") == 0) {
            parse_bool(&profile->allow_reject_input,
                       value);
        }
    }

    fclose(fp);

    if (profile->profile_id[0] == '\0')
        return -1;

    if (profile->artifact_id[0] == '\0')
        return -1;

    if (profile->backend[0] == '\0')
        return -1;

    if (metrics_template[0] == '\0')
        return -1;

    if (substitute_metrics_template(profile, metrics_template) < 0)
        return -1;

    return 0;
}

void tear_profile_print(const struct tear_profile *profile)
{
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO, "TEAR profile:");
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "profile_id=%s", profile->profile_id);
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "artifact_id=%s", profile->artifact_id);
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "backend=%s", profile->backend);
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "metrics_file_template=%s",
             profile->metrics_file_template);
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "adaptation.keep_current_profile=%s",
             profile->allow_keep_current_profile ?
             "allowed" : "disabled");
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "adaptation.request_high_accuracy_profile=%s",
             profile->allow_request_high_accuracy_profile ?
             "allowed" : "disabled");
    tear_log(TEAR_COMPONENT, TEAR_LOG_INFO,
             "adaptation.reject_input=%s",
             profile->allow_reject_input ? "allowed" : "disabled");
}
