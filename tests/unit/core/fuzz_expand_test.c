#include "common.h"
#include "fuzz/expand_body.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_expand, plainloop_replay_and_generation) {
    fuzz_bounds bounds;
    fuzz_bounds_init(&bounds);
    size_t ran = 0;
    bool replayed = fuzz_replay_dir("expand", bt_fuzz_expand_once, &bounds, &ran);
    BT_CHECK(replayed);
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_generate(bt_fuzz_expand_once, &bounds, alphabet, sizeof alphabet - 1U, 20261013, &ran);
    printf("fuzz expand: %zu inputs\n", ran);
    BT_CHECK(ran > 0);
}
