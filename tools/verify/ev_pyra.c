#include "verify/ev_pyra.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "puzzles/tier1.h"

static const size_t TIP_LOCALS[3] = {0, 4, 8};
static const size_t EDGE_LOCALS[3] = {1, 3, 6};
static const size_t CENTER_LOCALS[3] = {2, 5, 7};

static const char *const PYRA_HONESTY =
    "l4e tag prefixes (DS DF US UF NP) name source page sections, their per-row edge geometry "
    "is not derivable from the engine state, tier 2 pins corner and center facelets exactly home "
    "after composing out one U layer vertex adjustment and bounds the displaced edge facelets at "
    "4 to 8 in that frame";

uint8_t verify_pyra_flat(const bt_pz_grid *grid, size_t face, size_t local) {
    size_t row;
    size_t col;
    if (local == 0) {
        row = 0;
        col = 0;
    } else if (local < 4) {
        row = 1;
        col = local - 1;
    } else {
        row = 2;
        col = local - 4;
    }
    return grid->faces[face].rows[row].cells[col];
}

static bool band_home(const bt_pz_grid *grid, const bt_pz_grid *solved, const size_t *locals) {
    for (size_t face = 0; face < 4; face++) {
        for (size_t at = 0; at < 3; at++) {
            if (verify_pyra_flat(grid, face, locals[at]) !=
                verify_pyra_flat(solved, face, locals[at])) {
                return false;
            }
        }
    }
    return true;
}

static size_t edge_facelets_moved(const bt_pz_grid *grid, const bt_pz_grid *solved) {
    size_t moved = 0;
    for (size_t face = 0; face < 4; face++) {
        for (size_t at = 0; at < 3; at++) {
            if (verify_pyra_flat(grid, face, EDGE_LOCALS[at]) !=
                verify_pyra_flat(solved, face, EDGE_LOCALS[at])) {
                moved += 1;
            }
        }
    }
    return moved;
}

int verify_pyra_l4e_disturbance(const bt_pyra_state *state, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_pyra.l4e_disturbance");
    bt_pyra_state solved_state;
    bt_pyra_solved_state(&solved_state);
    bt_pz_grid solved;
    bt_pyra_facelets(&solved_state, &solved);
    bt_pz_error pz_err;
    void *quarter = bt_pz_alg_create();
    bt_pz_engine eng = bt_pyra_engine();
    if (quarter == NULL || !eng.parse_alg(&eng, "U", 1, quarter, &pz_err)) {
        bt_pz_alg_destroy(quarter);
        snprintf(err, err_len, "%s", "the U vertex token failed to parse");
        BT_TRACE_FAIL("ev_pyra.l4e_disturbance", "U token parse failed");
        BT_TRACE_EXIT("ev_pyra.l4e_disturbance");
        return -1;
    }
    for (size_t turns = 0; turns < VERIFY_PYRA_U_FRAME_WORDS; turns++) {
        bt_pyra_state framed = *state;
        for (size_t at = 0; at < turns; at++) {
            if (!bt_pyra_apply_alg(&framed, quarter, &pz_err)) {
                bt_pz_alg_destroy(quarter);
                snprintf(err, err_len, "%s", "the U vertex turn failed to apply");
                BT_TRACE_FAIL("ev_pyra.l4e_disturbance", "U apply failed");
                BT_TRACE_EXIT("ev_pyra.l4e_disturbance");
                return -1;
            }
        }
        bt_pz_grid grid;
        bt_pyra_facelets(&framed, &grid);
        if (!band_home(&grid, &solved, TIP_LOCALS) || !band_home(&grid, &solved, CENTER_LOCALS)) {
            continue;
        }
        size_t moved = edge_facelets_moved(&grid, &solved);
        bt_pz_alg_destroy(quarter);
        if (moved >= 4 && moved <= 8) {
            BT_TRACE_EXIT("ev_pyra.l4e_disturbance");
            return (int)moved;
        }
        snprintf(err, err_len, "%zu edge facelets moved, a last four edges case holds 4 to 8",
                 moved);
        BT_TRACE_FAIL("ev_pyra.l4e_disturbance", "%zu moved", moved);
        BT_TRACE_EXIT("ev_pyra.l4e_disturbance");
        return -1;
    }
    bt_pz_alg_destroy(quarter);
    snprintf(err, err_len, "%s", "corners and centers are not home in any U layer frame");
    BT_TRACE_FAIL("ev_pyra.l4e_disturbance", "no frame holds the bands home");
    BT_TRACE_EXIT("ev_pyra.l4e_disturbance");
    return -1;
}

int verify_pyra_case_state(const char *alg_text, bt_pyra_state *state, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_pyra.case_state");
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pyra_engine();
    if (alg == NULL || !eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_pyra.case_state", "parse failed %s", alg_text);
        BT_TRACE_EXIT("ev_pyra.case_state");
        return 1;
    }
    if (bt_pz_alg_is_empty(alg)) {
        snprintf(err, err_len, "%s", "alg is empty");
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_pyra.case_state", "empty alg");
        BT_TRACE_EXIT("ev_pyra.case_state");
        return 1;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_pyra_solved_state(state);
    if (!bt_pyra_apply_alg(state, inverse, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
        BT_TRACE_FAIL("ev_pyra.case_state", "apply failed %s", alg_text);
        BT_TRACE_EXIT("ev_pyra.case_state");
        return 1;
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    BT_TRACE_EXIT("ev_pyra.case_state");
    return 0;
}

void verify_pyra_verify_l4e(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_pyra.verify_l4e");
    bt_pz_engine eng = bt_pyra_engine();
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char err[VERIFY_ERR_MAX];
        if (row->alg[0] == '\0') {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", "alg is empty");
        } else {
            bt_tier1_error tier1_err;
            if (bt_verify_tier1(&eng, row->alg, &tier1_err) == BT_TIER1_OK) {
                outcome.tier1_ok = true;
            } else {
                bt_strbuf sb;
                bt_strbuf_init(&sb, 64);
                bt_tier1_error_message(&tier1_err, &sb);
                snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", sb.data);
                bt_strbuf_free(&sb);
            }
        }
        bt_pyra_state state;
        if (verify_pyra_case_state(row->alg, &state, err, sizeof err) != 0) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else if (verify_pyra_l4e_disturbance(&state, err, sizeof err) >= 0) {
            outcome.tier2 = verify_tier2_pass();
        } else {
            outcome.tier2 = verify_tier2_fail(err);
        }
        verify_section_push_row(out, &outcome);
    }
    verify_section_push_honesty(out, PYRA_HONESTY);
    BT_TRACE_EXIT("ev_pyra.verify_l4e");
}
