/* SPDX-License-Identifier: Apache-2.0 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "tear_ta.h"

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
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
