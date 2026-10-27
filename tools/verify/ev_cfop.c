#include "verify/ev_cfop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"

static const uint8_t U_BAND_POSITIONS[9] = {0, 1, 2, 3, 24, 25, 26, 27, 48};

static size_t face_of_code(uint8_t code) {
    if (code < 24) {
        return code / 4;
    }
    if (code < 48) {
        return (code - 24) / 4;
    }
    return code - 48;
}

static bool band_contains(const uint8_t *band, size_t band_len, uint8_t position) {
    for (size_t at = 0; at < band_len; at++) {
        if (band[at] == position) {
            return true;
        }
    }
    return false;
}

size_t verify_cfop_u_band(const verify_bld_layout *layout, uint8_t *out, size_t cap) {
    BT_TRACE_ENTER("ev_cfop.u_band");
    uint8_t u_bit = (uint8_t)(1U << 0U);
    size_t count = 0;
    for (size_t index = 0; index < layout->piece_count && count < cap; index++) {
        if ((layout->masks[index] & u_bit) == 0) {
            continue;
        }
        for (size_t at = 0; at < layout->piece_lens[index] && count < cap; at++) {
            out[count] = layout->pieces[index][at];
            count += 1;
        }
    }
    BT_TRACE_EXIT("ev_cfop.u_band");
    return count;
}

bool verify_cfop_f2l_exact(const bt_cube *state, const uint8_t *band, size_t band_len) {
    BT_TRACE_ENTER("ev_cfop.f2l_exact");
    for (uint8_t position = 0; position < VERIFY_BLD_POSITION_COUNT; position++) {
        if (band_contains(band, band_len, position)) {
            continue;
        }
        if (bt_cube_sticker_at(state, position) != position) {
            BT_TRACE_EXIT("ev_cfop.f2l_exact");
            return false;
        }
    }
    BT_TRACE_EXIT("ev_cfop.f2l_exact");
    return true;
}

static bool u_uniform(const bt_cube *state) {
    for (size_t at = 0; at < sizeof U_BAND_POSITIONS; at++) {
        if (face_of_code(bt_cube_sticker_at(state, U_BAND_POSITIONS[at])) != 0) {
            return false;
        }
    }
    return true;
}

static const char *const ROTATION_TIPS[6] = {"", "x", "x2", "x'", "z", "z'"};
static const char *const ROTATION_TWISTS[4] = {"", "y", "y2", "y'"};

static size_t rotations(bt_alg out[24]) {
    size_t count = 0;
    char word[16];
    for (size_t tip = 0; tip < 6; tip++) {
        for (size_t twist = 0; twist < 4; twist++) {
            if (ROTATION_TIPS[tip][0] == '\0') {
                snprintf(word, sizeof word, "%s", ROTATION_TWISTS[twist]);
            } else if (ROTATION_TWISTS[twist][0] == '\0') {
                snprintf(word, sizeof word, "%s", ROTATION_TIPS[tip]);
            } else {
                snprintf(word, sizeof word, "%s %s", ROTATION_TIPS[tip], ROTATION_TWISTS[twist]);
            }
            bt_alg_init(&out[count]);
            char token[64];
            if (bt_alg_parse(word, &out[count], token, sizeof token)) {
                count += 1;
            }
        }
    }
    return count;
}

static size_t rotated_states(const bt_cube *state, bt_cube out[24]) {
    bt_alg words[24];
    size_t count = rotations(words);
    for (size_t at = 0; at < count; at++) {
        out[at] = *state;
        bt_cube_apply_alg(&out[at], &words[at]);
        bt_alg_free(&words[at]);
    }
    return count;
}

static bool cross_solved_on_some_face(const bt_cube *state, const verify_bld_layout *layout) {
    for (bt_face face = 0; face < 6; face++) {
        uint8_t bit = (uint8_t)(1U << (unsigned)face);
        bool solved = true;
        for (size_t index = 0; index < layout->piece_count; index++) {
            uint8_t mask = layout->masks[index];
            uint8_t layers = 0;
            for (uint8_t bits = mask; bits != 0; bits = (uint8_t)(bits >> 1U)) {
                layers = (uint8_t)(layers + (bits & 1U));
            }
            if (layers != 2 || (mask & bit) == 0) {
                continue;
            }
            for (size_t at = 0; at < layout->piece_lens[index]; at++) {
                uint8_t position = layout->pieces[index][at];
                if (bt_cube_sticker_at(state, position) != position) {
                    solved = false;
                }
            }
        }
        if (solved) {
            return true;
        }
    }
    return false;
}

