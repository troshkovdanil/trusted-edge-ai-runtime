// SPDX-License-Identifier: Apache-2.0

#include "observability.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_fp = NULL;
static FILE *event_fp = NULL;
static FILE *metric_fp = NULL;

void tear_log_init(FILE *fp)
{
    log_fp = fp ? fp : stdout;
}

int tear_event_init(const char *path)
{
    if (!path || path[0] == '\0')
        return -1;

    if (event_fp) {
        fclose(event_fp);
        event_fp = NULL;
    }

    event_fp = fopen(path, "a");
    return event_fp ? 0 : -1;
}

void tear_event_shutdown(void)
{
    if (event_fp) {
        fclose(event_fp);
        event_fp = NULL;
    }
}

int tear_metric_init(const char *path)
{
    if (!path || path[0] == '\0')
        return -1;

    if (metric_fp) {
        fclose(metric_fp);
        metric_fp = NULL;
    }

    metric_fp = fopen(path, "w");
    return metric_fp ? 0 : -1;
}

void tear_metric_shutdown(void)
{
    if (metric_fp) {
        fclose(metric_fp);
        metric_fp = NULL;
    }
}

static long monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return -1;

    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static FILE *log_stream(void)
{
    return log_fp ? log_fp : stdout;
}

static FILE *event_stream(void)
{
    return event_fp ? event_fp : stdout;
}

static FILE *metric_stream(void)
{
    return metric_fp ? metric_fp : stdout;
}

static const char *log_level_name(enum tear_log_level level)
{
    switch (level) {
    case TEAR_LOG_DEBUG:
        return "debug";
    case TEAR_LOG_INFO:
        return "info";
    case TEAR_LOG_WARN:
        return "warn";
    case TEAR_LOG_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static void write_optional_field(FILE *out,
                                 const char *key,
                                 const char *value)
{
    if (value && value[0] != '\0')
        fprintf(out, " %s=%s", key, value);
}

void tear_log(const char *component,
              const char *workload,
              const char *artifact_id,
              enum tear_log_level level,
              const char *fmt,
              ...)
{
    FILE *out = log_stream();
    va_list ap;

    fprintf(out,
            "TEAR_LOG ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " level=%s msg=\"", log_level_name(level));

    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fprintf(out, "\"\n");

    fflush(out);
}

void tear_event_ex(const char *component,
                   const char *workload,
                   const char *artifact_id,
                   const char *event)
{
    FILE *out = event_stream();

    fprintf(out,
            "TEAR_EVENT ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " event=%s\n", event);

    fflush(out);
}

void tear_event_ex_kv(const char *component,
                      const char *workload,
                      const char *artifact_id,
                      const char *event,
                      const char *key,
                      long value)
{
    FILE *out = event_stream();

    fprintf(out,
            "TEAR_EVENT ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " event=%s %s=%ld\n", event, key, value);

    fflush(out);
}

void tear_metric_long(const char *component,
                      const struct tear_profile *profile,
                      const char *name,
                      long value)
{
    FILE *out = metric_stream();

    if (!profile || profile->profile_id[0] == '\0' ||
        profile->artifact_id[0] == '\0') {
        return;
    }

    fprintf(out,
            "TEAR_METRIC ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    fprintf(out,
            " profile_id=%s artifact_id=%s",
            profile->profile_id,
            profile->artifact_id);

    write_optional_field(out, "backend", profile->backend);

    fprintf(out, " name=%s value=%ld\n", name, value);

    fflush(out);
}

/* Compatibility wrappers. */
void tear_event(const char *event)
{
    tear_event_ex("legacy", NULL, NULL, event);
}

void tear_event_kv(const char *event, const char *key, long value)
{
    tear_event_ex_kv("legacy", NULL, NULL, event, key, value);
}
