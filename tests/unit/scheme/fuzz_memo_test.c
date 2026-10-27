#include "fuzz/fuzz_common.h"
#include "fuzz/memo.h"

BT_TEST(fuzz_memo, plainloop) {
    fuzz_plainloop("memo", LLVMFuzzerTestOneInput, NULL, 0, fuzz_seed());
}
