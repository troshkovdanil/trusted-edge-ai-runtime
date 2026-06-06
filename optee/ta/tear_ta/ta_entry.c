/* SPDX-License-Identifier: Apache-2.0 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "tear_ta.h"

#define TEAR_STATE_MAX 256

static char enrolled_state[TEAR_STATE_MAX];
static size_t enrolled_state_len;

TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("TEAR TA create");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	DMSG("TEAR TA destroy");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
				    TEE_Param params[4],
				    void **sess_ctx)
{
	(void)param_types;
	(void)params;
	(void)sess_ctx;

	DMSG("TEAR TA open session");
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *sess_ctx)
{
	(void)sess_ctx;

	DMSG("TEAR TA close session");
}

TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx,
				      uint32_t cmd_id,
				      uint32_t param_types,
				      TEE_Param params[4])
{
	(void)sess_ctx;
	(void)param_types;
	(void)params;

	switch (cmd_id) {
	case TEAR_TA_CMD_PING:
		DMSG("TEAR TA ping");
		return TEE_SUCCESS;
	case TEAR_TA_CMD_ENROLL:
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (params[0].memref.size >= TEAR_STATE_MAX)
			return TEE_ERROR_BAD_PARAMETERS;

		TEE_MemMove(enrolled_state, params[0].memref.buffer, params[0].memref.size);
		enrolled_state[params[0].memref.size] = '\0';
		enrolled_state_len = params[0].memref.size;

		DMSG("TEAR TA enroll");
		return TEE_SUCCESS;
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
