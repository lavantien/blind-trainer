#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "puzzles/geometry.h"
#include "puzzles/tier1.h"

#include <string.h>

typedef struct {
    uint8_t kind;
    uint8_t quarter;
} cycle_move;

enum {
    CYCLE_CLOCK = 0,
    CYCLE_COUNTER = 1,
    CYCLE_SLASH = 2,
};

typedef struct {
    bool identity_invert;
    bool counter_shifts_forward;
    bool slash_needs_solved;
    bool counter_lands_off;
} cycle_cfg;

typedef struct {
    int32_t turn;
} cycle_state;

static bt_pz_engine cycle_engine(const cycle_cfg *cfg) {
    return (bt_pz_engine){
        .name = "cycle",
        .self = cfg,
        .move_size = sizeof(cycle_move),
        .state_size = sizeof(cycle_state),
    };
}

static bool cycle_parse_move(const bt_pz_engine *eng, const char *token, size_t len, void *out_move,
                             bt_pz_error *err) {
    (void)eng;
    cycle_move *mv = out_move;
    if (len == 1 && token[0] == 's') {
        mv->kind = CYCLE_SLASH;
        mv->quarter = 1;
        return true;
    }
    if (len == 0 || len > 2) {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
        return false;
    }
    uint8_t quarter = 1;
    if (len == 2) {
        if (token[1] == '2') {
            quarter = 2;
        } else if (token[1] == '\'') {
            quarter = 3;
        } else {
            bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
            return false;
        }
    }
    if (token[0] == 'a') {
        mv->kind = CYCLE_CLOCK;
    } else if (token[0] == 'b') {
        mv->kind = CYCLE_COUNTER;
    } else {
        bt_pz_error_set(err, BT_PZ_ERR_PARSE, token, len);
        return false;
    }
    mv->quarter = quarter;
    return true;
}

static void cycle_append_suffix(bt_strbuf *out, char base, uint8_t quarter) {
    char text[3] = {base, '\0', '\0'};
    if (quarter == 2) {
        text[1] = '2';
    } else if (quarter == 3) {
        text[1] = '\'';
    }
    bt_strbuf_append_str(out, text);
}

static void cycle_format_alg(const bt_pz_engine *eng, const void *alg, bt_strbuf *out) {
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        const cycle_move *mv = bt_pz_alg_at(eng, alg, i);
        if (mv->kind == CYCLE_SLASH) {
            bt_strbuf_append_str(out, "s");
        } else {
            cycle_append_suffix(out, mv->kind == CYCLE_CLOCK ? 'a' : 'b', mv->quarter);
        }
    }
}

static void cycle_invert_alg(const bt_pz_engine *eng, const void *alg, void *out) {
    const cycle_cfg *cfg = eng->self;
    bt_pz_alg *vec = out;
    vec->len = 0;
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const cycle_move *src = bt_pz_alg_at(eng, alg, len - 1 - i);
        cycle_move flipped = *src;
        if (!cfg->identity_invert) {
            if (flipped.kind == CYCLE_CLOCK) {
                flipped.kind = CYCLE_COUNTER;
            } else if (flipped.kind == CYCLE_COUNTER) {
                flipped.kind = CYCLE_CLOCK;
            }
        }
        bt_pz_alg_push(eng, out, &flipped);
    }
}

static bool cycle_apply_alg(const bt_pz_engine *eng, void *state, const void *alg,
                            bt_pz_error *err) {
    const cycle_cfg *cfg = eng->self;
    cycle_state *turn_state = state;
    size_t len = bt_pz_alg_len(alg);
    for (size_t i = 0; i < len; i++) {
        const cycle_move *mv = bt_pz_alg_at(eng, alg, i);
        if (mv->kind == CYCLE_CLOCK) {
            turn_state->turn += (int32_t)mv->quarter;
        } else if (mv->kind == CYCLE_COUNTER) {
            if (cfg->counter_lands_off && turn_state->turn == 0) {
                turn_state->turn = 7;
            } else if (cfg->counter_shifts_forward) {
                turn_state->turn += (int32_t)mv->quarter;
            } else {
                turn_state->turn -= (int32_t)mv->quarter;
            }
        } else if (cfg->slash_needs_solved && turn_state->turn != 0) {
            bt_pz_error_set(err, BT_PZ_ERR_APPLY, "s", strlen("s"));
            return false;
        }
    }
    return true;
}

