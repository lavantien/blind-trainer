#include <stddef.h>

#include "common/bt_harness.h"

extern void bt_test_alg_parses_every_token_kind(void);
extern void bt_test_alg_parses_and_canonicalizes_suffixes(void);
extern void bt_test_alg_empty_and_whitespace_parse_to_empty(void);
extern void bt_test_alg_metrics(void);
extern void bt_test_alg_inverse_format(void);
extern void bt_test_alg_rejects_garbage_tokens(void);
extern void bt_test_alg_parse_error_carries_the_token(void);
extern void bt_test_alg_splits_unicode_whitespace(void);

int main(void) {
    bt_test_entry tests[] = {
        {"alg.parses_every_token_kind", bt_test_alg_parses_every_token_kind},
        {"alg.parses_and_canonicalizes_suffixes", bt_test_alg_parses_and_canonicalizes_suffixes},
        {"alg.empty_and_whitespace_parse_to_empty", bt_test_alg_empty_and_whitespace_parse_to_empty},
        {"alg.metrics", bt_test_alg_metrics},
        {"alg.inverse_format", bt_test_alg_inverse_format},
        {"alg.rejects_garbage_tokens", bt_test_alg_rejects_garbage_tokens},
        {"alg.parse_error_carries_the_token", bt_test_alg_parse_error_carries_the_token},
        {"alg.splits_unicode_whitespace", bt_test_alg_splits_unicode_whitespace},
    };
    return bt_harness_run("core_alg", tests, sizeof tests / sizeof tests[0]);
}
