/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <tee_client_api.h>

#include "../ta/tear_ta/include/tear_ta.h"
#include "tear_optee_client.h"

int tear_optee_ping(void)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "TEAR_OPTEE_ERROR initialize_context res=0x%x\n", res);
		return -1;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr,
			"TEAR_OPTEE_ERROR open_session res=0x%x origin=0x%x\n",
			res, err_origin);
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_PING, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr,
			"TEAR_OPTEE_ERROR ping res=0x%x origin=0x%x\n",
			res, err_origin);
		TEEC_CloseSession(&sess);
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return 0;
}
