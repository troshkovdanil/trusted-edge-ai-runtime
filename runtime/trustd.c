// SPDX-License-Identifier: Apache-2.0

#include "model_manifest.h"
#include "observability.h"
#include "trusted_state.h"
#ifdef TEAR_ENABLE_OPTEE
#include "tear_optee_client.h"
#endif
#include "runtime_paths.h"

#include <errno.h>
#include <stdarg.h>
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

static void trustd_event(const char *event)
{
    tear_event(TEAR_COMPONENT, event);
}

static void trustd_manifest_event(const struct tear_model_manifest *manifest,
                                  const char *event)
{
    tear_event_manifest(TEAR_COMPONENT, manifest, event);
}

static void trustd_perror(const char *msg)
{
    tear_log(TEAR_COMPONENT,
             TEAR_LOG_ERROR,
             "%s: %s",
             msg,
             strerror(errno));
}

static void client_reply(int client, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vdprintf(client, fmt, ap);
    va_end(ap);
}

static void client_reply_ok(int client)
{
    client_reply(client, "OK");
}

static void client_reply_err(int client)
{
    client_reply(client, "ERR");
}

static void client_reply_state(int client, const char *state)
{
    client_reply(client, "STATE %s", state);
}

static void client_reply_decision(int client, const char *decision)
{
    client_reply(client, "DECISION %s", decision);
}

static int create_socket(void)
{
    const char *socket_path = tear_trustd_socket_path();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    unlink(socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

static int parse_decision_message(const char *buf,
                                  char *run_id,
                                  char *artifact_id,
                                  char *proposal,
                                  char *decision,
                                  char *reason,
                                  long *value)
{
    return sscanf(buf,
                  "RECORD_DECISION %63s %63s %127s %63s %127s %ld",
                  run_id,
                  artifact_id,
                  proposal,
                  decision,
                  reason,
                  value) == 6 ? 0 : -1;
}

#ifdef TEAR_ENABLE_OPTEE
static void manifest_to_state(const struct tear_model_manifest *m,
                              char *state,
                              size_t state_size)
{
    snprintf(state,
             state_size,
             "%s %d %s %s",
             m->artifact_id,
             m->version,
             m->backend,
             m->model_hash);
}
#endif

static int model_update_allowed(const struct tear_model_manifest *old,
                                const struct tear_model_manifest *new)
{
    if (strcmp(old->artifact_id, new->artifact_id) != 0)
        return 0;

    if (strcmp(old->backend, new->backend) != 0)
        return 0;

    return new->version > old->version;
}

/* File backend. */

static int file_enroll(const struct tear_model_manifest *m)
{
    return tear_trusted_state_store(TEAR_TRUSTED_STATE, m);
}

static int file_update(const struct tear_model_manifest *incoming)
{
    struct tear_model_manifest trusted;

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) < 0)
        return -1;

    if (!model_update_allowed(&trusted, incoming))
        return -2;

    return tear_trusted_state_store(TEAR_TRUSTED_STATE, incoming);
}

static int file_verify(const struct tear_model_manifest *incoming)
{
    struct tear_model_manifest trusted;

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &trusted) < 0)
        return -1;

    return same_manifest(incoming, &trusted) ? 0 : -1;
}

static int file_report(char *state, size_t state_size)
{
    struct tear_model_manifest m;
    int n;

    if (tear_trusted_state_load(TEAR_TRUSTED_STATE, &m) < 0)
        return -1;

    n = snprintf(state,
                 state_size,
                 "%s %d %s %s",
                 m.artifact_id,
                 m.version,
                 m.backend,
                 m.model_hash);

    return n >= 0 && (size_t)n < state_size ? 0 : -1;
}

static int file_record_decision(const char *run_id,
                                const char *artifact_id,
                                const char *proposal,
                                const char *decision,
                                const char *reason,
                                long value)
{
    return tear_trusted_state_append_decision(TEAR_TRUSTED_DECISIONS,
                                              run_id,
                                              artifact_id,
                                              proposal,
                                              decision,
                                              reason,
                                              value);
}

static int file_report_decision(char *decision, size_t decision_size)
{
    return tear_trusted_state_report_decision(TEAR_TRUSTED_DECISIONS,
                                              decision,
                                              decision_size);
}

/* OP-TEE backend. */

static int optee_enroll(const struct tear_model_manifest *m)
{
#ifdef TEAR_ENABLE_OPTEE
    char state[256];

    manifest_to_state(m, state, sizeof(state));
    return tear_optee_enroll(state);
#else
    (void)m;
    return -1;
#endif
}

static int optee_update(const struct tear_model_manifest *incoming)
{
#ifdef TEAR_ENABLE_OPTEE
    char state[256];

    manifest_to_state(incoming, state, sizeof(state));
    return tear_optee_update(state);
#else
    (void)incoming;
    return -1;
#endif
}

static int optee_verify(const struct tear_model_manifest *incoming)
{
#ifdef TEAR_ENABLE_OPTEE
    char state[256];

    manifest_to_state(incoming, state, sizeof(state));
    return tear_optee_verify(state);
#else
    (void)incoming;
    return -1;
#endif
}

