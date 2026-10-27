#include "verify/ev_mega.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "puzzles/tier1.h"

static const char *const MEGA_HONESTY =
    "4lll stage predicates are necessary conditions on the start state, the cubeskills case "
    "states do not stack as a strict eo co cp ep pipeline, placement is checked in the "
    "center-pinned frame";

size_t verify_mega_ring(void) {
    return sizeof BT_MEGA_NEIGHBOR_CW[BT_MEGA_U] / sizeof BT_MEGA_NEIGHBOR_CW[BT_MEGA_U][0];
}

bool verify_mega_edges_oriented(const bt_mega_state *state) {
    size_t ring = verify_mega_ring();
    for (size_t k = 0; k < ring; k++) {
        if (bt_mega_color(state, BT_MEGA_U, bt_mega_edge_slot(k)) != BT_MEGA_U) {
            return false;
        }
    }
    return true;
}

bool verify_mega_corners_oriented(const bt_mega_state *state) {
    size_t ring = verify_mega_ring();
    for (size_t k = 0; k < ring; k++) {
        if (bt_mega_color(state, BT_MEGA_U, bt_mega_corner_slot(k)) != BT_MEGA_U) {
            return false;
        }
    }
    return true;
}

bool verify_mega_corners_placed(const bt_mega_state *state) {
    size_t ring = verify_mega_ring();
    for (size_t side_at = 0; side_at < ring; side_at++) {
        uint8_t side = BT_MEGA_NEIGHBOR_CW[BT_MEGA_U][side_at];
        const uint8_t *side_ring = BT_MEGA_NEIGHBOR_CW[side];
        size_t pos = ring;
        for (size_t at = 0; at < ring; at++) {
            if (side_ring[at] == BT_MEGA_U) {
                pos = at;
                break;
            }
        }
        if (pos == ring) {
            return false;
        }
        if (bt_mega_color(state, side, bt_mega_corner_slot(pos)) != side) {
            return false;
        }
        if (bt_mega_color(state, side, bt_mega_corner_slot((pos + ring - 1) % ring)) != side) {
            return false;
        }
    }
    return true;
}

int verify_mega_stage_of(const char *tag, verify_mega_ll_stage *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_mega.stage_of");
    size_t len = strlen(tag);
    while (len > 0 && tag[len - 1] >= '0' && tag[len - 1] <= '9') {
        len -= 1;
    }
    char stem[32];
    snprintf(stem, sizeof stem, "%.*s", (int)len, tag);
    if (strcmp(stem, "EO") == 0) {
        *out = VERIFY_MEGA_STAGE_EO;
    } else if (strcmp(stem, "CO") == 0) {
        *out = VERIFY_MEGA_STAGE_CO;
    } else if (strcmp(stem, "CP") == 0) {
        *out = VERIFY_MEGA_STAGE_CP;
    } else if (strcmp(stem, "EP") == 0) {
        *out = VERIFY_MEGA_STAGE_EP;
    } else {
        snprintf(err, err_len, "tag %s names no 4lll stage", stem);
        BT_TRACE_FAIL("ev_mega.stage_of", "tag %s", tag);
        BT_TRACE_EXIT("ev_mega.stage_of");
        return 1;
    }
    BT_TRACE_EXIT("ev_mega.stage_of");
    return 0;
}

int verify_mega_tag_of(const char *front_json, char *out, size_t out_len) {
    BT_TRACE_ENTER("ev_mega.tag_of");
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        snprintf(out, out_len, "%s", json_err.message);
        BT_TRACE_FAIL("ev_mega.tag_of", "json parse failed: %s", json_err.message);
        BT_TRACE_EXIT("ev_mega.tag_of");
        return 1;
    }
    bt_json_value *field = bt_json_get(value, "case");
    bool ok = (bool)(field != NULL && field->type == BT_JSON_STRING);
    if (ok) {
        snprintf(out, out_len, "%s", field->as.string);
    } else {
        snprintf(out, out_len, "%s", "front_json holds no case tag");
    }
    bt_json_free(value);
    if (!ok) {
        BT_TRACE_FAIL("ev_mega.tag_of", "no case tag");
        BT_TRACE_EXIT("ev_mega.tag_of");
        return 1;
    }
    BT_TRACE_EXIT("ev_mega.tag_of");
    return 0;
}

