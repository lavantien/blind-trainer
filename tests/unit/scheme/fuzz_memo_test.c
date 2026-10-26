#include "fuzz/memo.h"

#include "fuzz_runner.h"

BT_TEST(fuzz_memo, plainloop) {
    bt_fuzz_run_plainloop("tests/fuzz/corpus/memo", "memo.pending", LLVMFuzzerTestOneInput);
}