static void cycle_solved_state(const bt_pz_engine *eng, void *out_state) {
    (void)eng;
    cycle_state *turn_state = out_state;
    turn_state->turn = 0;
}

static bool cycle_is_solved(const bt_pz_engine *eng, const void *state) {
    (void)eng;
    const cycle_state *turn_state = state;
    return turn_state->turn == 0;
}

static void cycle_facelets(const bt_pz_engine *eng, const void *state, bt_pz_grid *out) {
    (void)eng;
    const cycle_state *turn_state = state;
    out->face_count = 1;
    out->faces[0].row_count = 1;
    out->faces[0].rows[0].cols = 1;
    out->faces[0].rows[0].cells[0] = (uint8_t)turn_state->turn;
}

static bt_pz_engine cycle_engine_full(const cycle_cfg *cfg) {
    bt_pz_engine eng = cycle_engine(cfg);
    eng.parse_move = cycle_parse_move;
    eng.parse_alg = bt_pz_alg_default_parse;
    eng.format_alg = cycle_format_alg;
    eng.invert_alg = cycle_invert_alg;
    eng.apply_alg = cycle_apply_alg;
    eng.solved_state = cycle_solved_state;
    eng.is_solved = cycle_is_solved;
    eng.facelets = cycle_facelets;
    return eng;
}

static cycle_cfg cycle_sound(void) {
    return (cycle_cfg){
        .identity_invert = false,
        .counter_shifts_forward = false,
        .slash_needs_solved = false,
        .counter_lands_off = false,
    };
}

static void tier1_message(const bt_tier1_error *err, char *out, size_t out_size) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_tier1_error_message(err, &sb);
    snprintf(out, out_size, "%s", sb.data);
    bt_strbuf_free(&sb);
}

static bool cycle_parse_text(bt_pz_engine *eng, const char *text, void *alg, bt_pz_error *err) {
    return eng->parse_alg(eng, text, strlen(text), alg, err);
}

BT_TEST(tier1, default_parse_splits_on_unicode_whitespace) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_pz_error perr;
    static const char *texts[] = {
        "a\xC2\x85 b",     "a\xC2\xA0 b",     "a\xE1\x9A\x80 b",
        "a\xE2\x80\x80 b", "a\xE2\x80\xA8 b", "a\xE2\x80\xAF b",
        "a\xE2\x81\x9F b", "a\xE3\x80\x80 b", "a\v b",
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        void *alg = bt_pz_alg_create();
        BT_CHECK(cycle_parse_text(&eng, texts[i], alg, &perr));
        BT_CHECK_EQ(bt_pz_alg_len(alg), (size_t)2);
        bt_strbuf sb;
        bt_strbuf_init(&sb, 32);
        eng.format_alg(&eng, alg, &sb);
        BT_CHECK_EQ(sb.data, "a b");
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
    }
    void *fused = bt_pz_alg_create();
    BT_CHECK(cycle_parse_text(&eng,
                              "a\xC2\xA0"
                              "s",
                              fused, &perr));
    BT_CHECK_EQ(bt_pz_alg_len(fused), (size_t)2);
    bt_pz_alg_destroy(fused);
}

BT_TEST(tier1, accepts_valid_algs) {
    static const char *texts[] = {
        "", "a", "b", "s", "a b", "a a a a", "a2 b' a2", "  a   b2  ", "a s b",
    };
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        BT_CHECK_EQ(bt_verify_tier1(&eng, texts[i], &err), (int)BT_TIER1_OK);
    }
}

BT_TEST(tier1, rejects_garbage_tokens) {
    static const char *texts[] = {"c", "a3", "a''", "2a", "s2", "A", "a z b", "-"};
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        BT_CHECK_EQ(bt_verify_tier1(&eng, texts[i], &err), (int)BT_TIER1_PARSE);
    }
}

BT_TEST(tier1, parse_failure_carries_the_offending_token) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a z b", &err), (int)BT_TIER1_PARSE);
    BT_CHECK_EQ(err.cause.kind, (int)BT_PZ_ERR_PARSE);
    BT_CHECK(strcmp(err.cause.token, "z") == 0);
}

BT_TEST(tier1, rejects_identity_inversion) {
    cycle_cfg cfg = cycle_sound();
    cfg.identity_invert = true;
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a", &err), (int)BT_TIER1_FORWARD);
}

BT_TEST(tier1, rejects_apply_sign_drift) {
    cycle_cfg cfg = cycle_sound();
    cfg.counter_shifts_forward = true;
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a", &err), (int)BT_TIER1_FORWARD);
}

