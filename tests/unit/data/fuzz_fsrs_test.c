#include "fuzz/fsrs.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_fsrs, plainloop) {
    fuzz_plainloop("fsrs", LLVMFuzzerTestOneInput, NULL, 0, fuzz_seed());
}
