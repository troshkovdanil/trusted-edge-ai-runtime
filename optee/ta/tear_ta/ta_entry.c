/* SPDX-License-Identifier: Apache-2.0 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "tear_ta.h"

#define TEAR_STATE_MAX 256

#define TEAR_STATE_OBJECT_ID "tear.trusted_state.v1"
#define TEAR_STATE_OBJECT_ID_LEN (sizeof(TEAR_STATE_OBJECT_ID) - 1)

static TEE_Result write_trusted_state(const void *buf, size_t len)
{
	TEE_ObjectHandle obj;
	TEE_Result res;

	if (len == 0 || len >= TEAR_STATE_MAX)
		return TEE_ERROR_BAD_PARAMETERS;

	res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
					 TEAR_STATE_OBJECT_ID,
					 TEAR_STATE_OBJECT_ID_LEN,
					 TEE_DATA_FLAG_ACCESS_READ |
					 TEE_DATA_FLAG_ACCESS_WRITE |
					 TEE_DATA_FLAG_ACCESS_WRITE_META |
					 TEE_DATA_FLAG_OVERWRITE,
					 TEE_HANDLE_NULL,
					 buf,
					 len,
					 &obj);
	if (res != TEE_SUCCESS)
		return res;

	TEE_CloseObject(obj);
	return TEE_SUCCESS;
}

static TEE_Result read_trusted_state(void *buf, size_t buf_len, size_t *out_len)
{
	TEE_ObjectHandle obj;
	TEE_ObjectInfo info;
	TEE_Result res;
	size_t read_len = 0;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
				       TEAR_STATE_OBJECT_ID,
				       TEAR_STATE_OBJECT_ID_LEN,
				       TEE_DATA_FLAG_ACCESS_READ,
				       &obj);
	if (res != TEE_SUCCESS)
		return res;

	res = TEE_GetObjectInfo1(obj, &info);
	if (res != TEE_SUCCESS) {
		TEE_CloseObject(obj);
		return res;
	}

	if (info.dataSize == 0 || info.dataSize >= buf_len) {
		TEE_CloseObject(obj);
		return TEE_ERROR_SHORT_BUFFER;
	}

	res = TEE_ReadObjectData(obj, buf, info.dataSize, &read_len);
	TEE_CloseObject(obj);

	if (res != TEE_SUCCESS)
		return res;

	if (read_len != info.dataSize)
		return TEE_ERROR_CORRUPT_OBJECT;

	*out_len = read_len;
	return TEE_SUCCESS;
}

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

	case TEAR_TA_CMD_ENROLL: {
		TEE_Result res;

		DMSG("TEAR TA enroll");
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		DMSG("TEAR TA enroll persistent");
		res = write_trusted_state(params[0].memref.buffer,
				params[0].memref.size);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA enroll - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_VERIFY: {
		char trusted_state[TEAR_STATE_MAX];
		size_t trusted_state_len = 0;
		TEE_Result res;

		DMSG("TEAR TA verify");
		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		res = read_trusted_state(trusted_state,
				sizeof(trusted_state),
				&trusted_state_len);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA verify persistent: trusted_state_len=%zu",
				trusted_state_len);

		if (params[0].memref.size != trusted_state_len)
			return TEE_ERROR_SECURITY;

		if (TEE_MemCompare(trusted_state,
					params[0].memref.buffer,
					trusted_state_len) != 0)
			return TEE_ERROR_SECURITY;

		DMSG("TEAR TA verify - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
