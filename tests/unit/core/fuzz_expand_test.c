#include "common.h"
#include "fuzz/expand_body.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_expand, plainloop_replay_and_generation) {
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_plainloop("expand", LLVMFuzzerTestOneInput, alphabet, sizeof alphabet - 1U,
                   fuzz_seed() + 1);
}
