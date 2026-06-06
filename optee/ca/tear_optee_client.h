/* SPDX-License-Identifier: Apache-2.0 */

#ifndef TEAR_OPTEE_CLIENT_H
#define TEAR_OPTEE_CLIENT_H

int tear_optee_ping(void);
int tear_optee_enroll(const char *state);
int tear_optee_verify(const char *state);
int tear_optee_update(const char *state);

#endif
