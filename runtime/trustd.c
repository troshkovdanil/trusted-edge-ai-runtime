// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "telemetry.h"
#include "trusted_state.h"
#ifdef TEAR_ENABLE_OPTEE
#include "tear_optee_client.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TEAR_TRUSTD_SOCKET "/tmp/tear-trustd.sock"
#define TEAR_TRUSTED_STATE "/tmp/tear-trusted-state"
#define TEAR_TRUSTED_DECISIONS "/tmp/tear-trusted-decisions"

static int create_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path,
            TEAR_TRUSTD_SOCKET,
            sizeof(addr.sun_path) - 1);

    unlink(TEAR_TRUSTD_SOCKET);

    if (bind(fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int same_manifest(
    const struct tear_model_manifest *a,
    const struct tear_model_manifest *b)
{
    return strcmp(a->model_id, b->model_id) == 0 &&
           a->version == b->version &&
           strcmp(a->backend, b->backend) == 0 &&
           strcmp(a->model_hash, b->model_hash) == 0;
}

static int parse_manifest_message(
    const char *buf,
    const char *command,
    struct tear_model_manifest *m)
{
    char fmt[64];

    snprintf(fmt, sizeof(fmt),
             "%s %%63s %%d %%31s %%127s",
             command);

    memset(m, 0, sizeof(*m));

    return sscanf(buf,
                  fmt,
                  m->model_id,
                  &m->version,
                  m->backend,
                  m->model_hash) == 4 ? 0 : -1;
}

static void handle_enroll(int client, const char *buf)
{
    struct tear_model_manifest m;

    if (parse_manifest_message(buf, "ENROLL", &m) < 0) {
        tear_event("model_enroll_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (tear_trusted_state_store(TEAR_TRUSTED_STATE, &m) == 0) {
        tear_event("model_enroll");
        dprintf(client, "OK\n");
    } else {
        tear_event("model_enroll_failed");
        dprintf(client, "ERR\n");
    }
}

static int model_update_allowed(const struct tear_model_manifest *old,
                                const struct tear_model_manifest *new)
{
        if (strcmp(old->model_id, new->model_id) != 0)
                return 0;

        if (strcmp(old->backend, new->backend) != 0)
                return 0;

        return new->version > old->version;
}

static void handle_update(int client, const char *buf)
{
        struct tear_model_manifest incoming;
        struct tear_model_manifest trusted;

        if (parse_manifest_message(buf, "UPDATE", &incoming) < 0) {
                tear_event("model_update_failed");
                dprintf(client, "ERR\n");
                return;
        }

        if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) < 0) {
                tear_event("model_update_no_trusted_state");
                dprintf(client, "ERR\n");
                return;
        }

        if (!model_update_allowed(&trusted, &incoming)) {
                tear_event("model_rollback_rejected");
                dprintf(client, "ERR\n");
                return;
        }

        if (tear_trusted_state_store(TEAR_TRUSTED_STATE, &incoming) < 0) {
                tear_event("model_update_failed");
                dprintf(client, "ERR\n");
                return;
        }

        tear_event("model_update_ok");
        dprintf(client, "OK\n");
}

static void handle_verify(int client, const char *buf)
{
    struct tear_model_manifest incoming;
    struct tear_model_manifest trusted;

    if (parse_manifest_message(buf, "VERIFY", &incoming) < 0) {
        tear_event("model_verify_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) == 0 &&
        same_manifest(&incoming, &trusted)) {
        tear_event("model_verify_ok");
        dprintf(client, "OK\n");
    } else {
        tear_event("model_verify_failed");
        dprintf(client, "ERR\n");
    }
}

static void handle_report(int client)
{
    struct tear_model_manifest m;

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &m) == 0) {
        dprintf(client,
                "STATE %s %d %s %s\n",
                m.model_id,
                m.version,
                m.backend,
                m.model_hash);
    } else {
        dprintf(client, "ERR\n");
    }
}