static int slot_pair_displaced(const char *front_json, const bt_cube *state,
                               const verify_bld_layout *layout, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_cfop.slot_pair_displaced");
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        snprintf(err, err_len, "%s", json_err.message);
        BT_TRACE_FAIL("ev_cfop.slot_pair_displaced", "json failed");
        BT_TRACE_EXIT("ev_cfop.slot_pair_displaced");
        return -1;
    }
    const bt_json_value *field = bt_json_get(value, "slot");
    bool ok = (bool)(field != NULL && field->type == BT_JSON_STRING);
    if (!ok) {
        bt_json_free(value);
        snprintf(err, err_len, "%s", "f2l row names no slot");
        BT_TRACE_FAIL("ev_cfop.slot_pair_displaced", "no slot");
        BT_TRACE_EXIT("ev_cfop.slot_pair_displaced");
        return -1;
    }
    const char *slot = field->as.string;
    uint8_t mask = 0;
    bool bad = false;
    for (const char *cursor = slot; *cursor != '\0'; cursor++) {
        switch (*cursor) {
        case 'u':
        case 'U':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_U));
            break;
        case 'l':
        case 'L':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_L));
            break;
        case 'f':
        case 'F':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_F));
            break;
        case 'r':
        case 'R':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_R));
            break;
        case 'b':
        case 'B':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_B));
            break;
        case 'd':
        case 'D':
            mask = (uint8_t)(mask | (1U << (unsigned)BT_FACE_D));
            break;
        default:
            bad = true;
            break;
        }
    }
    bt_json_free(value);
    if (bad) {
        snprintf(err, err_len, "slot %s holds a non-face letter", slot);
        BT_TRACE_FAIL("ev_cfop.slot_pair_displaced", "bad slot %s", slot);
        BT_TRACE_EXIT("ev_cfop.slot_pair_displaced");
        return -1;
    }
    uint8_t u_bit = (uint8_t)(1U << 0U);
    for (size_t index = 0; index < layout->piece_count; index++) {
        uint8_t piece_mask = layout->masks[index];
        if ((piece_mask & u_bit) != 0) {
            continue;
        }
        bool home = true;
        for (size_t at = 0; at < layout->piece_lens[index]; at++) {
            uint8_t position = layout->pieces[index][at];
            if (bt_cube_sticker_at(state, position) != position) {
                home = false;
            }
        }
        if (home) {
            continue;
        }
        if ((piece_mask & mask) == mask) {
            BT_TRACE_EXIT("ev_cfop.slot_pair_displaced");
            return 1;
        }
    }
    BT_TRACE_EXIT("ev_cfop.slot_pair_displaced");
    return 0;
}

static int perm_of(const char *text, uint8_t perm[VERIFY_BLD_POSITION_COUNT]) {
    BT_TRACE_ENTER("ev_cfop.perm_of");
    bt_alg alg;
    bt_alg_init(&alg);
    char token[64];
    if (!bt_alg_parse(text, &alg, token, sizeof token)) {
        bt_alg_free(&alg);
        BT_TRACE_FAIL("ev_cfop.perm_of", "parse failed %s", text);
        BT_TRACE_EXIT("ev_cfop.perm_of");
        return 1;
    }
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &alg);
    bt_alg_free(&alg);
    for (uint8_t position = 0; position < VERIFY_BLD_POSITION_COUNT; position++) {
        perm[position] = bt_cube_sticker_at(&cube, position);
    }
    BT_TRACE_EXIT("ev_cfop.perm_of");
    return 0;
}

static const char *const MOVE_SUFFIXES[4] = {"", "2", "'", "2'"};

