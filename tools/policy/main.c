#include "policy/lib.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"

static void trace_from_config(const bt_json_value *config) {
    long long ring_bytes = 0;
    long long max_threads = 0;
    long long line_max = 0;
    if (bt_config_int(config, "trace.ring_bytes", &ring_bytes) != 0 ||
        bt_config_int(config, "trace.max_threads", &max_threads) != 0 ||
        bt_config_int(config, "trace.line_max", &line_max) != 0) {
        return;
    }
    if (bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max) != 0) {
        return;
    }
    bt_trace_install_crash_handler();
}

static int real(policy_check_mode mode) {
    char err[POLICY_ERR_MAX];
    char root[BT_PATH_MAX];
    policy_workspace_root(root);
    bt_json_value *config = policy_load_config(err, sizeof err);
    if (config == NULL) {
        fprintf(stderr, "bt-policy: %s\n", err);
        return 2;
    }
    trace_from_config(config);
    char allowlist[BT_PATH_MAX];
    snprintf(allowlist, sizeof allowlist, "%s/config/constants_allow.txt", root);
    policy_outcome outcome;
    int rc = policy_run_checks(root, mode, config, allowlist, &outcome, err, sizeof err);
    bt_json_free(config);
    if (rc != 0) {
        fprintf(stderr, "bt-policy: %s\n", err);
        bt_trace_shutdown();
        return 2;
    }
    for (size_t at = 0; at < outcome.finding_count; at++) {
        printf("bt-policy: %s:%llu: %s\n", outcome.findings[at].path,
               (unsigned long long)outcome.findings[at].line, outcome.findings[at].detail);
    }
    for (size_t at = 0; at < outcome.stale_count; at++) {
        fprintf(stderr, "bt-policy: stale allowlist entry: %s\n", outcome.stale_allowlist[at]);
    }
    int failed = outcome.finding_count > 0;
    policy_outcome_free(&outcome);
    bt_trace_shutdown();
    return failed ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return real(POLICY_MODE_BOTH);
    }
    if (argc == 2 && strcmp(argv[1], "comments") == 0) {
        return real(POLICY_MODE_COMMENTS);
    }
    if (argc == 2 && strcmp(argv[1], "constants") == 0) {
        return real(POLICY_MODE_CONSTANTS);
    }
    if (argc == 2 && strcmp(argv[1], "sloc") == 0) {
        return real(POLICY_MODE_SLOC);
    }
    if (argc == 3 && strcmp(argv[1], "selftest") == 0) {
        char err[POLICY_ERR_MAX];
        int rc = policy_selftest(argv[2], err, sizeof err);
        if (rc < 0) {
            fprintf(stderr, "bt-policy: %s\n", err);
            return 2;
        }
        return rc == 1 ? 0 : 1;
    }
    fprintf(stderr, "usage: bt-policy [comments|constants|sloc|selftest <workdir>]\n");
    return 2;
}
