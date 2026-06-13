// SPDX-License-Identifier: Apache-2.0

#pragma once

enum tear_log_level {
    TEAR_LOG_DEBUG,
    TEAR_LOG_INFO,
    TEAR_LOG_WARN,
    TEAR_LOG_ERROR,
};

void tear_log(const char *component,
              const char *workload,
              const char *artifact_id,
              enum tear_log_level level,
              const char *fmt,
              ...);

void tear_event_ex(const char *component,
                   const char *workload,
                   const char *artifact_id,
                   const char *event);

void tear_event_ex_kv(const char *component,
                      const char *workload,
                      const char *artifact_id,
                      const char *event,
                      const char *key,
                      long value);

void tear_metric_long(const char *component,
                      const char *workload,
                      const char *artifact_id,
                      const char *name,
                      long value);

/* Compatibility wrappers. */
void tear_event(const char *event);
void tear_event_kv(const char *event, const char *key, long value);
