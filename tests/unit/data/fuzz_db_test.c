#include "fuzz/db.h"
#include "fuzz/fuzz_common.h"

BT_TEST(fuzz_db, plainloop) {
    fuzz_plainloop("db", LLVMFuzzerTestOneInput, NULL, 0, fuzz_seed());
}