static void handle_record_decision(int client, const char *buf)
{
    char model_id[64];
    char proposal[128];
    char decision[64];
    char reason[128];
    long value;

    if (sscanf(buf,
               "RECORD_DECISION %63s %127s %63s %127s %ld",
               model_id,
               proposal,
               decision,
               reason,
               &value) != 5) {
        tear_event("optimization_decision_record_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (tear_trusted_state_append_decision(TEAR_TRUSTED_DECISIONS,
                                           model_id,
                                           proposal,
                                           decision,
                                           reason,
                                           value) < 0) {
        tear_event("optimization_decision_record_failed");
        dprintf(client, "ERR\n");
        return;
    }

    tear_event("optimization_decision_recorded");
    dprintf(client, "OK\n");
}

enum tear_trust_backend {
    TEAR_TRUST_BACKEND_FILE,
    TEAR_TRUST_BACKEND_OPTEE,
};

static int parse_backend(int argc, char **argv,
                         enum tear_trust_backend *backend,
                         int *self_test,
                         int *self_test_enroll)
{
    *backend = TEAR_TRUST_BACKEND_FILE;
    *self_test = 0;
    *self_test_enroll = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            i++;

            if (strcmp(argv[i], "file") == 0) {
                *backend = TEAR_TRUST_BACKEND_FILE;
            } else if (strcmp(argv[i], "optee") == 0) {
#ifdef TEAR_ENABLE_OPTEE
                *backend = TEAR_TRUST_BACKEND_OPTEE;
#else
                fprintf(stderr, "TEAR trustd: OP-TEE backend not built\n");
                return -1;
#endif
            } else {
                fprintf(stderr, "TEAR trustd: unknown backend: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "--self-test") == 0) {
            *self_test = 1;
        } else if (strcmp(argv[i], "--self-test-enroll") == 0) {
            *self_test_enroll = 1;
        } else {
            fprintf(stderr, "usage: tear-trustd [--backend file|optee] [--self-test] [--self-test-enroll]\n");
            return -1;
        }
    }

    return 0;
}

static int run_self_test(enum tear_trust_backend backend)
{
    if (backend == TEAR_TRUST_BACKEND_FILE) {
        tear_event("trustd_file_backend_self_test_ok");
        return 0;
    }

#ifdef TEAR_ENABLE_OPTEE
    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
        if (tear_optee_ping() == 0) {
            tear_event("trustd_optee_backend_ping_ok");
            return 0;
        }

        tear_event("trustd_optee_backend_ping_failed");
        return 1;
    }
#endif

    return 1;
}

static int run_enroll_self_test(enum tear_trust_backend backend)
{
	if (backend != TEAR_TRUST_BACKEND_OPTEE)
		return 1;

#ifdef TEAR_ENABLE_OPTEE
	if (tear_optee_enroll("demo-model 1 mock sha256-demo-model-v1") == 0) {
		tear_event("trustd_optee_backend_enroll_ok");
		return 0;
	}

	tear_event("trustd_optee_backend_enroll_failed");
#endif
	return 1;
}

int main(int argc, char **argv)
{
    enum tear_trust_backend backend;
    int self_test;
    int self_test_enroll;

    if (parse_backend(argc, argv, &backend, &self_test, &self_test_enroll) < 0)
        return 1;

    if (self_test)
        return run_self_test(backend);

    if (self_test_enroll)
        return run_enroll_self_test(backend);

    int server = create_socket();

    if (server < 0) {
        perror("trustd socket");
        return 1;
    }

    tear_event("trustd_start");

    while (1) {
        int client = accept(server, NULL, NULL);

        if (client < 0)
            continue;

        char buf[512];

        ssize_t n = read(client, buf, sizeof(buf) - 1);

        if (n <= 0) {
            close(client);
            continue;
        }

        buf[n] = '\0';

        if (strncmp(buf, "ENROLL", 6) == 0) {
            handle_enroll(client, buf);
        } else if (strncmp(buf, "VERIFY", 6) == 0) {
            handle_verify(client, buf);
        } else if (strncmp(buf, "REPORT", 6) == 0) {
            handle_report(client);
        } else if (strncmp(buf, "UPDATE", 6) == 0) {
            handle_update(client, buf);
        } else if (strncmp(buf, "RECORD_DECISION", 15) == 0) {
            handle_record_decision(client, buf);
        } else {
            dprintf(client, "ERR\n");
        }

        close(client);
    }

    return 0;
}
