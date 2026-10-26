#include "common.h"
#include "fuzz/fuzz_common.h"
#include "fuzz/notation_body.h"

BT_TEST(fuzz_notation, plainloop_replay_and_generation) {
    fuzz_bounds bounds;
    fuzz_bounds_init(&bounds);
    size_t ran = 0;
    bool replayed = fuzz_replay_dir("notation", bt_fuzz_notation_once, &bounds, &ran);
    BT_CHECK(replayed);
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_generate(bt_fuzz_notation_once, &bounds, alphabet, sizeof alphabet - 1U, 20261012, &ran);
    printf("fuzz notation: %zu inputs\n", ran);
    BT_CHECK(ran > 0);
}
