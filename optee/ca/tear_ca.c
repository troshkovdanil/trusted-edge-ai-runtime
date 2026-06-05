/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include <tee_client_api.h>

#include "../ta/tear_ta/include/tear_ta.h"

int main(void)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "TEAR_OPTEE_CA_ERROR initialize_context res=0x%x\n", res);
		return EXIT_FAILURE;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr,
			"TEAR_OPTEE_CA_ERROR open_session res=0x%x origin=0x%x\n",
			res, err_origin);
		TEEC_FinalizeContext(&ctx);
		return EXIT_FAILURE;
	}

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_PING,
				 NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr,
			"TEAR_OPTEE_CA_ERROR ping res=0x%x origin=0x%x\n",
			res, err_origin);
		TEEC_CloseSession(&sess);
		TEEC_FinalizeContext(&ctx);
		return EXIT_FAILURE;
	}

	printf("TEAR_OPTEE_CA_PING_OK\n");

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return EXIT_SUCCESS;
}
