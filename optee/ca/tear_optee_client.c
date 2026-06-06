/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

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

int tear_optee_enroll(const char *state)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		return -1;

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = (void *)state;
	op.params[0].tmpref.size = strlen(state);

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_ENROLL, &op, &err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return res == TEEC_SUCCESS ? 0 : -1;
}

int tear_optee_verify(const char *state)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		return -1;

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = (void *)state;
	op.params[0].tmpref.size = strlen(state);

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_VERIFY, &op, &err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return res == TEEC_SUCCESS ? 0 : -1;
}

int tear_optee_update(const char *state)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		return -1;

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = (void *)state;
	op.params[0].tmpref.size = strlen(state);

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_UPDATE, &op, &err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return res == TEEC_SUCCESS ? 0 : -1;
}

int tear_optee_report(char *state, size_t state_size)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	if (!state || state_size == 0)
		return -1;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		return -1;

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = state;
	op.params[0].tmpref.size = state_size;

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_REPORT, &op, &err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	if (res != TEEC_SUCCESS)
		return -1;

	state[state_size - 1] = '\0';
	return 0;
}
