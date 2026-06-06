/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "tear_ta.h"

#define TEAR_STATE_MAX 256

#define TEAR_STATE_OBJECT_ID "tear.trusted_state.v1"
#define TEAR_STATE_OBJECT_ID_LEN (sizeof(TEAR_STATE_OBJECT_ID) - 1)

#define TEAR_DECISION_MAX 512

#define TEAR_DECISION_OBJECT_ID "tear.last_decision.v1"
#define TEAR_DECISION_OBJECT_ID_LEN (sizeof(TEAR_DECISION_OBJECT_ID) - 1)

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

static TEE_Result write_trusted_decision(const void *buf, size_t len)
{
	TEE_ObjectHandle obj;
	TEE_Result res;

	if (len == 0 || len >= TEAR_DECISION_MAX)
		return TEE_ERROR_BAD_PARAMETERS;

	res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
					 TEAR_DECISION_OBJECT_ID,
					 TEAR_DECISION_OBJECT_ID_LEN,
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

struct tear_ta_state {
	char model_id[64];
	int version;
	char backend[32];
	char model_hash[128];
};

static int copy_token(char *dst, size_t dst_size,
		      const char **cursor)
{
	const char *start;
	size_t len;

	while (**cursor == ' ')
		(*cursor)++;

	start = *cursor;

	while (**cursor != '\0' && **cursor != ' ')
		(*cursor)++;

	len = (size_t)(*cursor - start);
	if (len == 0 || len >= dst_size)
		return -1;

	TEE_MemMove(dst, start, len);
	dst[len] = '\0';

	return 0;
}

static int parse_positive_int(const char *s, int *out)
{
	int value = 0;

	if (*s == '\0')
		return -1;

	while (*s != '\0') {
		if (*s < '0' || *s > '9')
			return -1;

		value = value * 10 + (*s - '0');
		s++;
	}

	*out = value;
	return 0;
}

static int parse_state(const char *buf, struct tear_ta_state *state)
{
	const char *cursor = buf;
	char version_buf[16];

	TEE_MemFill(state, 0, sizeof(*state));

	if (copy_token(state->model_id, sizeof(state->model_id), &cursor) < 0)
		return -1;

	if (copy_token(version_buf, sizeof(version_buf), &cursor) < 0)
		return -1;

	if (parse_positive_int(version_buf, &state->version) < 0)
		return -1;

	if (copy_token(state->backend, sizeof(state->backend), &cursor) < 0)
		return -1;

	if (copy_token(state->model_hash, sizeof(state->model_hash), &cursor) < 0)
		return -1;

	while (*cursor == ' ')
		cursor++;

	return *cursor == '\0' ? 0 : -1;
}

static bool update_allowed(const struct tear_ta_state *old,
			   const struct tear_ta_state *new)
{
	if (strcmp(old->model_id, new->model_id) != 0)
		return false;

	if (strcmp(old->backend, new->backend) != 0)
		return false;

	return new->version > old->version;
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

	case TEAR_TA_CMD_UPDATE: {
		char old_buf[TEAR_STATE_MAX];
		char new_buf[TEAR_STATE_MAX];
		size_t old_len = 0;
		struct tear_ta_state old_state;
		struct tear_ta_state new_state;
		TEE_Result res;

		DMSG("TEAR TA update");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (params[0].memref.size == 0 ||
		    params[0].memref.size >= sizeof(new_buf))
			return TEE_ERROR_BAD_PARAMETERS;

		TEE_MemMove(new_buf, params[0].memref.buffer, params[0].memref.size);
		new_buf[params[0].memref.size] = '\0';

		res = read_trusted_state(old_buf, sizeof(old_buf), &old_len);
		if (res != TEE_SUCCESS)
			return res;

		old_buf[old_len] = '\0';

		if (parse_state(old_buf, &old_state) < 0 ||
		    parse_state(new_buf, &new_state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (!update_allowed(&old_state, &new_state))
			return TEE_ERROR_SECURITY;

		res = write_trusted_state(new_buf, params[0].memref.size);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA update - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_REPORT: {
		char trusted_state[TEAR_STATE_MAX];
		size_t trusted_state_len = 0;
		TEE_Result res;

		DMSG("TEAR TA report");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		res = read_trusted_state(trusted_state,
					 sizeof(trusted_state),
					 &trusted_state_len);
		if (res != TEE_SUCCESS)
			return res;

		if (params[0].memref.size < trusted_state_len + 1) {
			params[0].memref.size = trusted_state_len + 1;
			return TEE_ERROR_SHORT_BUFFER;
		}

		TEE_MemMove(params[0].memref.buffer,
			    trusted_state,
			    trusted_state_len);
		((char *)params[0].memref.buffer)[trusted_state_len] = '\0';
		params[0].memref.size = trusted_state_len + 1;

		DMSG("TEAR TA report - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_RECORD_DECISION: {
		TEE_Result res;

		DMSG("TEAR TA record decision");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (params[0].memref.size == 0 ||
		    params[0].memref.size >= TEAR_DECISION_MAX)
			return TEE_ERROR_BAD_PARAMETERS;

		res = write_trusted_decision(params[0].memref.buffer,
					     params[0].memref.size);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA record decision - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