static int split_suffix(const char *token, char *letter, size_t letter_cap, char *suffix,
                        size_t suffix_cap) {
    size_t len = 0;
    while ((token[len] >= 'a' && token[len] <= 'z') || (token[len] >= 'A' && token[len] <= 'Z') ||
           token[len] == 'w') {
        len += 1;
    }
    snprintf(letter, letter_cap, "%.*s", (int)len, token);
    snprintf(suffix, suffix_cap, "%s", token + len);
    if (letter[0] == '\0') {
        return 1;
    }
    bool known = false;
    for (size_t at = 0; at < 4; at++) {
        if (strcmp(suffix, MOVE_SUFFIXES[at]) == 0) {
            known = true;
        }
    }
    return known ? 0 : 1;
}

static const char *const FACE_LETTERS[6] = {"U", "L", "F", "R", "B", "D"};
static const char *const WIDE_LETTERS[6] = {"u", "l", "f", "r", "b", "d"};
static const char *const SLICE_LETTERS[3] = {"M", "E", "S"};

static bool letter_in(const char *const *set, size_t count, const char *letter) {
    for (size_t at = 0; at < count; at++) {
        if (strcmp(set[at], letter) == 0) {
            return true;
        }
    }
    return false;
}

static const char *const *candidates_of(const char *letter, size_t *count) {
    size_t letter_len = strlen(letter);
    char head[2] = {letter[0], '\0'};
    if (letter_in(FACE_LETTERS, 6, letter)) {
        *count = 6;
        return FACE_LETTERS;
    }
    if (letter_in(WIDE_LETTERS, 6, letter) ||
        (letter_len == 2 && letter[1] == 'w' && letter_in(FACE_LETTERS, 6, head))) {
        *count = 6;
        return WIDE_LETTERS;
    }
    if (letter_in(SLICE_LETTERS, 3, letter)) {
        *count = 3;
        return SLICE_LETTERS;
    }
    return NULL;
}

int verify_cfop_rewrite_to_fixed(const char *alg_text, char *out, size_t out_len) {
    BT_TRACE_ENTER("ev_cfop.rewrite_to_fixed");
    char frame[256] = {0};
    size_t frame_len = 0;
    size_t used = 0;
    const char *cursor = alg_text;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor += 1;
        }
        size_t len = (size_t)(cursor - start);
        char token[32];
        snprintf(token, sizeof token, "%.*s", (int)len, start);
        if (token[0] == '\0') {
            continue;
        }
        char letter[16];
        char suffix[8];
        if (split_suffix(token, letter, sizeof letter, suffix, sizeof suffix) != 0) {
            snprintf(out, out_len, "token %s holds no canonical move", token);
            BT_TRACE_FAIL("ev_cfop.rewrite_to_fixed", "bad token %s", token);
            BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
            return 1;
        }
        if (strcmp(letter, "x") == 0 || strcmp(letter, "y") == 0 || strcmp(letter, "z") == 0) {
            if (frame_len + len + 1 < sizeof frame) {
                if (frame_len > 0) {
                    frame[frame_len++] = ' ';
                }
                memcpy(frame + frame_len, token, len);
                frame_len += len;
                frame[frame_len] = '\0';
            }
            continue;
        }
        char probe_text[640];
        uint8_t target[VERIFY_BLD_POSITION_COUNT];
        if (frame_len == 0) {
            snprintf(probe_text, sizeof probe_text, "%s", token);
        } else {
            snprintf(probe_text, sizeof probe_text, "%s %s", frame, token);
        }
        if (perm_of(probe_text, target) != 0) {
            snprintf(out, out_len, "alg parse failed at %s", token);
            BT_TRACE_FAIL("ev_cfop.rewrite_to_fixed", "target parse %s", token);
            BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
            return 1;
        }
        size_t candidate_count = 0;
        const char *const *candidates = candidates_of(letter, &candidate_count);
        if (candidates == NULL) {
            snprintf(out, out_len, "token %s joins no move class", token);
            BT_TRACE_FAIL("ev_cfop.rewrite_to_fixed", "no class %s", token);
            BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
            return 1;
        }
        char matched[16] = {0};
        for (size_t cand_at = 0; cand_at < candidate_count && matched[0] == '\0'; cand_at++) {
            for (size_t suf_at = 0; suf_at < 4 && matched[0] == '\0'; suf_at++) {
                char fixed[16];
                snprintf(fixed, sizeof fixed, "%s%s", candidates[cand_at], MOVE_SUFFIXES[suf_at]);
                if (frame_len == 0) {
                    snprintf(probe_text, sizeof probe_text, "%s", fixed);
                } else {
                    snprintf(probe_text, sizeof probe_text, "%s %s", fixed, frame);
                }
                uint8_t probe[VERIFY_BLD_POSITION_COUNT];
                if (perm_of(probe_text, probe) == 0 && memcmp(probe, target, sizeof target) == 0) {
                    snprintf(matched, sizeof matched, "%s", fixed);
                }
            }
        }
        if (matched[0] == '\0') {
            snprintf(out, out_len, "token %s conjugates to no in-class move", token);
            BT_TRACE_FAIL("ev_cfop.rewrite_to_fixed", "no conjugate %s", token);
            BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
            return 1;
        }
        if (used + strlen(matched) + 2 >= out_len) {
            snprintf(out, out_len, "rewritten alg overflows the buffer");
            BT_TRACE_FAIL("ev_cfop.rewrite_to_fixed", "overflow");
            BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
            return 1;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, matched, strlen(matched));
        used += strlen(matched);
        out[used] = '\0';
    }
    out[used] = '\0';
    BT_TRACE_EXIT("ev_cfop.rewrite_to_fixed");
    return 0;
}

