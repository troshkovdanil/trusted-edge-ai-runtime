/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "tear_ta.h"

#define TEAR_STATE_RECORD_MAX 256
#define TEAR_STATE_DB_MAX 2048

#define TEAR_STATE_OBJECT_ID "tear.trusted_state.v1"
#define TEAR_STATE_OBJECT_ID_LEN (sizeof(TEAR_STATE_OBJECT_ID) - 1)

#define TEAR_DECISION_MAX 512

#define TEAR_DECISION_OBJECT_ID "tear.last_decision.v1"
#define TEAR_DECISION_OBJECT_ID_LEN (sizeof(TEAR_DECISION_OBJECT_ID) - 1)

struct tear_ta_state {
	char artifact_id[64];
	int version;
	char backend[32];
	char model_hash[128];
};

static TEE_Result write_trusted_state_db(const void *buf, size_t len)
{
	TEE_ObjectHandle obj;
	TEE_Result res;

	if (len == 0 || len >= TEAR_STATE_DB_MAX)
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

static TEE_Result read_trusted_state_db(void *buf,
					size_t buf_len,
					size_t *out_len)
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

static TEE_Result read_trusted_decision(void *buf, size_t buf_len,
					size_t *out_len)
{
	TEE_ObjectHandle obj;
	TEE_ObjectInfo info;
	TEE_Result res;
	size_t read_len = 0;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
				       TEAR_DECISION_OBJECT_ID,
				       TEAR_DECISION_OBJECT_ID_LEN,
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

	if (copy_token(state->artifact_id, sizeof(state->artifact_id), &cursor) < 0)
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

static bool same_state(const struct tear_ta_state *a,
		       const struct tear_ta_state *b)
{
	return strcmp(a->artifact_id, b->artifact_id) == 0 &&
	       a->version == b->version &&
	       strcmp(a->backend, b->backend) == 0 &&
	       strcmp(a->model_hash, b->model_hash) == 0;
}

static bool update_allowed(const struct tear_ta_state *old,
			   const struct tear_ta_state *new)
{
	if (strcmp(old->artifact_id, new->artifact_id) != 0)
		return false;

	if (strcmp(old->backend, new->backend) != 0)
		return false;

	return new->version > old->version;
}

static int copy_input_state(const TEE_Param *param,
			    char *state,
			    size_t state_size)
{
	if (param->memref.size == 0 || param->memref.size >= state_size)
		return -1;

	TEE_MemMove(state, param->memref.buffer, param->memref.size);
	state[param->memref.size] = '\0';

	return 0;
}

static int next_state_record(const char *db,
			     size_t db_len,
			     size_t *offset,
			     char *record,
			     size_t record_size)
{
	size_t start;
	size_t len;

	while (*offset < db_len &&
	       (db[*offset] == '\n' || db[*offset] == '\r'))
		(*offset)++;

	if (*offset >= db_len)
		return 0;

	start = *offset;

	while (*offset < db_len &&
	       db[*offset] != '\n' &&
	       db[*offset] != '\r')
		(*offset)++;

	len = *offset - start;
	if (len == 0 || len >= record_size)
		return -1;

	TEE_MemMove(record, db + start, len);
	record[len] = '\0';

	return 1;
}

static int append_record(char *db,
			 size_t *db_len,
			 const char *record,
			 size_t record_len)
{
	if (record_len == 0 || record_len >= TEAR_STATE_RECORD_MAX)
		return -1;

	if (*db_len + record_len + 1 >= TEAR_STATE_DB_MAX)
		return -1;

	TEE_MemMove(db + *db_len, record, record_len);
	*db_len += record_len;

	db[*db_len] = '\n';
	(*db_len)++;

	return 0;
}

static TEE_Result find_trusted_state(const char *artifact_id,
				     char *trusted,
				     size_t trusted_size)
{
	char db[TEAR_STATE_DB_MAX];
	size_t db_len = 0;
	size_t offset = 0;
	TEE_Result res;

	res = read_trusted_state_db(db, sizeof(db), &db_len);
	if (res != TEE_SUCCESS)
		return res;

	while (offset < db_len) {
		char record[TEAR_STATE_RECORD_MAX];
		struct tear_ta_state state;
		int ret;

		ret = next_state_record(db,
					db_len,
					&offset,
					record,
					sizeof(record));
		if (ret < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (ret == 0)
			break;

		if (parse_state(record, &state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (strcmp(state.artifact_id, artifact_id) == 0) {
			size_t len = strlen(record);

			if (len == 0 || len >= trusted_size)
				return TEE_ERROR_SHORT_BUFFER;

			TEE_MemMove(trusted, record, len);
			trusted[len] = '\0';

			return TEE_SUCCESS;
		}
	}

	return TEE_ERROR_ITEM_NOT_FOUND;
}

static TEE_Result upsert_trusted_state(const char *incoming)
{
	char db[TEAR_STATE_DB_MAX];
	char out[TEAR_STATE_DB_MAX];
	size_t db_len = 0;
	size_t out_len = 0;
	size_t offset = 0;
	size_t incoming_len = strlen(incoming);
	struct tear_ta_state incoming_state;
	bool found = false;
	TEE_Result res;

	if (incoming_len == 0 || incoming_len >= TEAR_STATE_RECORD_MAX)
		return TEE_ERROR_BAD_PARAMETERS;

	if (parse_state(incoming, &incoming_state) < 0)
		return TEE_ERROR_BAD_FORMAT;

	TEE_MemFill(db, 0, sizeof(db));
	TEE_MemFill(out, 0, sizeof(out));

	res = read_trusted_state_db(db, sizeof(db), &db_len);
	if (res != TEE_SUCCESS && res != TEE_ERROR_ITEM_NOT_FOUND)
		return res;

	if (res == TEE_SUCCESS) {
		while (offset < db_len) {
			char record[TEAR_STATE_RECORD_MAX];
			struct tear_ta_state existing_state;
			int ret;

			ret = next_state_record(db,
						db_len,
						&offset,
						record,
						sizeof(record));
			if (ret < 0)
				return TEE_ERROR_BAD_FORMAT;

			if (ret == 0)
				break;

			if (parse_state(record, &existing_state) < 0)
				return TEE_ERROR_BAD_FORMAT;

			if (strcmp(existing_state.artifact_id,
				   incoming_state.artifact_id) == 0) {
				if (append_record(out,
						  &out_len,
						  incoming,
						  incoming_len) < 0)
					return TEE_ERROR_SHORT_BUFFER;

				found = true;
			} else {
				if (append_record(out,
						  &out_len,
						  record,
						  strlen(record)) < 0)
					return TEE_ERROR_SHORT_BUFFER;
			}
		}
	}

	if (!found) {
		if (append_record(out, &out_len, incoming, incoming_len) < 0)
			return TEE_ERROR_SHORT_BUFFER;
	}

	return write_trusted_state_db(out, out_len);
}

static TEE_Result latest_trusted_state(char *state, size_t state_size,
				       size_t *state_len)
{
	char db[TEAR_STATE_DB_MAX];
	char latest[TEAR_STATE_RECORD_MAX];
	size_t db_len = 0;
	size_t offset = 0;
	size_t latest_len = 0;
	TEE_Result res;

	res = read_trusted_state_db(db, sizeof(db), &db_len);
	if (res != TEE_SUCCESS)
		return res;

	TEE_MemFill(latest, 0, sizeof(latest));

	while (offset < db_len) {
		char record[TEAR_STATE_RECORD_MAX];
		int ret;

		ret = next_state_record(db,
					db_len,
					&offset,
					record,
					sizeof(record));
		if (ret < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (ret == 0)
			break;

		latest_len = strlen(record);
		if (latest_len == 0 || latest_len >= sizeof(latest))
			return TEE_ERROR_BAD_FORMAT;

		TEE_MemMove(latest, record, latest_len);
		latest[latest_len] = '\0';
	}

	if (latest_len == 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (latest_len >= state_size)
		return TEE_ERROR_SHORT_BUFFER;

	TEE_MemMove(state, latest, latest_len);
	state[latest_len] = '\0';
	*state_len = latest_len;

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

	switch (cmd_id) {
	case TEAR_TA_CMD_PING:
		DMSG("TEAR TA ping");
		return TEE_SUCCESS;

	case TEAR_TA_CMD_ENROLL: {
		char incoming[TEAR_STATE_RECORD_MAX];
		TEE_Result res;

		DMSG("TEAR TA enroll");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (copy_input_state(&params[0],
				     incoming,
				     sizeof(incoming)) < 0)
			return TEE_ERROR_BAD_PARAMETERS;

		res = upsert_trusted_state(incoming);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA enroll - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_VERIFY: {
		char incoming[TEAR_STATE_RECORD_MAX];
		char trusted[TEAR_STATE_RECORD_MAX];
		struct tear_ta_state incoming_state;
		struct tear_ta_state trusted_state;
		TEE_Result res;

		DMSG("TEAR TA verify");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (copy_input_state(&params[0],
				     incoming,
				     sizeof(incoming)) < 0)
			return TEE_ERROR_BAD_PARAMETERS;

		if (parse_state(incoming, &incoming_state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		res = find_trusted_state(incoming_state.artifact_id,
					 trusted,
					 sizeof(trusted));
		if (res != TEE_SUCCESS)
			return res;

		if (parse_state(trusted, &trusted_state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (!same_state(&incoming_state, &trusted_state))
			return TEE_ERROR_SECURITY;

		DMSG("TEAR TA verify - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_UPDATE: {
		char incoming[TEAR_STATE_RECORD_MAX];
		char trusted[TEAR_STATE_RECORD_MAX];
		struct tear_ta_state incoming_state;
		struct tear_ta_state trusted_state;
		TEE_Result res;

		DMSG("TEAR TA update");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		if (copy_input_state(&params[0],
				     incoming,
				     sizeof(incoming)) < 0)
			return TEE_ERROR_BAD_PARAMETERS;

		if (parse_state(incoming, &incoming_state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		res = find_trusted_state(incoming_state.artifact_id,
					 trusted,
					 sizeof(trusted));
		if (res != TEE_SUCCESS)
			return res;

		if (parse_state(trusted, &trusted_state) < 0)
			return TEE_ERROR_BAD_FORMAT;

		if (!update_allowed(&trusted_state, &incoming_state))
			return TEE_ERROR_SECURITY;

		res = upsert_trusted_state(incoming);
		if (res != TEE_SUCCESS)
			return res;

		DMSG("TEAR TA update - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	case TEAR_TA_CMD_REPORT: {
		char trusted_state[TEAR_STATE_RECORD_MAX];
		size_t trusted_state_len = 0;
		TEE_Result res;

		DMSG("TEAR TA report");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		res = latest_trusted_state(trusted_state,
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

	case TEAR_TA_CMD_REPORT_DECISION: {
		char decision[TEAR_DECISION_MAX];
		size_t decision_len = 0;
		TEE_Result res;

		DMSG("TEAR TA report decision");

		if (param_types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE))
			return TEE_ERROR_BAD_PARAMETERS;

		res = read_trusted_decision(decision,
					    sizeof(decision),
					    &decision_len);
		if (res != TEE_SUCCESS)
			return res;

		if (params[0].memref.size < decision_len + 1) {
			params[0].memref.size = decision_len + 1;
			return TEE_ERROR_SHORT_BUFFER;
		}

		TEE_MemMove(params[0].memref.buffer, decision, decision_len);
		((char *)params[0].memref.buffer)[decision_len] = '\0';
		params[0].memref.size = decision_len + 1;

		DMSG("TEAR TA report decision - TEE_SUCCESS");
		return TEE_SUCCESS;
	}

	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
