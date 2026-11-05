#include "common.h"
#include "common/alloc.h"
#include "fuzz/fuzz_common.h"
#include "fuzz/notation_body.h"
#include "unit/core/death_test.h"

BT_TEST(fuzz_notation, plainloop_replay_and_generation) {
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_plainloop("notation", LLVMFuzzerTestOneInput, alphabet, sizeof alphabet - 1U,
                   fuzz_seed() + 2);
}

static void notation_death_formatted_init(void) {
    bt_alloc_fail_arm(0);
    const uint8_t input[] = {'R'};
    (void)bt_fuzz_notation_once(input, sizeof input);
}

BT_TEST(fuzz_notation, the_body_aborts_when_a_strbuf_init_fails) {
    bt_death_expect_code("fuzz_notation.formatted_init", "strbuf init failed in notation body", 134,
                         notation_death_formatted_init);
}
