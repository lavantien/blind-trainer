#ifndef BT_FUZZ_FSRS_H
#define BT_FUZZ_FSRS_H

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/rng.h"
#include "common/strbuf.h"
#include "data/fsrs.h"
#include "data/srs.h"
#include "fuzz/fuzz_common.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BT_FUZZ_FSRS_WALK_MASK 0x5f5f44454f415531ULL
#define BT_FUZZ_FSRS_HOSTILE_MASK 0x5f5f484f53544931ULL
#define BT_FUZZ_FSRS_REPLAY_MASK 0x5f5f524550415931ULL
#define BT_FUZZ_FSRS_CONFIG_MASK 0x5f5f434647465a31ULL
#define BT_FUZZ_FSRS_REVIEW_MAX 64

static bt_grade bt_fuzz_fsrs_grade(bt_rng *rng) {
    return (bt_grade)(1 + bt_rng_below(rng, 4));
}

static double bt_fuzz_fsrs_bounded_elapsed(bt_rng *rng) {
    switch (bt_rng_below(rng, 4)) {
    case 0:
        return (double)bt_rng_below(rng, 10) / 10.0;
    case 1:
        return (double)bt_rng_below(rng, 3660);
    case 2:
        return (double)bt_rng_below(rng, 365000);
    default:
        return (double)bt_rng_below(rng, 1000000000);
    }
}

static double bt_fuzz_fsrs_from_bits(bt_rng *rng) {
    uint64_t bits = bt_rng_below(rng, UINT64_MAX);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void bt_fuzz_fsrs_default_walk(uint64_t seed) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, seed ^ BT_FUZZ_FSRS_WALK_MASK);
    bt_fsrs_memory memory = bt_fsrs_memory_initial(bt_fuzz_fsrs_grade(&rng), &cfg);
    BT_CHECK(bt_fsrs_memory_is_valid(memory) == 1);
    size_t rounds = (size_t)bt_rng_below(&rng, 600);
    for (size_t i = 0; i < rounds; i++) {
        bt_grade grade = bt_fuzz_fsrs_grade(&rng);
        double elapsed = bt_fuzz_fsrs_bounded_elapsed(&rng);
        memory = bt_fsrs_memory_review(memory, grade, elapsed, &cfg);
        if (!bt_fsrs_memory_is_valid(memory)) {
            BT_ABORT("default weights must keep the state valid, elapsed %.17g", elapsed);
        }
        long long interval = bt_fsrs_next_interval_days(memory.stability, &cfg);
        if (interval < 1 || interval > 36500) {
            BT_ABORT("interval %lld left the clamps", interval);
        }
        double retrievability = bt_fsrs_memory_retrievability(memory, elapsed);
        if (!isfinite(retrievability) || retrievability <= 0.0 || retrievability > 1.0) {
            BT_ABORT("retrievability %.17g left (0, 1]", retrievability);
        }
    }
}

static void bt_fuzz_fsrs_hostile_walk(uint64_t seed) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, seed ^ BT_FUZZ_FSRS_HOSTILE_MASK);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        switch (bt_rng_below(&rng, 3)) {
        case 0:
            break;
        case 1:
            cfg.weights[i] = (double)bt_rng_below(&rng, 10000) / 7.0;
            break;
        default:
            cfg.weights[i] = bt_fuzz_fsrs_from_bits(&rng);
            break;
        }
    }
    switch (bt_rng_below(&rng, 3)) {
    case 0:
        cfg.request_retention = 0.9;
        break;
    case 1:
        cfg.request_retention = (double)bt_rng_below(&rng, 1500) / 1000.0;
        break;
    default:
        cfg.request_retention = bt_fuzz_fsrs_from_bits(&rng);
        break;
    }
    cfg.interval_clamp_days[0] = (long long)bt_rng_below(&rng, 100) - 50;
    cfg.interval_clamp_days[1] = (long long)bt_rng_below(&rng, 100) - 50;
    bt_fsrs_memory memory = bt_fsrs_memory_initial(bt_fuzz_fsrs_grade(&rng), &cfg);
    size_t rounds = (size_t)bt_rng_below(&rng, 300);
    for (size_t i = 0; i < rounds; i++) {
        double elapsed;
        if (bt_rng_below(&rng, 3) == 0) {
            elapsed = bt_fuzz_fsrs_bounded_elapsed(&rng);
        } else {
            elapsed = bt_fuzz_fsrs_from_bits(&rng);
        }
        memory = bt_fsrs_memory_review(memory, bt_fuzz_fsrs_grade(&rng), elapsed, &cfg);
        (void)bt_fsrs_next_interval_days(memory.stability, &cfg);
        (void)bt_fsrs_memory_retrievability(memory, elapsed);
    }
}