static int optee_report(char *state, size_t state_size)
{
#ifdef TEAR_ENABLE_OPTEE
    return tear_optee_report(state, state_size);
#else
    (void)state;
    (void)state_size;
    return -1;
#endif
}

static int optee_record_decision(const char *run_id,
                                 const char *artifact_id,
                                 const char *proposal,
                                 const char *decision,
                                 const char *reason,
                                 long value)
{
#ifdef TEAR_ENABLE_OPTEE
    return tear_optee_record_decision(run_id,
                                      artifact_id,
                                      proposal,
                                      decision,
                                      reason,
                                      value);
#else
    (void)run_id;
    (void)artifact_id;
    (void)proposal;
    (void)decision;
    (void)reason;
    (void)value;
    return -1;
#endif
}

static int optee_report_decision(char *decision, size_t decision_size)
{
#ifdef TEAR_ENABLE_OPTEE
    return tear_optee_report_decision(decision, decision_size);
#else
    (void)decision;
    (void)decision_size;
    return -1;
#endif
}

/* Backend dispatch. */

static int trust_backend_enroll(enum tear_trust_backend backend,
                                const struct tear_model_manifest *m)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_enroll(m);

    return file_enroll(m);
}

static int trust_backend_update(enum tear_trust_backend backend,
                                const struct tear_model_manifest *incoming)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_update(incoming);

    return file_update(incoming);
}

static int trust_backend_verify(enum tear_trust_backend backend,
                                const struct tear_model_manifest *incoming)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_verify(incoming);

    return file_verify(incoming);
}

static int trust_backend_report(enum tear_trust_backend backend,
                                char *state,
                                size_t state_size)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_report(state, state_size);

    return file_report(state, state_size);
}

static int trust_backend_record_decision(enum tear_trust_backend backend,
                                         const char *run_id,
                                         const char *artifact_id,
                                         const char *proposal,
                                         const char *decision,
                                         const char *reason,
                                         long value)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_record_decision(run_id,
                                     artifact_id,
                                     proposal,
                                     decision,
                                     reason,
                                     value);

    return file_record_decision(run_id,
                                artifact_id,
                                proposal,
                                decision,
                                reason,
                                value);
}

static int trust_backend_report_decision(enum tear_trust_backend backend,
                                         char *decision,
                                         size_t decision_size)
{
    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        return optee_report_decision(decision, decision_size);

    return file_report_decision(decision, decision_size);
}

/* Protocol handlers. */

static void handle_enroll(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest m;

    if (parse_manifest_message(buf, "ENROLL", &m) < 0) {
        trustd_event("model_enroll_failed");
        client_reply_err(client);
        return;
    }

    if (trust_backend_enroll(backend, &m) == 0) {
        trustd_manifest_event(&m,
                              backend == TEAR_TRUST_BACKEND_OPTEE ?
                              "optee_model_enroll" :
                              "model_enroll");
        client_reply_ok(client);
        return;
    }

    trustd_manifest_event(&m,
                          backend == TEAR_TRUST_BACKEND_OPTEE ?
                          "optee_model_enroll_failed" :
                          "model_enroll_failed");
    client_reply_err(client);
}

static void handle_update(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest incoming;
    int ret;

    if (parse_manifest_message(buf, "UPDATE", &incoming) < 0) {
        trustd_event("model_update_failed");
        client_reply_err(client);
        return;
    }

    ret = trust_backend_update(backend, &incoming);

    if (ret == 0) {
        trustd_manifest_event(&incoming,
                              backend == TEAR_TRUST_BACKEND_OPTEE ?
                              "optee_model_update_ok" :
                              "model_update_ok");
        client_reply_ok(client);
        return;
    }

    if (ret == -2) {
        trustd_manifest_event(&incoming,
                              backend == TEAR_TRUST_BACKEND_OPTEE ?
                              "optee_model_rollback_rejected" :
                              "model_rollback_rejected");
    } else {
        trustd_manifest_event(&incoming,
                              backend == TEAR_TRUST_BACKEND_OPTEE ?
                              "optee_model_update_failed" :
                              "model_update_failed");
    }

    client_reply_err(client);
}

static void handle_verify(int client,
                          const char *buf,
                          enum tear_trust_backend backend)
{
    struct tear_model_manifest incoming;

    if (parse_manifest_message(buf, "VERIFY", &incoming) < 0) {
        trustd_event("model_verify_failed");
        client_reply_err(client);
        return;
    }

    if (trust_backend_verify(backend, &incoming) == 0) {
        trustd_manifest_event(&incoming,
                              backend == TEAR_TRUST_BACKEND_OPTEE ?
                              "optee_model_verify_ok" :
                              "model_verify_ok");
        client_reply_ok(client);
        return;
    }

    trustd_manifest_event(&incoming,
                          backend == TEAR_TRUST_BACKEND_OPTEE ?
                          "optee_model_verify_failed" :
                          "model_verify_failed");
    client_reply_err(client);
}

