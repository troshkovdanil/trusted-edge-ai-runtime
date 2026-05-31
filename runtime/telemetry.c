// SPDX-License-Identifier: Apache-2.0

#include "telemetry.h"

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

static FILE *telemetry_stream(void)
{
    const char *path = getenv(TEAR_TELEMETRY_FILE_ENV);

    if (!path || path[0] == '\0')
        return stdout;

    FILE *f = fopen(path, "a");

    if (!f)
        return stdout;

    return f;
}

static void telemetry_close(FILE *f)
{
    if (f && f != stdout)
        fclose(f);
}

void tear_event(const char *event)
{
    FILE *out = telemetry_stream();

    fprintf(out, "TEAR_EVENT ts_ms=%ld event=%s\n",
            monotonic_ms(), event);
    fflush(out);

    telemetry_close(out);
}

void tear_event_kv(const char *event, const char *key, long value)
{
    FILE *out = telemetry_stream();

    fprintf(out, "TEAR_EVENT ts_ms=%ld event=%s %s=%ld\n",
            monotonic_ms(), event, key, value);
    fflush(out);

    telemetry_close(out);
}
