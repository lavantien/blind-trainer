#include "common.h"
#include "common/alloc.h"
#include "fuzz/expand_body.h"
#include "fuzz/fuzz_common.h"
#include "unit/core/death_test.h"

BT_TEST(fuzz_expand, plainloop_replay_and_generation) {
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_plainloop("expand", LLVMFuzzerTestOneInput, alphabet, sizeof alphabet - 1U,
                   fuzz_seed() + 1);
}

static void expand_death_formatted_init(void) {
    bt_alloc_fail_arm(0);
    const uint8_t input[] = {'R'};
    (void)bt_fuzz_expand_once(input, sizeof input);
}

static void expand_death_cancelled_init(void) {
    bt_alloc_fail_arm(1);
    const uint8_t input[] = {'R'};
    (void)bt_fuzz_expand_once(input, sizeof input);
}

BT_TEST(fuzz_expand, the_body_aborts_when_a_strbuf_init_fails) {
    bt_death_expect_code("fuzz_expand.formatted_init", "strbuf init failed in expand body", 134,
                         expand_death_formatted_init);
    bt_death_expect_code("fuzz_expand.cancelled_init", "strbuf init failed in expand body", 134,
                         expand_death_cancelled_init);
}