int verify_cfop_stage_of(const char *stage, verify_cfop_stage *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_cfop.stage_of");
    if (strcmp(stage, "pll") == 0) {
        *out = VERIFY_CFOP_PLL;
    } else if (strcmp(stage, "oll") == 0) {
        *out = VERIFY_CFOP_OLL;
    } else if (strcmp(stage, "f2l") == 0) {
        *out = VERIFY_CFOP_F2L;
    } else {
        snprintf(err, err_len, "stage %s names no cfop step", stage);
        BT_TRACE_FAIL("ev_cfop.stage_of", "stage %s", stage);
        BT_TRACE_EXIT("ev_cfop.stage_of");
        return 1;
    }
    BT_TRACE_EXIT("ev_cfop.stage_of");
    return 0;
}

verify_tier2 verify_cfop_stage_outcome(verify_cfop_stage stage, const bt_cube *state,
                                       const verify_bld_layout *layout, const char *front_json) {
    BT_TRACE_ENTER("ev_cfop.stage_outcome");
    uint8_t band[VERIFY_BLD_POSITION_COUNT];
    size_t band_len = verify_cfop_u_band(layout, band, VERIFY_BLD_POSITION_COUNT);
    if (bt_cube_is_solved(state)) {
        BT_TRACE_EXIT("ev_cfop.stage_outcome");
        return verify_tier2_fail("start state is already solved");
    }
    bt_cube frames[24];
    size_t frame_count = rotated_states(state, frames);
    bool any_f2l = false;
    bool any_u_uniform = false;
    for (size_t at = 0; at < frame_count; at++) {
        if (verify_cfop_f2l_exact(&frames[at], band, band_len)) {
            any_f2l = true;
        }
        if (u_uniform(&frames[at])) {
            any_u_uniform = true;
        }
    }
    verify_tier2 outcome;
    if (stage == VERIFY_CFOP_PLL) {
        if (any_f2l && any_u_uniform) {
            outcome = verify_tier2_pass();
        } else {
            outcome = verify_tier2_fail(
                "no rotation frames a solved first two layers with an oriented last layer");
        }
    } else if (stage == VERIFY_CFOP_OLL) {
        if (any_f2l && !any_u_uniform) {
            outcome = verify_tier2_pass();
        } else {
            outcome = verify_tier2_fail(
                "no rotation frames a solved first two layers with a disoriented last layer");
        }
    } else {
        char err[VERIFY_ERR_MAX];
        if (!cross_solved_on_some_face(state, layout)) {
            outcome = verify_tier2_fail("the state holds no solved cross");
        } else if (verify_cfop_f2l_exact(state, band, band_len)) {
            outcome = verify_tier2_fail("the first two layers are already solved");
        } else {
            int displaced = slot_pair_displaced(front_json, state, layout, err, sizeof err);
            if (displaced < 0) {
                outcome = verify_tier2_fail(err);
            } else if (displaced == 0) {
                outcome = verify_tier2_fail("the named slot holds its pair solved");
            } else {
                outcome = verify_tier2_pass();
            }
        }
    }
    BT_TRACE_EXIT("ev_cfop.stage_outcome");
    return outcome;
}

