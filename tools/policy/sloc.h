#ifndef POLICY_SLOC_H
#define POLICY_SLOC_H

#include <stddef.h>

#include "policy/lib.h"

typedef struct {
    unsigned long long max_file_lines;
} policy_sloc_config;

int policy_sloc_from_config(const bt_json_value *config, policy_sloc_config *out, char *err,
                            size_t err_len);
int policy_cap_finding(const char *text, size_t len, const char *rel, unsigned long long cap,
                       policy_finding *out);

#endif
