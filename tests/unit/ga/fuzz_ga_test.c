#include "fuzz/fuzz_common.h"
#include "fuzz/ga.h"

static const char BT_FUZZ_GA_ALPHABET[] = {
    'a',    'b',    'c',    'd',    'e',    'g',    'h',    'i',    'k',    'l',    'm',
    'n',    'o',    'p',    'r',    's',    't',    'u',    'v',    'x',    'y',    '\n',
    '\t',   ' ',    '#',    '"',    '{',    '}',    '[',    ']',    ':',    ',',    '0',
    '1',    '2',    '\xC2', '\x85', '\xA0', '\xC3', '\x84', '\x91', '\xA1', '\xA3', '\xAA',
    '\xB4', '\xC6', '\xB0', '\xA1', '\xE1', '\xBA', '\xBB', '\xAB', '\xAF',
};

BT_TEST(fuzz_ga, plainloop) {
    fuzz_plainloop("ga", LLVMFuzzerTestOneInput, BT_FUZZ_GA_ALPHABET, sizeof BT_FUZZ_GA_ALPHABET,
                   fuzz_seed());
    printf("fuzz ga truncated tail inputs: %zu\n", bt_fuzz_ga_truncated_tail_inputs);
    BT_CHECK(bt_fuzz_ga_truncated_tail_inputs > 0);
}