static void strip_parens(const char *text, char *out, size_t out_len) {
    size_t used = 0;
    for (const char *cursor = text; *cursor != '\0' && used + 1 < out_len; cursor++) {
        if (*cursor == '(' || *cursor == ')') {
            continue;
        }
        out[used++] = *cursor;
    }
    out[used] = '\0';
}

static void verify_cfop(const bt_seed_rows *rows, const char *stage_name,
                        verify_section_result *out) {
    BT_TRACE_ENTER("ev_cfop.verify_cfop");
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    verify_cfop_stage stage;
    char stage_err[VERIFY_ERR_MAX];
    if (verify_cfop_stage_of(stage_name, &stage, stage_err, sizeof stage_err) != 0) {
        verify_section_init(out);
        for (size_t at = 0; at < rows->count; at++) {
            const bt_seed_row *row = &rows->items[at];
            verify_row_outcome outcome = {0};
            snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", stage_err);
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
            verify_section_push_row(out, &outcome);
        }
        BT_TRACE_FAIL("ev_cfop.verify_cfop", "stage %s", stage_name);
        BT_TRACE_EXIT("ev_cfop.verify_cfop");
        return;
    }
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char alg_text[512];
        strip_parens(row->alg, alg_text, sizeof alg_text);
        bool blank = true;
        for (const char *cursor = alg_text; *cursor != '\0'; cursor++) {
            if (*cursor != ' ' && *cursor != '\t') {
                blank = false;
            }
        }
        bt_alg alg;
        bt_alg_init(&alg);
        char token[64];
        char err[VERIFY_ERR_MAX];
        if (blank) {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", "alg is empty");
        } else if (!bt_alg_parse(alg_text, &alg, token, sizeof token)) {
            snprintf(err, sizeof err, "alg parse failed at %s", token);
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
        } else if (verify_bld_round_trip(&alg, err, sizeof err) != 0) {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
        } else {
            outcome.tier1_ok = true;
        }
        if (!outcome.tier1_ok) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
            bt_alg_free(&alg);
            verify_section_push_row(out, &outcome);
            continue;
        }
        char fixed[512];
        if (verify_cfop_rewrite_to_fixed(alg_text, fixed, sizeof fixed) != 0) {
            outcome.tier2 = verify_tier2_fail(fixed);
            bt_alg_free(&alg);
            verify_section_push_row(out, &outcome);
            continue;
        }
        bt_cube state;
        if (fixed[0] == '\0') {
            verify_bld_case_cube(&alg, &state);
        } else {
            bt_alg fixed_alg;
            bt_alg_init(&fixed_alg);
            bt_alg_parse(fixed, &fixed_alg, token, sizeof token);
            verify_bld_case_cube(&fixed_alg, &state);
            bt_alg_free(&fixed_alg);
        }
        outcome.tier2 = verify_cfop_stage_outcome(stage, &state, &layout, row->front_json);
        bt_alg_free(&alg);
        verify_section_push_row(out, &outcome);
    }
    BT_TRACE_EXIT("ev_cfop.verify_cfop");
}

void verify_cfop_verify_pll(const bt_seed_rows *rows, verify_section_result *out) {
    verify_cfop(rows, "pll", out);
}

void verify_cfop_verify_oll(const bt_seed_rows *rows, verify_section_result *out) {
    verify_cfop(rows, "oll", out);
}

void verify_cfop_verify_f2l(const bt_seed_rows *rows, verify_section_result *out) {
    verify_cfop(rows, "f2l", out);
}
