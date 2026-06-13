// SPDX-License-Identifier: Apache-2.0

#include "observability.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEAR_TELEMETRY_FILE_ENV "TEAR_TELEMETRY_FILE"

static long monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return -1;

    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static FILE *observability_stream(void)
{
    const char *path = getenv(TEAR_TELEMETRY_FILE_ENV);

    if (!path || path[0] == '\0')
        return stdout;

    FILE *f = fopen(path, "a");

    if (!f)
        return stdout;

    return f;
}

static void observability_close(FILE *f)
{
    if (f && f != stdout)
        fclose(f);
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
    FILE *out = observability_stream();
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
    observability_close(out);
}

void tear_event_ex(const char *component,
                   const char *workload,
                   const char *artifact_id,
                   const char *event)
{
    FILE *out = observability_stream();

    fprintf(out,
            "TEAR_EVENT ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " event=%s\n", event);

    fflush(out);
    observability_close(out);
}

void tear_event_ex_kv(const char *component,
                      const char *workload,
                      const char *artifact_id,
                      const char *event,
                      const char *key,
                      long value)
{
    FILE *out = observability_stream();

    fprintf(out,
            "TEAR_EVENT ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " event=%s %s=%ld\n", event, key, value);

    fflush(out);
    observability_close(out);
}

void tear_metric_long(const char *component,
                      const char *workload,
                      const char *artifact_id,
                      const char *name,
                      long value)
{
    FILE *out = observability_stream();

    fprintf(out,
            "TEAR_METRIC ts_ms=%ld component=%s",
            monotonic_ms(),
            component ? component : "unknown");

    write_optional_field(out, "workload", workload);
    write_optional_field(out, "artifact_id", artifact_id);

    fprintf(out, " name=%s value=%ld\n", name, value);

    fflush(out);
    observability_close(out);
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