int verify_mega_case_state(const char *alg_text, bt_mega_state *state, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_mega.case_state");
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_mega_engine();
    if (alg == NULL || !eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_mega.case_state", "parse failed %s", alg_text);
        BT_TRACE_EXIT("ev_mega.case_state");
        return 1;
    }
    if (bt_pz_alg_is_empty(alg)) {
        snprintf(err, err_len, "%s", "alg is empty");
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_mega.case_state", "empty alg");
        BT_TRACE_EXIT("ev_mega.case_state");
        return 1;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_mega_solved_state(state);
    if (!bt_mega_apply_alg(state, inverse, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, 64);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
        BT_TRACE_FAIL("ev_mega.case_state", "apply failed %s", alg_text);
        BT_TRACE_EXIT("ev_mega.case_state");
        return 1;
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    BT_TRACE_EXIT("ev_mega.case_state");
    return 0;
}

static bool centers_home(const bt_mega_state *state) {
    for (uint8_t face = 0; face < BT_MEGA_FACES; face++) {
        if (bt_mega_color(state, face, BT_MEGA_CENTER) != face) {
            return false;
        }
    }
    return true;
}

static bool corners_placed_in_center_frame(const bt_mega_state *state) {
    size_t ring = verify_mega_ring();
    bt_pz_error pz_err;
    void *unspin = bt_pz_alg_create();
    bt_pz_engine eng = bt_mega_engine();
    if (unspin == NULL || !eng.parse_alg(&eng, "y'", 2, unspin, &pz_err)) {
        bt_pz_alg_destroy(unspin);
        return false;
    }
    bt_mega_state aligned = *state;
    for (size_t at = 0; at < ring; at++) {
        if (centers_home(&aligned)) {
            bt_pz_alg_destroy(unspin);
            return verify_mega_corners_placed(&aligned);
        }
        if (!bt_mega_apply_alg(&aligned, unspin, &pz_err)) {
            bt_pz_alg_destroy(unspin);
            return false;
        }
    }
    bt_pz_alg_destroy(unspin);
    return false;
}

verify_tier2 verify_mega_stage_outcome(verify_mega_ll_stage stage, const bt_mega_state *state) {
    BT_TRACE_ENTER("ev_mega.stage_outcome");
    bool edges = verify_mega_edges_oriented(state);
    bool corners = verify_mega_corners_oriented(state);
    bool placed = corners_placed_in_center_frame(state);
    verify_tier2 outcome;
    switch (stage) {
    case VERIFY_MEGA_STAGE_EO:
        outcome = edges ? verify_tier2_fail("edges already oriented") : verify_tier2_pass();
        break;
    case VERIFY_MEGA_STAGE_CO:
        outcome = corners ? verify_tier2_fail("corners already oriented") : verify_tier2_pass();
        break;
    case VERIFY_MEGA_STAGE_CP:
        outcome = placed ? verify_tier2_fail("corners already placed") : verify_tier2_pass();
        break;
    default:
        if (!placed) {
            outcome = verify_tier2_fail("corners not placed");
        } else if (bt_mega_is_solved(state)) {
            outcome = verify_tier2_fail("start state is already solved");
        } else {
            outcome = verify_tier2_pass();
        }
        break;
    }
    BT_TRACE_EXIT("ev_mega.stage_outcome");
    return outcome;
}

void verify_mega_verify_4lll(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_mega.verify_4lll");
    bt_pz_engine eng = bt_mega_engine();
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
        bt_mega_state state;
        char tag[32];
        verify_mega_ll_stage stage;
        if (verify_mega_case_state(row->alg, &state, err, sizeof err) != 0 ||
            verify_mega_tag_of(row->front_json, tag, sizeof tag) != 0 ||
            verify_mega_stage_of(tag, &stage, err, sizeof err) != 0) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else {
            outcome.tier2 = verify_mega_stage_outcome(stage, &state);
        }
        verify_section_push_row(out, &outcome);
    }
    verify_section_push_honesty(out, MEGA_HONESTY);
    BT_TRACE_EXIT("ev_mega.verify_4lll");
}
