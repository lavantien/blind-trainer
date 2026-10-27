#include "puzzles/skewb.h"
#include "puzzles/tier1.h"

#include "common/bt_harness.h"

#include <string.h>

static bool tier1_ok(const bt_skewb *skewb, const char *text) {
    bt_pz_engine eng = bt_skewb_engine(skewb);
    bt_tier1_error err;
    return bt_verify_tier1(&eng, text, &err) == BT_TIER1_OK;
}

static uint64_t xorshift(uint64_t *seed) {
    uint64_t x = *seed;
    x ^= x << 13U;
    x ^= x >> 7U;
    x ^= x << 17U;
    *seed = x;
    return x;
}

BT_TEST(skewb_inline, solved_state_is_solved) {
    bt_skewb skewb = bt_skewb_new();
    bt_skewb_state state;
    bt_skewb_solved_state(&skewb, &state);
    BT_CHECK(bt_skewb_is_solved(&skewb, &state));
}

BT_TEST(skewb_inline, every_single_turn_breaks_and_reverts) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    bt_pz_error err;
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_skewb_move move = {vertex, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_skewb_state state;
            bt_skewb_solved_state(&skewb, &state);
            if (!bt_skewb_apply_alg(&skewb, &state, alg, &err)) {
                BT_ABORT("apply failed: %s", err.token);
            }
            BT_CHECK(!bt_skewb_is_solved(&skewb, &state));
            void *inverse = bt_pz_alg_create();
            bt_skewb_invert_alg(&skewb, alg, inverse);
            if (!bt_skewb_apply_alg(&skewb, &state, inverse, &err)) {
                BT_ABORT("apply inverse failed: %s", err.token);
            }
            BT_CHECK(bt_skewb_is_solved(&skewb, &state));
            bt_pz_alg_destroy(inverse);
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(skewb_inline, move_tokens_round_trip_through_format) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    for (uint8_t vertex = 0; vertex < 4; vertex++) {
        for (uint8_t quarter = 1; quarter <= 2; quarter++) {
            void *alg = bt_pz_alg_create();
            bt_skewb_move move = {vertex, quarter};
            bt_pz_alg_push(&eng, alg, &move);
            bt_strbuf text;
            bt_strbuf_init(&text, 8);
            bt_skewb_format_alg(&skewb, alg, &text);
            bt_skewb_move parsed;
            BT_CHECK(bt_skewb_parse_move(text.data, text.len, &parsed, NULL));
            BT_CHECK(bt_skewb_move_eq(parsed, move));
            bt_strbuf_free(&text);
            bt_pz_alg_destroy(alg);
        }
    }
}

BT_TEST(skewb_inline, inverse_round_trips_a_deterministic_alg_sweep) {
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine eng = bt_skewb_engine(&skewb);
    uint64_t seed = 0x5eed1234abcdf00dULL;
    bt_pz_error err;
    for (size_t iteration = 0; iteration < 4096; iteration++) {
        size_t length = (size_t)(xorshift(&seed) % 24);
        void *alg = bt_pz_alg_create();
        for (size_t i = 0; i < length; i++) {
            bt_skewb_move move = {
                (uint8_t)(xorshift(&seed) % 4),
                (uint8_t)(xorshift(&seed) % 2 + 1),
            };
            bt_pz_alg_push(&eng, alg, &move);
        }
        void *inverse = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, alg, inverse);
        bt_skewb_state forward;
        bt_skewb_state reverse;
        bt_skewb_solved_state(&skewb, &forward);
        bt_skewb_solved_state(&skewb, &reverse);
        if (!bt_skewb_apply_alg(&skewb, &forward, alg, &err) ||
            !bt_skewb_apply_alg(&skewb, &forward, inverse, &err) ||
            !bt_skewb_is_solved(&skewb, &forward)) {
            BT_ABORT("forward round trip failed");
        }
        if (!bt_skewb_apply_alg(&skewb, &reverse, inverse, &err) ||
            !bt_skewb_apply_alg(&skewb, &reverse, alg, &err) ||
            !bt_skewb_is_solved(&skewb, &reverse)) {
            BT_ABORT("reverse round trip failed");
        }
        bt_strbuf text;
        bt_strbuf_init(&text, 256);
        bt_skewb_format_alg(&skewb, alg, &text);
        void *reparsed = bt_pz_alg_create();
        if (!bt_skewb_parse_alg(&skewb, text.data, text.len, reparsed, &err) ||
            bt_pz_alg_len(reparsed) != bt_pz_alg_len(alg)) {
            BT_ABORT("format round trip failed for %s", text.data);
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
            bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, reparsed, i);
            if (!bt_skewb_move_eq(a, b)) {
                BT_ABORT("format round trip differs for %s", text.data);
            }
        }
        void *twice = bt_pz_alg_create();
        bt_skewb_invert_alg(&skewb, inverse, twice);
        if (bt_pz_alg_len(twice) != bt_pz_alg_len(alg)) {
            BT_ABORT("double inverse length differs");
        }
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_skewb_move a = *(const bt_skewb_move *)bt_pz_alg_at(&eng, alg, i);
            bt_skewb_move b = *(const bt_skewb_move *)bt_pz_alg_at(&eng, twice, i);
            if (!bt_skewb_move_eq(a, b)) {
                BT_ABORT("double inverse differs");
            }
        }
        if (!tier1_ok(&skewb, text.data)) {
            BT_ABORT("tier1 failed for %s", text.data);
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(reparsed);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(inverse);
        bt_pz_alg_destroy(alg);
    }
}
