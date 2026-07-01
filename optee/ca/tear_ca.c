/* SPDX-License-Identifier: Apache-2.0 */

#include "observability.h"
#include "tear_optee_client.h"

#include <stdlib.h>

#define TEAR_COMPONENT "tear_optee_ca"

#ifdef TEAR_HOST_BUILD
#define TEAR_PLATFORM_DIR "build/platforms/host-mock"
#else
#define TEAR_PLATFORM_DIR "/tmp"
#endif
#define DEFAULT_EVENT_PATH TEAR_PLATFORM_DIR "/tear-optee-ca-events.log"

int main(void)
{
    int ret = EXIT_FAILURE;

    if (tear_event_init(DEFAULT_EVENT_PATH) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize OP-TEE CA events");
        return EXIT_FAILURE;
    }

    if (tear_optee_ping() != 0) {
        tear_event(TEAR_COMPONENT, "optee_ca_ping_failed");
        goto out;
    }

    tear_event(TEAR_COMPONENT, "optee_ca_ping_ok");

    ret = EXIT_SUCCESS;

out:
    tear_event_shutdown();
    return ret;
}
