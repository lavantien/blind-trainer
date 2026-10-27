#include "common.h"
#include "fuzz/fuzz_common.h"
#include "fuzz/notation_body.h"

BT_TEST(fuzz_notation, plainloop_replay_and_generation) {
    static const char alphabet[] =
        "UDLRFBudlrfb2MESxyz' ()[]:,+-0123456789abcdefghijklmnopqrstuvwxyz \t";
    fuzz_plainloop("notation", LLVMFuzzerTestOneInput, alphabet, sizeof alphabet - 1U, fuzz_seed());
}
