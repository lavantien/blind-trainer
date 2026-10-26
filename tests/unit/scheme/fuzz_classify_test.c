#include "fuzz/classify.h"

#include "fuzz_runner.h"

BT_TEST(fuzz_classify, plainloop) {
    bt_fuzz_run_plainloop("tests/fuzz/corpus/classify", "classify.pending", LLVMFuzzerTestOneInput);
}
