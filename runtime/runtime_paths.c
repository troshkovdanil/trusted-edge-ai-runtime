// SPDX-License-Identifier: Apache-2.0

#include "runtime_paths.h"

#include <stdlib.h>

#define TEAR_TRUSTD_SOCKET_DEFAULT "/tmp/tear-trustd.sock"
#define TEAR_OPTD_SOCKET_DEFAULT "/tmp/tear-optd.sock"

static const char *env_or_default(const char *env_name,
                                  const char *default_value)
{
    const char *value = getenv(env_name);

    if (value && value[0] != '\0')
        return value;

    return default_value;
}

const char *tear_trustd_socket_path(void)
{
    return env_or_default("TEAR_TRUSTD_SOCKET",
                          TEAR_TRUSTD_SOCKET_DEFAULT);
}

const char *tear_optd_socket_path(void)
{
    return env_or_default("TEAR_OPTD_SOCKET",
                          TEAR_OPTD_SOCKET_DEFAULT);
}
