// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "observability.h"
#include "trusted_state.h"
#ifdef TEAR_ENABLE_OPTEE
#include "tear_optee_client.h"
#endif
#include "runtime_paths.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TEAR_COMPONENT "trustd"
#define TEAR_TRUSTED_STATE "/tmp/tear-trusted-state"
#define TEAR_TRUSTED_DECISIONS "/tmp/tear-trusted-decisions"

#ifdef TEAR_HOST_BUILD
#define DEFAULT_EVENT_PATH "build/host/tear-trustd-events.log"
#else
#define DEFAULT_EVENT_PATH "/tmp/tear-trustd-events.log"
#endif

enum tear_trust_backend {
    TEAR_TRUST_BACKEND_FILE,
    TEAR_TRUST_BACKEND_OPTEE,
};

static void trustd_event(const char *artifact_id, const char *event)
{
    tear_event_ex(TEAR_COMPONENT, NULL, artifact_id, event);
}

static int create_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    const char *socket_path = tear_trustd_socket_path();

    strncpy(addr.sun_path,
            socket_path,
            sizeof(addr.sun_path) - 1);

    unlink(socket_path);

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

static int same_manifest(const struct tear_model_manifest *a,
                         const struct tear_model_manifest *b)
{
    return strcmp(a->artifact_id, b->artifact_id) == 0 &&
           a->version == b->version &&
           strcmp(a->backend, b->backend) == 0 &&
           strcmp(a->model_hash, b->model_hash) == 0;
}

static int parse_manifest_message(const char *buf,
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
                  m->artifact_id,
                  &m->version,
                  m->backend,
                  m->model_hash) == 4 ? 0 : -1;
}

static void handle_enroll(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest m;

    if (parse_manifest_message(buf, "ENROLL", &m) < 0) {
        trustd_event(NULL, "model_enroll_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        char state[256];

        snprintf(state, sizeof(state), "%s %d %s %s",
                 m.artifact_id, m.version, m.backend, m.model_hash);

        if (tear_optee_enroll(state) == 0) {
            trustd_event(m.artifact_id, "optee_model_enroll");
            dprintf(client, "OK\n");
        } else {
            trustd_event(m.artifact_id, "optee_model_enroll_failed");
            dprintf(client, "ERR\n");
        }
#else
        trustd_event(m.artifact_id, "optee_model_enroll_failed");
        dprintf(client, "ERR\n");
#endif
        return;
    }

    if (tear_trusted_state_store(TEAR_TRUSTED_STATE, &m) == 0) {
        trustd_event(m.artifact_id, "model_enroll");
        dprintf(client, "OK\n");
    } else {
        trustd_event(m.artifact_id, "model_enroll_failed");
        dprintf(client, "ERR\n");
    }
}

static int model_update_allowed(const struct tear_model_manifest *old,
                                const struct tear_model_manifest *new)
{
    if (strcmp(old->artifact_id, new->artifact_id) != 0)
        return 0;

    if (strcmp(old->backend, new->backend) != 0)
        return 0;

    return new->version > old->version;
}

static void handle_update(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest incoming;
    struct tear_model_manifest trusted;

    if (parse_manifest_message(buf, "UPDATE", &incoming) < 0) {
        trustd_event(NULL, "model_update_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        char state[256];

        snprintf(state, sizeof(state), "%s %d %s %s",
                 incoming.artifact_id,
                 incoming.version,
                 incoming.backend,
                 incoming.model_hash);

        if (tear_optee_update(state) == 0) {
            trustd_event(incoming.artifact_id, "optee_model_update_ok");
            dprintf(client, "OK\n");
        } else {
            trustd_event(incoming.artifact_id, "optee_model_update_rejected");
            dprintf(client, "ERR\n");
        }
#else
        trustd_event(incoming.artifact_id, "optee_model_update_rejected");
        dprintf(client, "ERR\n");
#endif
        return;
    }

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) < 0) {
        trustd_event(incoming.artifact_id, "model_update_no_trusted_state");
        dprintf(client, "ERR\n");
        return;
    }

    if (!model_update_allowed(&trusted, &incoming)) {
        trustd_event(incoming.artifact_id, "model_rollback_rejected");
        dprintf(client, "ERR\n");
        return;
    }

    if (tear_trusted_state_store(TEAR_TRUSTED_STATE, &incoming) < 0) {
        trustd_event(incoming.artifact_id, "model_update_failed");
        dprintf(client, "ERR\n");
        return;
    }

    trustd_event(incoming.artifact_id, "model_update_ok");
    dprintf(client, "OK\n");
}

