#include "fuzz/classify.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_classify, plainloop) {
    fuzz_plainloop("classify", LLVMFuzzerTestOneInput, NULL, 0, fuzz_seed());
}

BT_TEST(fuzz_classify, the_runs_bound_stops_the_body) {
    uint8_t input[] = {1, 2, 3};
    for (size_t i = 0; i < 5002; i++) {
        (void)LLVMFuzzerTestOneInput(input, sizeof input);
    }
    BT_CHECK(!bt_fuzz_within_bounds());
}
