#include "verify/ev_nxn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "verify/lib.h"

static const char *const TIP_WORDS[6] = {"", "x", "x2", "x'", "z", "z'"};
static const char *const TWIST_WORDS[4] = {"", "y", "y2", "y'"};

void verify_nxn_rotations_init(verify_nxn_rotations *rotations) {
    rotations->items = NULL;
    rotations->count = 0;
    rotations->cap = 0;
}

void verify_nxn_rotations_free(verify_nxn_rotations *rotations) {
    for (size_t at = 0; at < rotations->count; at++) {
        bt_pz_alg_destroy(rotations->items[at]);
    }
    free(rotations->items);
    rotations->items = NULL;
    rotations->count = 0;
    rotations->cap = 0;
}

static void rotations_push(verify_nxn_rotations *rotations, void *alg) {
    if (rotations->count == rotations->cap) {
        void **items = bt_realloc(rotations->items, VERIFY_NXN_ROTATION_COUNT * sizeof *items);
        if (items == NULL) {
            BT_TRACE_FAIL("ev_nxn.rotations_push", "oom at %zu", rotations->count);
            bt_pz_alg_destroy(alg);
            return;
        }
        rotations->items = items;
        rotations->cap = VERIFY_NXN_ROTATION_COUNT;
    }
    rotations->items[rotations->count] = alg;
    rotations->count += 1;
}

void verify_nxn_rotations_build(const bt_pz_nxn *engine, verify_nxn_rotations *out) {
    BT_TRACE_ENTER("ev_nxn.rotations_build");
    verify_nxn_rotations_init(out);
    bt_pz_engine eng = bt_pz_nxn_engine(engine);
    for (size_t tip = 0; tip < 6; tip++) {
        for (size_t twist = 0; twist < 4; twist++) {
            char text[16];
            snprintf(text, sizeof text, "%s%s%s", TIP_WORDS[tip],
                     TIP_WORDS[tip][0] == '\0' ? "" : " ", TWIST_WORDS[twist]);
            bt_pz_error err;
            void *alg = bt_pz_alg_create();
            eng.parse_alg(&eng, text, strlen(text), alg, &err);
            bt_pz_nxn_state state;
            bt_pz_nxn_solved_state(engine, &state);
            bt_pz_nxn_apply_alg(engine, &state, alg, &err);
            rotations_push(out, alg);
        }
    }
    BT_TRACE_EXIT("ev_nxn.rotations_build");
}

bool verify_nxn_d_layer_solved(const bt_pz_nxn *engine, const bt_pz_nxn_state *state) {
    BT_TRACE_ENTER("ev_nxn.d_layer_solved");
    bt_pz_grid grid;
    bt_pz_nxn_facelets(engine, state, &grid);
    const bt_pz_face *down = &grid.faces[5];
    uint8_t down_color = down->rows[0].cells[0];
    bool down_uniform = true;
    for (size_t row = 0; row < down->row_count; row++) {
        for (size_t col = 0; col < down->rows[row].cols; col++) {
            if (down->rows[row].cells[col] != down_color) {
                down_uniform = false;
            }
        }
    }
    bool sides_uniform = true;
    for (size_t side = 1; side <= 4; side++) {
        const bt_pz_face *face = &grid.faces[side];
        const bt_pz_row *bottom = &face->rows[face->row_count - 1];
        uint8_t color = bottom->cells[0];
        for (size_t col = 0; col < bottom->cols; col++) {
            if (bottom->cells[col] != color) {
                sides_uniform = false;
            }
        }
    }
    BT_TRACE_EXIT("ev_nxn.d_layer_solved");
    return (bool)(down_uniform && sides_uniform);
}

int verify_nxn_tier1_nonempty(const bt_pz_nxn *engine, const char *alg_text, char *err,
                              size_t err_len) {
    BT_TRACE_ENTER("ev_nxn.tier1_nonempty");
    bt_pz_engine eng = bt_pz_nxn_engine(engine);
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    if (alg == NULL || !eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_nxn.tier1_nonempty", "parse failed %s", alg_text);
        BT_TRACE_EXIT("ev_nxn.tier1_nonempty");
        return 1;
    }
    if (bt_pz_alg_is_empty(alg)) {
        snprintf(err, err_len, "%s", "alg is empty");
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_nxn.tier1_nonempty", "empty alg");
        BT_TRACE_EXIT("ev_nxn.tier1_nonempty");
        return 1;
    }
    bt_pz_alg_destroy(alg);
    BT_TRACE_EXIT("ev_nxn.tier1_nonempty");
    return 0;
}

verify_tier2 verify_nxn_tier2_cll(const bt_pz_nxn *engine, const verify_nxn_rotations *rotations,
                                  const char *alg_text) {
    BT_TRACE_ENTER("ev_nxn.tier2_cll");
    bt_pz_engine eng = bt_pz_nxn_engine(engine);
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    if (alg == NULL || !eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_pz_error_message(&pz_err, &sb);
        verify_tier2 fail = verify_tier2_fail(sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_nxn.tier2_cll", "parse failed %s", alg_text);
        BT_TRACE_EXIT("ev_nxn.tier2_cll");
        return fail;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_pz_nxn_state base;
    bt_pz_nxn_solved_state(engine, &base);
    bt_pz_nxn_apply_alg(engine, &base, inverse, &pz_err);
    for (size_t at = 0; at < rotations->count; at++) {
        bt_pz_nxn_state turned = base;
        bt_pz_nxn_apply_alg(engine, &turned, rotations->items[at], &pz_err);
        if (bt_pz_nxn_is_solved(engine, &turned)) {
            bt_pz_alg_destroy(alg);
            bt_pz_alg_destroy(inverse);
            BT_TRACE_FAIL("ev_nxn.tier2_cll", "already solved %s", alg_text);
            BT_TRACE_EXIT("ev_nxn.tier2_cll");
            return verify_tier2_fail("start state is already solved");
        }
    }
    for (size_t at = 0; at < rotations->count; at++) {
        bt_pz_nxn_state turned = base;
        bt_pz_nxn_apply_alg(engine, &turned, rotations->items[at], &pz_err);
        if (verify_nxn_d_layer_solved(engine, &turned)) {
            bt_pz_alg_destroy(alg);
            bt_pz_alg_destroy(inverse);
            BT_TRACE_EXIT("ev_nxn.tier2_cll");
            return verify_tier2_pass();
        }
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    BT_TRACE_EXIT("ev_nxn.tier2_cll");
    return verify_tier2_fail("no rotation leaves the bottom layer solved");
}

void verify_nxn_verify_twoby2(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_nxn.verify_twoby2");
    bt_pz_nxn engine;
    bt_pz_nxn_init(&engine, 2);
    verify_nxn_rotations rotations;
    verify_nxn_rotations_build(&engine, &rotations);
    if (rotations.count != VERIFY_NXN_ROTATION_COUNT) {
        BT_TRACE_FAIL("ev_nxn.verify_twoby2", "rotation count %zu", rotations.count);
        exit(1);
    }
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char err[VERIFY_ERR_MAX];
        if (verify_nxn_tier1_nonempty(&engine, row->alg, err, sizeof err) == 0) {
            outcome.tier1_ok = true;
        } else {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
        }
        outcome.tier2 = verify_nxn_tier2_cll(&engine, &rotations, row->alg);
        verify_section_push_row(out, &outcome);
    }
    verify_nxn_rotations_free(&rotations);
    BT_TRACE_EXIT("ev_nxn.verify_twoby2");
}