static void handle_verify(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest incoming;
    struct tear_model_manifest trusted;

    if (parse_manifest_message(buf, "VERIFY", &incoming) < 0) {
        trustd_event(NULL, "model_verify_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        char state[256];

        snprintf(state, sizeof(state), "%s %d %s %s",
                 incoming.artifact_id,
                 incoming.version,
                 incoming.backend,
                 incoming.model_hash);

        if (tear_optee_verify(state) == 0) {
            trustd_event(incoming.artifact_id, "optee_model_verify_ok");
            dprintf(client, "OK\n");
        } else {
            trustd_event(incoming.artifact_id, "optee_model_verify_failed");
            dprintf(client, "ERR\n");
        }
#else
        trustd_event(incoming.artifact_id, "optee_model_verify_failed");
        dprintf(client, "ERR\n");
#endif
        return;
    }

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) == 0 &&
        same_manifest(&incoming, &trusted)) {
        trustd_event(incoming.artifact_id, "model_verify_ok");
        dprintf(client, "OK\n");
    } else {
        trustd_event(incoming.artifact_id, "model_verify_failed");
        dprintf(client, "ERR\n");
    }
}

static void handle_report(int client, enum tear_trust_backend backend)
{
    struct tear_model_manifest m;

    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        char state[256];

        if (tear_optee_report(state, sizeof(state)) == 0)
            dprintf(client, "STATE %s\n", state);
        else
            dprintf(client, "ERR\n");
#else
        dprintf(client, "ERR\n");
#endif
        return;
    }

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &m) == 0) {
        dprintf(client,
                "STATE %s %d %s %s\n",
                m.artifact_id,
                m.version,
                m.backend,
                m.model_hash);
    } else {
        dprintf(client, "ERR\n");
    }
}

static void handle_record_decision(int client,
                                   const char *buf,
                                   enum tear_trust_backend backend)
{
    char run_id[64];
    char artifact_id[64];
    char proposal[128];
    char decision[64];
    char reason[128];
    long value;

    if (sscanf(buf,
               "RECORD_DECISION %63s %63s %127s %63s %127s %ld",
               run_id,
               artifact_id,
               proposal,
               decision,
               reason,
               &value) != 6) {
        trustd_event(NULL, "optimization_decision_record_failed");
        dprintf(client, "ERR\n");
        return;
    }

    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        if (tear_optee_record_decision(run_id,
                                       artifact_id,
                                       proposal,
                                       decision,
                                       reason,
                                       value) < 0) {
            trustd_event(artifact_id, "optimization_decision_record_failed");
            dprintf(client, "ERR\n");
            return;
        }

        trustd_event(artifact_id, "optee_record_decision_ok");
        trustd_event(artifact_id, "optimization_decision_recorded");
        dprintf(client, "OK\n");
#else
        trustd_event(artifact_id, "optimization_decision_record_failed");
        dprintf(client, "ERR\n");
#endif
        return;
    }

    if (tear_trusted_state_append_decision(TEAR_TRUSTED_DECISIONS,
                                           run_id,
                                           artifact_id,
                                           proposal,
                                           decision,
                                           reason,
                                           value) < 0) {
        trustd_event(artifact_id, "optimization_decision_record_failed");
        dprintf(client, "ERR\n");
        return;
    }

    trustd_event(artifact_id, "optimization_decision_recorded");
    dprintf(client, "OK\n");
}

static void handle_report_decision(int client, enum tear_trust_backend backend)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
#ifdef TEAR_ENABLE_OPTEE
        char decision[512];

        if (tear_optee_report_decision(decision, sizeof(decision)) == 0)
            dprintf(client, "DECISION %s\n", decision);
        else
            dprintf(client, "ERR\n");
#else
        dprintf(client, "ERR\n");
#endif
        return;
    }

    dprintf(client, "ERR\n");
}

