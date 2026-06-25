/* SPDX-License-Identifier: Apache-2.0 */

#include "observability.h"
#include "tear_optee_client.h"

#include <stdlib.h>

#define TEAR_COMPONENT "tear_optee_ca"

int main(void)
{
    if (tear_optee_ping() != 0) {
        tear_event(TEAR_COMPONENT, "optee_ca_ping_failed");
        return EXIT_FAILURE;
    }

    tear_event(TEAR_COMPONENT, "optee_ca_ping_ok");

    return EXIT_SUCCESS;
}
