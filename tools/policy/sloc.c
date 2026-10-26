#include "policy/sloc.h"

int policy_sloc_from_config(const bt_json_value *config, policy_sloc_config *out, char *err,
                            size_t err_len) {
    (void)config;
    (void)out;
    snprintf(err, err_len, "stub");
    return -1;
}

int policy_cap_finding(const char *text, size_t len, const char *rel, unsigned long long cap,
                       policy_finding *out) {
    (void)text;
    (void)len;
    (void)rel;
    (void)cap;
    (void)out;
    return 0;
}
