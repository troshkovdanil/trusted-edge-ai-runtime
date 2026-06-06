/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_OPTEE_CLIENT_H
#define TEAR_OPTEE_CLIENT_H

#include <stddef.h>

int tear_optee_ping(void);
int tear_optee_enroll(const char *state);
int tear_optee_verify(const char *state);
int tear_optee_update(const char *state);
int tear_optee_report(char *state, size_t state_size);
int tear_optee_record_decision(const char *model_id,
                               const char *proposal,
                               const char *decision,
                               const char *reason,
                               long value);

#endif
