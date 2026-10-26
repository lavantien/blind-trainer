#ifndef BT_FUZZ_MEMO_H
#define BT_FUZZ_MEMO_H

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/rng.h"
#include "scheme/core_deps.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BT_FUZZ_GOLDEN 0x9E3779B97F4A7C15ULL

static uint64_t bt_fuzz_seed_from(const uint8_t *data, size_t size) {
    uint8_t bytes[8] = {0};
    size_t take = size < 8 ? size : 8;
    for (size_t i = 0; i < take; i++) {
        bytes[i] = data[i];
    }
    uint64_t value;
    memcpy(&value, bytes, sizeof value);
    return value ^ ((uint64_t)size * BT_FUZZ_GOLDEN);
}

static bool bt_fuzz_within_bounds(void) {
    static bool inited;
    static bool has_runs;
    static long long runs;
    static bool has_secs;
    static long long secs;
    static clock_t start;
    static uint64_t count;
    if (!inited) {
        const bt_json_value *config = bt_config_load();
        long long value = 0;
        has_runs = (bt_config_int(config, "fuzz.runs", &value) == 0 && value > 0) != 0;
        runs = value;
        value = 0;
        has_secs = (bt_config_int(config, "fuzz.secs", &value) == 0 && value > 0) != 0;
        secs = value;
        start = clock();
        inited = true;
    }
    count++;
    if (has_runs && count > (uint64_t)runs) {
        return false;
    }
    if (has_secs && (double)(clock() - start) / CLOCKS_PER_SEC >= (double)secs) {
        return false;
    }
    return true;
}

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
