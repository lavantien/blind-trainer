#include "policy/constants.h"

#include <stdlib.h>

#include "policy/lib.h"

int policy_constants_from_config(const bt_json_value *config, policy_policy_config *out, char *err,
                                 size_t err_len) {
    (void)config;
    (void)out;
    snprintf(err, err_len, "stub");
    return -1;
}

void policy_policy_config_free(policy_policy_config *config) {
    for (size_t at = 0; at < config->allow_count; at++) {
        free(config->small_literal_allow[at]);
    }
    free(config->small_literal_allow);
    config->small_literal_allow = NULL;
    config->allow_count = 0;
}

policy_occurrence *policy_scan_literals(const char *masked_text, policy_lang lang, size_t *count) {
    (void)masked_text;
    (void)lang;
    *count = 0;
    return NULL;
}

void policy_occurrences_free(policy_occurrence *hits, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(hits[at].core);
    }
    free(hits);
}

policy_recurring *policy_recurring_literals(const char *masked_text, policy_lang lang,
                                            unsigned long long threshold, size_t *count) {
    (void)masked_text;
    (void)lang;
    (void)threshold;
    *count = 0;
    return NULL;
}

void policy_recurring_free(policy_recurring *hits, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(hits[at].core);
    }
    free(hits);
}