static void handle_report(int client, enum tear_trust_backend backend)
{
    char state[256];

    if (trust_backend_report(backend, state, sizeof(state)) == 0)
        client_reply_state(client, state);
    else
        client_reply_err(client);
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

    if (parse_decision_message(buf,
                               run_id,
                               artifact_id,
                               proposal,
                               decision,
                               reason,
                               &value) < 0) {
        trustd_event("optimization_decision_record_failed");
        client_reply_err(client);
        return;
    }

    if (trust_backend_record_decision(backend,
                                      run_id,
                                      artifact_id,
                                      proposal,
                                      decision,
                                      reason,
                                      value) < 0) {
        trustd_event("optimization_decision_record_failed");
        client_reply_err(client);
        return;
    }

    if (backend == TEAR_TRUST_BACKEND_OPTEE)
        trustd_event("optee_record_decision_ok");

    trustd_event("optimization_decision_recorded");
    client_reply_ok(client);
}

static void handle_report_decision(int client, enum tear_trust_backend backend)
{
    char decision[512];

    if (trust_backend_report_decision(backend,
                                      decision,
                                      sizeof(decision)) == 0)
        client_reply_decision(client, decision);
    else
        client_reply_err(client);
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
                tear_log(TEAR_COMPONENT,
                         TEAR_LOG_ERROR,
                         "OP-TEE backend not built");
                return -1;
#endif
            } else {
                tear_log(TEAR_COMPONENT,
                         TEAR_LOG_ERROR,
                         "unknown backend: %s",
                         argv[i]);
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
            tear_log(TEAR_COMPONENT,
                     TEAR_LOG_ERROR,
                     "usage: tear-trustd [--backend file|optee] "
                     "[--event-log <path>] "
                     "[--self-test] "
                     "[--self-test-enroll] "
                     "[--self-test-verify]");
            return -1;
        }
    }

    if (!*event_log || (*event_log)[0] == '\0') {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "missing --event-log <path>");
        return -1;
    }

    return 0;
}

static int run_self_test(enum tear_trust_backend backend)
{
    if (backend == TEAR_TRUST_BACKEND_FILE) {
        trustd_event("trustd_file_backend_self_test_ok");
        return 0;
    }

#ifdef TEAR_ENABLE_OPTEE
    if (backend == TEAR_TRUST_BACKEND_OPTEE) {
        if (tear_optee_ping() == 0) {
            trustd_event("trustd_optee_backend_ping_ok");
            return 0;
        }

        trustd_event("trustd_optee_backend_ping_failed");
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
        trustd_event("trustd_optee_backend_enroll_ok");
        return 0;
    }

    trustd_event("trustd_optee_backend_enroll_failed");
#endif

    return 1;
}

static int run_verify_self_test(enum tear_trust_backend backend)
{
    if (backend != TEAR_TRUST_BACKEND_OPTEE)
        return 1;

#ifdef TEAR_ENABLE_OPTEE
    if (tear_optee_enroll("demo-model 1 mock sha256-demo-model-v1")) {
        trustd_event("trustd_optee_backend_verify_failed");
        return 1;
    }

    trustd_event("trustd_optee_backend_enroll_before_verify_ok");

    if (tear_optee_verify("demo-model 1 mock sha256-demo-model-v1")) {
        trustd_event("trustd_optee_backend_verify_failed");
        return 1;
    }

    trustd_event("trustd_optee_backend_verify_ok");
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
    int server;

    if (parse_backend(argc,
                      argv,
                      &backend,
                      &event_log,
                      &self_test,
                      &self_test_enroll,
                      &self_test_verify) < 0)
        return 1;

    if (tear_event_init(event_log) < 0) {
        tear_log(TEAR_COMPONENT,
                 TEAR_LOG_ERROR,
                 "failed to initialize events");
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

    server = create_socket();

    if (server < 0) {
        trustd_perror("trustd socket");
        tear_event_shutdown();
        return 1;
    }

    trustd_event("trustd_start");

    while (1) {
        int client = accept(server, NULL, NULL);
        char buf[512];
        ssize_t n;

        if (client < 0)
            continue;

        n = read(client, buf, sizeof(buf) - 1);

        if (n <= 0) {
            close(client);
            continue;
        }

        buf[n] = '\0';

        if (strncmp(buf, "ENROLL", 6) == 0) {
            handle_enroll(client, buf, backend);
        } else if (strncmp(buf, "VERIFY", 6) == 0) {
            handle_verify(client, buf, backend);
        } else if (strncmp(buf, "REPORT_DECISION", 15) == 0) {
            handle_report_decision(client, backend);
        } else if (strncmp(buf, "REPORT", 6) == 0) {
            handle_report(client, backend);
        } else if (strncmp(buf, "UPDATE", 6) == 0) {
            handle_update(client, buf, backend);
        } else if (strncmp(buf, "RECORD_DECISION", 15) == 0) {
            handle_record_decision(client, buf, backend);
        } else {
            client_reply_err(client);
        }

        close(client);
    }

    return 0;
}
