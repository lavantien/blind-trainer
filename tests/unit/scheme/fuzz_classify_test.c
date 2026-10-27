#include "fuzz/classify.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_classify, plainloop) {
    fuzz_plainloop("classify", LLVMFuzzerTestOneInput, NULL, 0, fuzz_seed());
}
