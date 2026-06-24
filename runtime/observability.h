// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "model_manifest.h"
#include "profile.h"

#include <stdio.h>

enum tear_log_level {
    TEAR_LOG_DEBUG,
    TEAR_LOG_INFO,
    TEAR_LOG_WARN,
    TEAR_LOG_ERROR,
};

void tear_log_init(FILE *fp);
int tear_event_init(const char *path);
int tear_metric_init(const char *path);
void tear_event_shutdown(void);
void tear_metric_shutdown(void);

void tear_log(const char *component,
              const char *workload,
              const char *artifact_id,
              enum tear_log_level level,
              const char *fmt,
              ...);

void tear_event(const char *component,
                   const char *event);

void tear_event_profile(const char *component,
                           const struct tear_profile *profile,
                           const char *event);

void tear_event_manifest(const char *component,
                            const struct tear_model_manifest *manifest,
                            const char *event);

void tear_event_kv(const char *component,
                      const char *event,
                      const char *key,
                      long value);

void tear_metric_long(const char *component,
                      const struct tear_profile *profile,
                      const char *name,
                      long value);
