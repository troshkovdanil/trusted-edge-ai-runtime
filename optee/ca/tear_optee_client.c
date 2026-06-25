/* SPDX-License-Identifier: Apache-2.0 */

#include "observability.h"
#include "tear_optee_client.h"

#include <stdio.h>
#include <string.h>

#include <tee_client_api.h>

#include "../ta/tear_ta/include/tear_ta.h"

#define TEAR_COMPONENT "tear_optee_client"

static void optee_client_error(const char *op,
			       TEEC_Result res,
			       uint32_t err_origin)
{
	tear_log(TEAR_COMPONENT,
		 TEAR_LOG_ERROR,
		 "TEAR_OPTEE_ERROR %s res=0x%x origin=0x%x",
		 op,
		 res,
		 err_origin);
}

static int tear_optee_result_to_errno(TEEC_Result res)
{
	if (res == TEEC_SUCCESS)
		return 0;

	if (res == TEEC_ERROR_SECURITY)
		return -2;

	return -1;
}

int tear_optee_ping(void)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		optee_client_error("initialize_context", res, err_origin);
		return -1;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		optee_client_error("open_session", res, err_origin);
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	res = TEEC_InvokeCommand(&sess, TEAR_TA_CMD_PING, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		optee_client_error("ping", res, err_origin);
		TEEC_CloseSession(&sess);
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return 0;
}

static int tear_optee_invoke_state_cmd(uint32_t cmd, const char *state)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		optee_client_error("initialize_context", res, err_origin);
		return -1;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		optee_client_error("open_session", res, err_origin);
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

	res = TEEC_InvokeCommand(&sess, cmd, &op, &err_origin);
	if (res != TEEC_SUCCESS)
		optee_client_error("invoke_state_cmd", res, err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return tear_optee_result_to_errno(res);
}

int tear_optee_enroll(const char *state)
{
	return tear_optee_invoke_state_cmd(TEAR_TA_CMD_ENROLL, state);
}

int tear_optee_verify(const char *state)
{
	return tear_optee_invoke_state_cmd(TEAR_TA_CMD_VERIFY, state);
}

int tear_optee_update(const char *state)
{
	return tear_optee_invoke_state_cmd(TEAR_TA_CMD_UPDATE, state);
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
	if (res != TEEC_SUCCESS) {
		optee_client_error("initialize_context", res, err_origin);
		return -1;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		optee_client_error("open_session", res, err_origin);
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
	if (res != TEEC_SUCCESS)
		optee_client_error("report", res, err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	if (res != TEEC_SUCCESS)
		return -1;

	state[state_size - 1] = '\0';
	return 0;
}

int tear_optee_record_decision(const char *run_id,
			       const char *artifact_id,
			       const char *proposal,
			       const char *decision,
			       const char *reason,
			       long value)
{
	char record[512];
	int n;

	if (!run_id || !artifact_id || !proposal || !decision || !reason)
		return -1;

	n = snprintf(record, sizeof(record),
		     "run_id=%s artifact_id=%s proposal=%s decision=%s reason=%s value=%ld",
		     run_id,
		     artifact_id,
		     proposal,
		     decision,
		     reason,
		     value);
	if (n < 0 || (size_t)n >= sizeof(record))
		return -1;

	return tear_optee_invoke_state_cmd(TEAR_TA_CMD_RECORD_DECISION, record);
}

int tear_optee_report_decision(char *decision, size_t decision_size)
{
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_Result res;
	TEEC_UUID uuid = TEAR_TA_UUID;
	uint32_t err_origin = 0;

	if (!decision || decision_size == 0)
		return -1;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS) {
		optee_client_error("initialize_context", res, err_origin);
		return -1;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		optee_client_error("open_session", res, err_origin);
		TEEC_FinalizeContext(&ctx);
		return -1;
	}

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);
	op.params[0].tmpref.buffer = decision;
	op.params[0].tmpref.size = decision_size;

	res = TEEC_InvokeCommand(&sess,
				 TEAR_TA_CMD_REPORT_DECISION,
				 &op,
				 &err_origin);
	if (res != TEEC_SUCCESS)
		optee_client_error("report_decision", res, err_origin);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	if (res != TEEC_SUCCESS)
		return -1;

	decision[decision_size - 1] = '\0';
	return 0;
}
