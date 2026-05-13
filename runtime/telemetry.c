#include "telemetry.h"

#include <stdio.h>
#include <time.h>

static long monotonic_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return -1;

    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

void tear_event(const char *event)
{
    printf("TEAR_EVENT ts_ms=%ld event=%s\n", monotonic_ms(), event);
    fflush(stdout);
}

void tear_event_kv(const char *event, const char *key, long value)
{
    printf("TEAR_EVENT ts_ms=%ld event=%s %s=%ld\n",
           monotonic_ms(), event, key, value);
    fflush(stdout);
}
