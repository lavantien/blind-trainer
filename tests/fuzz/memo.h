#ifndef BT_FUZZ_MEMO_H
#define BT_FUZZ_MEMO_H

#include "common/bt_harness.h"
#include "fuzz/fuzz_common.h"
#include "scheme/core_deps.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void bt_fuzz_memo_once(const uint8_t *data, size_t size) {
    bt_cube cube;
    bt_scheme_random_state(bt_fuzz_seed_from(data, size), &cube);
    uint8_t occ[54];
    bt_scheme_state_array(&cube, occ);
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    bt_scheme_apply_memo(occ, &memo);
    for (uint8_t i = 0; i < 54; i++) {
        if (occ[i] != i) {
            BT_ABORT("memo round trip failed at slot %u", i);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!bt_fuzz_within_bounds()) {
        return 0;
    }
    bt_fuzz_memo_once(data, size);
    return 0;
}

#endif