BT_TEST(tier1, rejects_non_bijective_inverse_landing) {
    cycle_cfg cfg = cycle_sound();
    cfg.counter_lands_off = true;
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a", &err), (int)BT_TIER1_REVERSE);
}

BT_TEST(tier1, rejects_alg_legal_forward_but_illegal_reversed) {
    cycle_cfg cfg = cycle_sound();
    cfg.slash_needs_solved = true;
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a b", &err), (int)BT_TIER1_OK);
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a b s a", &err), (int)BT_TIER1_APPLY);
    BT_CHECK_EQ(err.cause.kind, (int)BT_PZ_ERR_APPLY);
    BT_CHECK(strcmp(err.cause.token, "s") == 0);
}

BT_TEST(tier1, cycle_mock_moves_and_renders_honestly) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    void *alg = bt_pz_alg_create();
    bt_pz_error perr;
    BT_CHECK(cycle_parse_text(&eng, "a2 b' s", alg, &perr));
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    eng.format_alg(&eng, alg, &sb);
    BT_CHECK_EQ(sb.data, "a2 b' s");
    void *inverted = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverted);
    bt_strbuf isb;
    bt_strbuf_init(&isb, 32);
    eng.format_alg(&eng, inverted, &isb);
    BT_CHECK_EQ(isb.data, "s a' b2");
    void *single = bt_pz_alg_create();
    BT_CHECK(cycle_parse_text(&eng, "a", single, &perr));
    cycle_state state;
    eng.solved_state(&eng, &state);
    BT_CHECK(eng.apply_alg(&eng, &state, single, &perr));
    BT_CHECK(!eng.is_solved(&eng, &state));
    bt_pz_grid grid;
    eng.facelets(&eng, &state, &grid);
    BT_CHECK_EQ(grid.faces[0].rows[0].cells[0], (uint8_t)1);
    cycle_state solved;
    eng.solved_state(&eng, &solved);
    eng.facelets(&eng, &solved, &grid);
    BT_CHECK_EQ(grid.faces[0].rows[0].cells[0], (uint8_t)0);
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverted);
    bt_pz_alg_destroy(single);
    bt_strbuf_free(&sb);
    bt_strbuf_free(&isb);
}

BT_TEST(tier1, verify_reports_the_ok_status_for_callers_skipping_the_return) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err = {.status = BT_TIER1_FORWARD, .cause = {0}};
    BT_CHECK_EQ(bt_verify_tier1(&eng, "a b", &err), (int)BT_TIER1_OK);
    BT_CHECK_EQ(err.status, (int)BT_TIER1_OK);
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    bt_tier1_error_message(&err, &sb);
    BT_CHECK_EQ(sb.len, (size_t)0);
    bt_strbuf_free(&sb);
}

BT_TEST(tier1, cycle_invert_replaces_a_reused_out_alg) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_pz_error perr;
    void *alg = bt_pz_alg_create();
    BT_CHECK(cycle_parse_text(&eng, "a b", alg, &perr));
    void *reused = bt_pz_alg_create();
    BT_CHECK(cycle_parse_text(&eng, "s", reused, &perr));
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)1);
    eng.invert_alg(&eng, alg, reused);
    BT_CHECK_EQ(bt_pz_alg_len(reused), (size_t)2);
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    eng.format_alg(&eng, reused, &sb);
    BT_CHECK_EQ(sb.data, "a b");
    bt_strbuf_free(&sb);
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(reused);
}

BT_TEST(tier1, tier1_error_displays_its_cause) {
    cycle_cfg cfg = cycle_sound();
    bt_pz_engine eng = cycle_engine_full(&cfg);
    bt_tier1_error err;
    BT_CHECK_EQ(bt_verify_tier1(&eng, "q", &err), (int)BT_TIER1_PARSE);
    char text[96];
    tier1_message(&err, text, sizeof text);
    BT_CHECK_EQ(text, "tier 1 parse failed: unknown token: q");
    bt_tier1_error forward = {.status = BT_TIER1_FORWARD};
    tier1_message(&forward, text, sizeof text);
    BT_CHECK_EQ(text, "tier 1 round trip failed: alg then inverse");
    bt_tier1_error reverse = {.status = BT_TIER1_REVERSE};
    tier1_message(&reverse, text, sizeof text);
    BT_CHECK_EQ(text, "tier 1 round trip failed: inverse then alg");
}
