#ifndef POLICY_CONSTANTS_H
#define POLICY_CONSTANTS_H

#include <stddef.h>

#include "common/json.h"
#include "policy/lexer.h"

typedef struct {
    size_t line;
    char *core;
} policy_occurrence;

typedef struct {
    char *core;
    size_t count;
    size_t line;
} policy_recurring;

typedef struct {
    unsigned long long literal_recurrence;
    char **small_literal_allow;
    size_t allow_count;
} policy_policy_config;

int policy_constants_from_config(const bt_json_value *config, policy_policy_config *out, char *err,
                                 size_t err_len);
void policy_policy_config_free(policy_policy_config *config);
policy_occurrence *policy_scan_literals(const char *text, size_t len, policy_lang lang,
                                        size_t *count);
void policy_occurrences_free(policy_occurrence *hits, size_t count);
policy_recurring *policy_recurring_literals(const char *text, size_t len, policy_lang lang,
                                            unsigned long long threshold, size_t *count);
void policy_recurring_free(policy_recurring *hits, size_t count);

#endif
