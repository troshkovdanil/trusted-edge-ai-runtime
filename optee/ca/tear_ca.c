/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include "tear_optee_client.h"

int main(void)
{
	if (tear_optee_ping() != 0)
		return EXIT_FAILURE;

	printf("TEAR_OPTEE_CA_PING_OK\n");
	return EXIT_SUCCESS;
}