static void bt_fuzz_fsrs_replay_walk(uint64_t seed) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, seed ^ BT_FUZZ_FSRS_REPLAY_MASK);
    bt_review_entry history[BT_FUZZ_FSRS_REVIEW_MAX];
    size_t history_count = 0;
    int any_bad_quality = 0;
    size_t rounds = (size_t)bt_rng_below(&rng, BT_FUZZ_FSRS_REVIEW_MAX);
    for (size_t i = 0; i < rounds; i++) {
        unsigned int quality;
        if (bt_rng_below(&rng, 4) == 0) {
            quality = (unsigned int)bt_rng_below(&rng, 8);
        } else {
            quality = 2 + (unsigned int)bt_rng_below(&rng, 4);
        }
        if (quality < 2 || quality > 5) {
            any_bad_quality = 1;
        }
        history[history_count++] = (bt_review_entry){
            (long long)i,
            quality,
            (long long)bt_rng_below(&rng, 4000) - 100,
            0,
        };
        int has_state = 0;
        bt_fsrs_memory state = {0.0, 0.0};
        int rc = bt_replay_sm2(history, history_count, &cfg, &has_state, &state);
        if (rc != 0) {
            if (any_bad_quality == 0) {
                BT_ABORT("replay rejected a history of valid qualities");
            }
            continue;
        }
        if (has_state == 0) {
            BT_CHECK(history_count == 0);
            continue;
        }
        if (!bt_fsrs_memory_is_valid(state)) {
            BT_ABORT("qualities 2..=5 must replay to a valid state");
        }
    }
}

static void bt_fuzz_fsrs_config_parse(uint64_t seed) {
    bt_rng rng;
    bt_rng_seed(&rng, seed ^ BT_FUZZ_FSRS_CONFIG_MASK);
    size_t rounds = (size_t)bt_rng_below(&rng, 32);
    for (size_t i = 0; i < rounds; i++) {
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_strbuf_append_str(&text, "{\"srs\":{\"fsrs\":{\"weights\":[");
        for (int w = 0; w < BT_FSRS_WEIGHT_COUNT; w++) {
            double weight;
            switch (bt_rng_below(&rng, 3)) {
            case 0:
                weight = 0.5;
                break;
            case 1:
                weight = (double)bt_rng_below(&rng, 2000);
                break;
            default:
                weight = bt_fuzz_fsrs_from_bits(&rng);
                break;
            }
            bt_strbuf_appendf(&text, "%s%.17g", w == 0 ? "" : ",", weight);
        }
        double retention;
        switch (bt_rng_below(&rng, 3)) {
        case 0:
            retention = 0.9;
            break;
        case 1:
            retention = (double)bt_rng_below(&rng, 1500) / 1000.0;
            break;
        default:
            retention = bt_fuzz_fsrs_from_bits(&rng);
            break;
        }
        long long lo = (long long)bt_rng_below(&rng, 10) - 3;
        long long hi = (long long)bt_rng_below(&rng, 40000);
        bt_strbuf_appendf(&text,
                          "],\"request_retention\":%.17g,\"interval_clamp_days\":[%lld,%lld]}}}",
                          retention, lo, hi);
        bt_json_error jerr;
        bt_json_value *root = NULL;
        if (bt_json_parse(text.data, text.len, &root, &jerr) != 0) {
            bt_strbuf_free(&text);
            continue;
        }
        bt_fsrs_config cfg;
        char error[96];
        int rc = bt_fsrs_config_from_json(root, &cfg, error, sizeof error);
        bt_json_free(root);
        bt_strbuf_free(&text);
        if (rc != 0) {
            continue;
        }
        for (int w = 0; w < BT_FSRS_WEIGHT_COUNT; w++) {
            BT_CHECK(isfinite(cfg.weights[w]));
        }
        if (!(cfg.request_retention > 0.0 && cfg.request_retention <= 1.0)) {
            BT_ABORT("retention %.17g accepted", retention);
        }
        if (cfg.interval_clamp_days[0] < 1 ||
            cfg.interval_clamp_days[1] < cfg.interval_clamp_days[0]) {
            BT_ABORT("clamps [%lld, %lld] accepted", cfg.interval_clamp_days[0],
                     cfg.interval_clamp_days[1]);
        }
    }
}

static void bt_fuzz_fsrs_once(const uint8_t *data, size_t size) {
    uint64_t seed = bt_fuzz_seed_from(data, size);
    bt_fuzz_fsrs_default_walk(seed);
    bt_fuzz_fsrs_hostile_walk(seed);
    bt_fuzz_fsrs_replay_walk(seed);
    bt_fuzz_fsrs_config_parse(seed);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!bt_fuzz_within_bounds()) {
        return 0;
    }
    bt_fuzz_fsrs_once(data, size);
    return 0;
}

#endif
