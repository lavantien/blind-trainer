#include "common/paths.h"
#include "fuzz/fuzz_common.h"
#include "fuzz/ga.h"
#include "unit/core/death_test.h"

#include <stdio.h>

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

BT_TEST(fuzz_ga, the_runs_bound_stops_the_body) {
    uint8_t input[] = {1, 2, 3};
    for (size_t i = 0; i < 5002; i++) {
        (void)LLVMFuzzerTestOneInput(input, sizeof input);
    }
    BT_CHECK(!bt_fuzz_within_bounds());
}

static void ga_death_unwritable_input_path(void) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s\\build\\test-tmp\\ga-fuzz-blocker.txt", root);
    FILE *file = fopen(blocker, "wb");
    if (file != NULL) {
        fclose(file);
    }
    char setting[BT_PATH_MAX];
    snprintf(setting, sizeof setting, "BT_ARTIFACTS=%s", blocker);
    _putenv(setting);
    uint8_t input[] = {1, 2, 3};
    bt_fuzz_ga_once(input, sizeof input);
}

BT_TEST(fuzz_ga, the_body_aborts_when_the_input_path_is_unwritable) {
    bt_death_expect_code("fuzz_ga.unwritable_input_path", "fuzz ga: cannot write", 134,
                         ga_death_unwritable_input_path);
}