static int parse_backend(int argc, char **argv,
                         enum tear_trust_backend *backend,
                         const char **event_log,
                         int *self_test,
                         int *self_test_enroll,
                         int *self_test_verify)
{
    *backend = TEAR_TRUST_BACKEND_FILE;
    *event_log = DEFAULT_EVENT_PATH;
    *self_test = 0;
    *self_test_enroll = 0;
    *self_test_verify = 0;

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
        } else if (strcmp(argv[i], "--event-log") == 0 && i + 1 < argc) {
            *event_log = argv[++i];
        } else if (strcmp(argv[i], "--self-test") == 0) {
            *self_test = 1;
        } else if (strcmp(argv[i], "--self-test-enroll") == 0) {
            *self_test_enroll = 1;
        } else if (strcmp(argv[i], "--self-test-verify") == 0) {
            *self_test_verify = 1;
        } else {
            fprintf(stderr,
                    "usage: tear-trustd [--backend file|optee] "
                    "[--event-log <path>] "
                    "[--self-test] "
                    "[--self-test-enroll] "
                    "[--self-test-verify]\n");
            return -1;
        }
    }

    if (!*event_log || (*event_log)[0] == '\0') {
        fprintf(stderr, "TEAR trustd: missing --event-log <path>\n");
        return -1;
    }

    return 0;
}

static int run_self_test(enum tear_trust_backend backend)
{
    if (backend == TEAR_TRUST_BACKEND_FILE) {
        trustd_event(NULL, "trustd_file_backend_self_test_ok");
        return 0;
    }

#ifdef TEAR_ENABLE_OPTEE
    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
        if (tear_optee_ping() == 0) {
            trustd_event(NULL, "trustd_optee_backend_ping_ok");
            return 0;
        }

        trustd_event(NULL, "trustd_optee_backend_ping_failed");
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
        trustd_event("demo-model", "trustd_optee_backend_enroll_ok");
        return 0;
    }

    trustd_event("demo-model", "trustd_optee_backend_enroll_failed");
#endif

    return 1;
}

static int run_verify_self_test(enum tear_trust_backend backend)
{
    if (backend != TEAR_TRUST_BACKEND_OPTEE)
        return 1;

#ifdef TEAR_ENABLE_OPTEE
    if (tear_optee_enroll("demo-model 1 mock sha256-demo-model-v1")) {
        trustd_event("demo-model", "trustd_optee_backend_verify_failed");
        return 1;
    }

    trustd_event("demo-model",
                 "trustd_optee_backend_enroll_before_verify_ok");

    if (tear_optee_verify("demo-model 1 mock sha256-demo-model-v1")) {
        trustd_event("demo-model", "trustd_optee_backend_verify_failed");
        return 1;
    }

    trustd_event("demo-model", "trustd_optee_backend_verify_ok");
#endif

    return 0;
}

int main(int argc, char **argv)
{
    enum tear_trust_backend backend;
    const char *event_log;
    int self_test;
    int self_test_enroll;
    int self_test_verify;

    if (parse_backend(argc, argv,
                      &backend,
                      &event_log,
                      &self_test,
                      &self_test_enroll,
                      &self_test_verify) < 0)
        return 1;

    if (tear_event_init(event_log) < 0) {
        fprintf(stderr, "TEAR trustd: failed to initialize events\n");
        return 1;
    }

    if (self_test) {
        int ret = run_self_test(backend);
        tear_event_shutdown();
        return ret;
    }

    if (self_test_enroll) {
        int ret = run_enroll_self_test(backend);
        tear_event_shutdown();
        return ret;
    }

    if (self_test_verify) {
        int ret = run_verify_self_test(backend);
        tear_event_shutdown();
        return ret;
    }

    int server = create_socket();

    if (server < 0) {
        perror("trustd socket");
        tear_event_shutdown();
        return 1;
    }

    trustd_event(NULL, "trustd_start");

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
            handle_enroll(client, buf, backend);

        } else if (strncmp(buf, "VERIFY", 6) == 0) {
            handle_verify(client, buf, backend);

        /*
         * Keep longer protocol commands before shorter prefixes.
         */
        } else if (strncmp(buf, "REPORT_DECISION", 15) == 0) {
            handle_report_decision(client, backend);
        } else if (strncmp(buf, "REPORT", 6) == 0) {
            handle_report(client, backend);

        } else if (strncmp(buf, "UPDATE", 6) == 0) {
            handle_update(client, buf, backend);

        } else if (strncmp(buf, "RECORD_DECISION", 15) == 0) {
            handle_record_decision(client, buf, backend);

        } else {
            dprintf(client, "ERR\n");
        }

        close(client);
    }

    return 0;
}
