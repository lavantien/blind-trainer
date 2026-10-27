#include "verify/ev_bld.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"

static const char FACE_CHARS[6] = {'U', 'L', 'F', 'R', 'B', 'D'};

void verify_bld_map_wide_suffix(const char *text, char *out, size_t out_len) {
    BT_TRACE_ENTER("ev_bld.map_wide_suffix");
    size_t used = 0;
    const char *cursor = text;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor += 1;
        }
        size_t len = (size_t)(cursor - start);
        if (used > 0 && used + 1 < out_len) {
            out[used++] = ' ';
        }
        if (len >= 2 && strchr("ULFRBD", start[0]) != NULL && start[1] == 'w') {
            if (used + 1 < out_len) {
                out[used++] = (char)((unsigned char)start[0] | 0x20U);
            }
            if (used + (len - 2) < out_len) {
                memcpy(out + used, start + 2, len - 2);
                used += len - 2;
            }
        } else {
            if (used + len < out_len) {
                memcpy(out + used, start, len);
                used += len;
            }
        }
    }
    out[used] = '\0';
    BT_TRACE_EXIT("ev_bld.map_wide_suffix");
}

static uint8_t face_bit(bt_face face) {
    return (uint8_t)(1U << (unsigned)face);
}

static int faces_of(const char *name, uint8_t *mask, char *err, size_t err_len) {
    *mask = 0;
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        const char *found = strchr(FACE_CHARS, *cursor);
        if (found == NULL || *found == '\0') {
            snprintf(err, err_len, "piece name %s holds a non-face letter", name);
            return 1;
        }
        *mask = (uint8_t)(*mask | face_bit((bt_face)(found - FACE_CHARS)));
    }
    return 0;
}

void verify_bld_layout_probe(verify_bld_layout *layout) {
    BT_TRACE_ENTER("ev_bld.layout_probe");
    uint8_t membership[VERIFY_BLD_POSITION_COUNT] = {0};
    for (bt_face face = 0; face < 6; face++) {
        char token[2] = {FACE_CHARS[face], '\0'};
        bt_alg alg;
        bt_alg_init(&alg);
        if (!bt_alg_try_parse(token, &alg)) {
            BT_TRACE_FAIL("ev_bld.layout_probe", "face turn %s parse failed", token);
            continue;
        }
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        bt_alg_free(&alg);
        for (uint8_t sticker = 0; sticker < VERIFY_BLD_POSITION_COUNT; sticker++) {
            if (bt_cube_position_of(&cube, sticker) != sticker) {
                membership[sticker] = (uint8_t)(membership[sticker] | face_bit(face));
            }
        }
    }
    for (int mask = 0; mask < 256; mask++) {
        layout->by_name[mask] = -1;
    }
    layout->piece_count = 0;
    for (int mask = 0; mask < 256; mask++) {
        size_t len = 0;
        for (size_t position = 0; position < VERIFY_BLD_PIECE_BAND_END; position++) {
            if (membership[position] == (uint8_t)mask) {
                len += 1;
            }
        }
        if (len == 0) {
            continue;
        }
        layout->by_name[mask] = (int)layout->piece_count;
        layout->masks[layout->piece_count] = (uint8_t)mask;
        layout->piece_lens[layout->piece_count] = len;
        size_t at = 0;
        for (size_t position = 0; position < VERIFY_BLD_PIECE_BAND_END; position++) {
            if (membership[position] == (uint8_t)mask) {
                layout->pieces[layout->piece_count][at] = (uint8_t)position;
                at += 1;
            }
        }
        layout->piece_count += 1;
    }
    BT_TRACE_EXIT("ev_bld.layout_probe");
}

void verify_bld_mask_name(uint8_t mask, char *out, size_t out_len) {
    size_t used = 0;
    for (bt_face face = 0; face < 6; face++) {
        if ((mask & face_bit(face)) != 0 && used + 1 < out_len) {
            out[used++] = FACE_CHARS[face];
        }
    }
    out[used] = '\0';
}

static size_t name_prefix_len(const char *name) {
    const char *bracket = strchr(name, '[');
    return bracket == NULL ? strlen(name) : (size_t)(bracket - name);
}

static uint8_t popcount8(uint8_t value) {
    uint8_t count = 0;
    while (value != 0) {
        count = (uint8_t)(count + (value & 1U));
        value = (uint8_t)(value >> 1U);
    }
    return count;
}

int verify_bld_slot_index(const verify_bld_layout *layout, const char *name, char *err,
                          size_t err_len) {
    BT_TRACE_ENTER("ev_bld.slot_index");
    char stem[VERIFY_BLD_NAME_MAX];
    size_t len = name_prefix_len(name);
    snprintf(stem, sizeof stem, "%.*s", (int)len, name);
    uint8_t mask = 0;
    if (faces_of(stem, &mask, err, err_len) != 0) {
        BT_TRACE_FAIL("ev_bld.slot_index", "bad name %s", name);
        BT_TRACE_EXIT("ev_bld.slot_index");
        return -1;
    }
    if (popcount8(mask) != strlen(stem)) {
        snprintf(err, err_len, "piece name %s repeats a face", stem);
        BT_TRACE_FAIL("ev_bld.slot_index", "repeat %s", name);
        BT_TRACE_EXIT("ev_bld.slot_index");
        return -1;
    }
    int index = layout->by_name[mask];
    if (index < 0) {
        snprintf(err, err_len, "piece name %s matches no slot", stem);
        BT_TRACE_FAIL("ev_bld.slot_index", "no slot %s", name);
        BT_TRACE_EXIT("ev_bld.slot_index");
        return -1;
    }
    BT_TRACE_EXIT("ev_bld.slot_index");
    return index;
}

int verify_bld_slot(const verify_bld_layout *layout, const char *name, const uint8_t **out,
                    size_t *out_len, char *err, size_t err_len) {
    int index = verify_bld_slot_index(layout, name, err, err_len);
    if (index < 0) {
        return 1;
    }
    *out = layout->pieces[index];
    *out_len = layout->piece_lens[index];
    return 0;
}

static size_t band_of(const verify_bld_layout *layout, uint8_t layers, size_t *out, size_t cap) {
    size_t count = 0;
    for (size_t index = 0; index < layout->piece_count && count < cap; index++) {
        if (popcount8(layout->masks[index]) == layers) {
            out[count] = index;
            count += 1;
        }
    }
    return count;
}

size_t verify_bld_corner_pieces(const verify_bld_layout *layout, size_t *out, size_t cap) {
    return band_of(layout, 3, out, cap);
}

size_t verify_bld_edge_pieces(const verify_bld_layout *layout, size_t *out, size_t cap) {
    return band_of(layout, 2, out, cap);
}

uint8_t verify_bld_mask_of(const verify_bld_layout *layout, size_t index) {
    return layout->masks[index];
}

const uint8_t *verify_bld_slot_of_index(const verify_bld_layout *layout, size_t index) {
    return layout->pieces[index];
}

size_t verify_bld_slot_len_of(const verify_bld_layout *layout, size_t index) {
    return layout->piece_lens[index];
}

static void sort_bytes(uint8_t *items, size_t count) {
    for (size_t at = 1; at < count; at++) {
        uint8_t value = items[at];
        size_t pos = at;
        while (pos > 0 && items[pos - 1] > value) {
            items[pos] = items[pos - 1];
            pos -= 1;
        }
        items[pos] = value;
    }
}

void verify_bld_audit(const bt_cube *state, const verify_bld_layout *layout,
                      verify_bld_state_audit *out) {
    BT_TRACE_ENTER("ev_bld.audit");
    uint8_t home_sets[VERIFY_BLD_MAX_PIECES][VERIFY_BLD_MAX_SLOT];
    for (size_t piece = 0; piece < layout->piece_count; piece++) {
        memcpy(home_sets[piece], layout->pieces[piece], layout->piece_lens[piece]);
        sort_bytes(home_sets[piece], layout->piece_lens[piece]);
    }
    for (size_t piece = 0; piece < layout->piece_count; piece++) {
        uint8_t current[VERIFY_BLD_MAX_SLOT];
        size_t len = layout->piece_lens[piece];
        for (size_t at = 0; at < len; at++) {
            current[at] = bt_cube_sticker_at(state, layout->pieces[piece][at]);
        }
        sort_bytes(current, len);
        size_t match = layout->piece_count;
        for (size_t other = 0; other < layout->piece_count; other++) {
            if (layout->piece_lens[other] == len && memcmp(home_sets[other], current, len) == 0) {
                match = other;
                break;
            }
        }
        if (match == layout->piece_count) {
            BT_TRACE_FAIL("ev_bld.audit", "illegal state at piece %zu", piece);
            exit(1);
        }
        out->piece_at[piece] = match;
    }
    out->disturbed_count = 0;
    for (uint8_t position = 0; position < VERIFY_BLD_POSITION_COUNT; position++) {
        if (bt_cube_sticker_at(state, position) != position) {
            out->disturbed[out->disturbed_count] = position;
            out->disturbed_count += 1;
        }
    }
    BT_TRACE_EXIT("ev_bld.audit");
}

size_t verify_bld_displaced(const verify_bld_state_audit *audit, const size_t *band,
                            size_t band_count, size_t *out, size_t cap) {
    size_t count = 0;
    for (size_t at = 0; at < band_count && count < cap; at++) {
        size_t piece = band[at];
        if (audit->piece_at[piece] != piece) {
            out[count] = piece;
            count += 1;
        }
    }
    return count;
}

int verify_bld_transposed(const verify_bld_state_audit *audit, const verify_bld_layout *layout,
                          const char *first, const char *second, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bld.transposed");
    char err_a[VERIFY_ERR_MAX];
    char err_b[VERIFY_ERR_MAX];
    int a = verify_bld_slot_index(layout, first, err_a, sizeof err_a);
    int b = verify_bld_slot_index(layout, second, err_b, sizeof err_b);
    if (a < 0 || b < 0) {
        snprintf(err, err_len, "%s", a < 0 ? err_a : err_b);
        BT_TRACE_FAIL("ev_bld.transposed", "slot lookup failed");
        BT_TRACE_EXIT("ev_bld.transposed");
        return 1;
    }
    if (audit->piece_at[a] != (size_t)b || audit->piece_at[b] != (size_t)a) {
        snprintf(err, err_len, "%s and %s are not transposed", first, second);
        BT_TRACE_FAIL("ev_bld.transposed", "%s vs %s", first, second);
        BT_TRACE_EXIT("ev_bld.transposed");
        return 1;
    }
    BT_TRACE_EXIT("ev_bld.transposed");
    return 0;
}

bool verify_bld_zero_permutation(const verify_bld_state_audit *audit,
                                 const verify_bld_layout *layout) {
    for (size_t piece = 0; piece < layout->piece_count; piece++) {
        if (audit->piece_at[piece] != piece) {
            return false;
        }
    }
    return true;
}

void verify_bld_case_cube(const bt_alg *alg, bt_cube *out) {
    BT_TRACE_ENTER("ev_bld.case_cube");
    bt_alg inverse = bt_alg_inverse(alg);
    *out = bt_cube_solved();
    bt_cube_apply_alg(out, &inverse);
    bt_alg_free(&inverse);
    BT_TRACE_EXIT("ev_bld.case_cube");
}

int verify_bld_round_trip(const bt_alg *alg, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bld.round_trip");
    bt_alg inverse = bt_alg_inverse(alg);
    bt_cube forward = bt_cube_solved();
    bt_cube_apply_alg(&forward, alg);
    bt_cube_apply_alg(&forward, &inverse);
    if (!bt_cube_is_solved(&forward)) {
        snprintf(err, err_len, "%s", "alg then inverse misses solved");
        bt_alg_free(&inverse);
        BT_TRACE_FAIL("ev_bld.round_trip", "forward miss");
        BT_TRACE_EXIT("ev_bld.round_trip");
        return 1;
    }
    bt_cube reverse = bt_cube_solved();
    bt_cube_apply_alg(&reverse, &inverse);
    bt_cube_apply_alg(&reverse, alg);
    bt_alg_free(&inverse);
    if (!bt_cube_is_solved(&reverse)) {
        snprintf(err, err_len, "%s", "inverse then alg misses solved");
        BT_TRACE_FAIL("ev_bld.round_trip", "reverse miss");
        BT_TRACE_EXIT("ev_bld.round_trip");
        return 1;
    }
    BT_TRACE_EXIT("ev_bld.round_trip");
    return 0;
}

static char *trim_span(char *text, size_t *len) {
    char *start = text;
    size_t length = *len;
    while (length > 0 && (*start == ' ' || *start == '\t')) {
        start += 1;
        length -= 1;
    }
    while (length > 0 && (start[length - 1] == ' ' || start[length - 1] == '\t')) {
        length -= 1;
    }
    *len = length;
    return start;
}

int verify_bld_parse_pair(const char *spec, char sep, char out[2][VERIFY_BLD_NAME_MAX], char *err,
                          size_t err_len) {
    BT_TRACE_ENTER("ev_bld.parse_pair");
    char copy[128];
    snprintf(copy, sizeof copy, "%s", spec);
    char *parts[3];
    size_t count = 0;
    char *cursor = copy;
    parts[count] = cursor;
    count += 1;
    while (*cursor != '\0' && count < 3) {
        if (*cursor == sep) {
            *cursor = '\0';
            parts[count] = cursor + 1;
            count += 1;
        }
        cursor += 1;
    }
    if (count != 2) {
        snprintf(err, err_len, "pair spec %s does not hold two names", spec);
        BT_TRACE_FAIL("ev_bld.parse_pair", "count %zu for %s", count, spec);
        BT_TRACE_EXIT("ev_bld.parse_pair");
        return 1;
    }
    for (size_t at = 0; at < 2; at++) {
        size_t len = strlen(parts[at]);
        char *trimmed = trim_span(parts[at], &len);
        snprintf(out[at], VERIFY_BLD_NAME_MAX, "%.*s", (int)len, trimmed);
    }
    BT_TRACE_EXIT("ev_bld.parse_pair");
    return 0;
}

int verify_bld_parity_swaps(const char *swaps, char edge[2][VERIFY_BLD_NAME_MAX],
                            char corner[2][VERIFY_BLD_NAME_MAX], char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bld.parity_swaps");
    char copy[128];
    snprintf(copy, sizeof copy, "%s", swaps);
    char *parts[3];
    size_t count = 0;
    char *cursor = copy;
    parts[count] = cursor;
    count += 1;
    while (*cursor != '\0' && count < 3) {
        if (*cursor == '+') {
            *cursor = '\0';
            parts[count] = cursor + 1;
            count += 1;
        }
        cursor += 1;
    }
    if (count != 2) {
        snprintf(err, err_len, "parity spec %s does not hold two pairs", swaps);
        BT_TRACE_FAIL("ev_bld.parity_swaps", "count %zu for %s", count, swaps);
        BT_TRACE_EXIT("ev_bld.parity_swaps");
        return 1;
    }
    if (verify_bld_parse_pair(parts[0], '-', edge, err, err_len) != 0) {
        BT_TRACE_EXIT("ev_bld.parity_swaps");
        return 1;
    }
    if (verify_bld_parse_pair(parts[1], '-', corner, err, err_len) != 0) {
        BT_TRACE_EXIT("ev_bld.parity_swaps");
        return 1;
    }
    BT_TRACE_EXIT("ev_bld.parity_swaps");
    return 0;
}

static bt_json_value *parse_fields(const char *front_json, char *err, size_t err_len) {
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        snprintf(err, err_len, "%s", json_err.message);
        return NULL;
    }
    return value;
}

static bool text_field(const bt_json_value *fields, const char *key, char *out, size_t out_len) {
    const bt_json_value *field = bt_json_get(fields, key);
    if (field == NULL || field->type != BT_JSON_STRING) {
        return false;
    }
    snprintf(out, out_len, "%s", field->as.string);
    return true;
}

static int confined(const verify_bld_state_audit *audit, const verify_bld_layout *layout,
                    const char *const *names, size_t name_count, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bld.confined");
    uint8_t allowed[VERIFY_BLD_POSITION_COUNT] = {0};
    for (size_t at = 0; at < name_count; at++) {
        const uint8_t *slot = NULL;
        size_t slot_len = 0;
        char slot_err[VERIFY_ERR_MAX];
        if (verify_bld_slot(layout, names[at], &slot, &slot_len, slot_err, sizeof slot_err) != 0) {
            snprintf(err, err_len, "%s", slot_err);
            BT_TRACE_FAIL("ev_bld.confined", "slot %s failed", names[at]);
            BT_TRACE_EXIT("ev_bld.confined");
            return 1;
        }
        for (size_t pos_at = 0; pos_at < slot_len; pos_at++) {
            allowed[slot[pos_at]] = 1;
        }
    }
    for (size_t at = 0; at < audit->disturbed_count; at++) {
        if (!allowed[audit->disturbed[at]]) {
            snprintf(err, err_len, "sticker at position %u moved outside the named slots",
                     audit->disturbed[at]);
            BT_TRACE_FAIL("ev_bld.confined", "position %u outside", audit->disturbed[at]);
            BT_TRACE_EXIT("ev_bld.confined");
            return 1;
        }
    }
    BT_TRACE_EXIT("ev_bld.confined");
    return 0;
}

static int others_home(const verify_bld_state_audit *audit, const verify_bld_layout *layout,
                       const size_t *band, size_t band_count, const size_t *skip, size_t skip_count,
                       char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bld.others_home");
    for (size_t at = 0; at < band_count; at++) {
        size_t piece = band[at];
        bool skipped = false;
        for (size_t skip_at = 0; skip_at < skip_count; skip_at++) {
            if (skip[skip_at] == piece) {
                skipped = true;
                break;
            }
        }
        if (skipped) {
            continue;
        }
        if (audit->piece_at[piece] != piece) {
            char name[VERIFY_BLD_NAME_MAX];
            verify_bld_mask_name(layout->masks[piece], name, sizeof name);
            snprintf(err, err_len, "piece %s left its slot", name);
            BT_TRACE_FAIL("ev_bld.others_home", "piece %zu left", piece);
            BT_TRACE_EXIT("ev_bld.others_home");
            return 1;
        }
    }
    BT_TRACE_EXIT("ev_bld.others_home");
    return 0;
}

static int double_transposition(const verify_bld_state_audit *audit,
                                const verify_bld_layout *layout,
                                const char edge[2][VERIFY_BLD_NAME_MAX],
                                const char corner[2][VERIFY_BLD_NAME_MAX], char *err,
                                size_t err_len) {
    BT_TRACE_ENTER("ev_bld.double_transposition");
    if (verify_bld_transposed(audit, layout, edge[0], edge[1], err, err_len) != 0) {
        BT_TRACE_EXIT("ev_bld.double_transposition");
        return 1;
    }
    if (verify_bld_transposed(audit, layout, corner[0], corner[1], err, err_len) != 0) {
        BT_TRACE_EXIT("ev_bld.double_transposition");
        return 1;
    }
    size_t edges[VERIFY_BLD_MAX_BAND];
    size_t corners[VERIFY_BLD_MAX_BAND];
    size_t edge_count = verify_bld_edge_pieces(layout, edges, VERIFY_BLD_MAX_BAND);
    size_t corner_count = verify_bld_corner_pieces(layout, corners, VERIFY_BLD_MAX_BAND);
    size_t band[2 * VERIFY_BLD_MAX_BAND];
    memcpy(band, edges, edge_count * sizeof *edges);
    memcpy(band + edge_count, corners, corner_count * sizeof *corners);
    int slots[4];
    slots[0] = verify_bld_slot_index(layout, edge[0], err, err_len);
    slots[1] = verify_bld_slot_index(layout, edge[1], err, err_len);
    slots[2] = verify_bld_slot_index(layout, corner[0], err, err_len);
    slots[3] = verify_bld_slot_index(layout, corner[1], err, err_len);
    for (size_t at = 0; at < 4; at++) {
        if (slots[at] < 0) {
            BT_TRACE_FAIL("ev_bld.double_transposition", "slot lookup failed");
            BT_TRACE_EXIT("ev_bld.double_transposition");
            return 1;
        }
    }
    size_t skip[4];
    for (size_t at = 0; at < 4; at++) {
        skip[at] = (size_t)slots[at];
    }
    int rc = others_home(audit, layout, band, edge_count + corner_count, skip, 4, err, err_len);
    BT_TRACE_EXIT("ev_bld.double_transposition");
    return rc;
}

static int split_plus(const char *text, char out[8][VERIFY_BLD_NAME_MAX], size_t *out_count,
                      char *err, size_t err_len) {
    char copy[160];
    snprintf(copy, sizeof copy, "%s", text);
    size_t count = 0;
    char *start = copy;
    for (char *cursor = copy;; cursor++) {
        if (*cursor != '+' && *cursor != '\0') {
            continue;
        }
        bool end = *cursor == '\0';
        *cursor = '\0';
        if (count >= 8) {
            snprintf(err, err_len, "spec %s holds too many names", text);
            return 1;
        }
        snprintf(out[count], VERIFY_BLD_NAME_MAX, "%s", start);
        count += 1;
        if (end) {
            break;
        }
        start = cursor + 1;
    }
    *out_count = count;
    return 0;
}

static int oldpoch_case(const char *front_json, const verify_bld_state_audit *audit,
                        const verify_bld_layout *layout, const bt_cube *state, char *err,
                        size_t err_len) {
    BT_TRACE_ENTER("ev_bld.oldpoch_case");
    bt_json_value *fields = parse_fields(front_json, err, err_len);
    if (fields == NULL) {
        BT_TRACE_FAIL("ev_bld.oldpoch_case", "json failed");
        BT_TRACE_EXIT("ev_bld.oldpoch_case");
        return 1;
    }
    char kind[VERIFY_BLD_KIND_MAX];
    char swaps[128];
    bool has_kind = text_field(fields, "kind", kind, sizeof kind);
    bool has_swaps = text_field(fields, "swaps", swaps, sizeof swaps);
    if (has_kind && strcmp(kind, "parity") == 0 && has_swaps) {
        char edge[2][VERIFY_BLD_NAME_MAX];
        char corner[2][VERIFY_BLD_NAME_MAX];
        int rc = 1;
        if (verify_bld_parity_swaps(swaps, edge, corner, err, err_len) == 0) {
            rc = double_transposition(audit, layout, edge, corner, err, err_len);
        }
        bt_json_free(fields);
        BT_TRACE_EXIT("ev_bld.oldpoch_case");
        return rc;
    }
    int rc = 0;
    if (has_kind && strcmp(kind, "parity") == 0) {
        size_t edges[VERIFY_BLD_MAX_BAND];
        size_t corners[VERIFY_BLD_MAX_BAND];
        size_t edge_count = verify_bld_edge_pieces(layout, edges, VERIFY_BLD_MAX_BAND);
        size_t corner_count = verify_bld_corner_pieces(layout, corners, VERIFY_BLD_MAX_BAND);
        size_t displaced_edges[VERIFY_BLD_MAX_BAND];
        size_t displaced_corners[VERIFY_BLD_MAX_BAND];
        size_t edge_displaced =
            verify_bld_displaced(audit, edges, edge_count, displaced_edges, VERIFY_BLD_MAX_BAND);
        size_t corner_displaced = verify_bld_displaced(audit, corners, corner_count,
                                                       displaced_corners, VERIFY_BLD_MAX_BAND);
        if (edge_displaced != 2 || corner_displaced != 2) {
            snprintf(err, err_len,
                     "parity case displaces %zu edges and %zu corners, expected 2 and 2",
                     edge_displaced, corner_displaced);
            rc = 1;
        }
    } else if (has_kind && strcmp(kind, "corner twist") == 0) {
        char twists[128];
        if (!text_field(fields, "twists", twists, sizeof twists)) {
            snprintf(err, err_len, "%s", "corner twist row names no twists");
            rc = 1;
        } else {
            char names[8][VERIFY_BLD_NAME_MAX];
            size_t name_count = 0;
            if (split_plus(twists, names, &name_count, err, err_len) == 0) {
                const char *name_ptrs[8];
                for (size_t at = 0; at < name_count; at++) {
                    name_ptrs[at] = names[at];
                }
                rc = confined(audit, layout, name_ptrs, name_count, err, err_len);
                if (rc == 0 && !verify_bld_zero_permutation(audit, layout)) {
                    snprintf(err, err_len, "%s", "corner twist row permutes pieces");
                    rc = 1;
                }
                if (rc == 0 && bt_cube_is_solved(state)) {
                    snprintf(err, err_len, "%s", "twist row leaves the cube solved");
                    rc = 1;
                }
            } else {
                rc = 1;
            }
        }
    } else if (has_kind && (strcmp(kind, "edge swap") == 0 || strcmp(kind, "corner swap") == 0 ||
                            strcmp(kind, "setup example") == 0)) {
        char buffer[VERIFY_BLD_NAME_MAX];
        char target[VERIFY_BLD_NAME_MAX];
        if (!text_field(fields, "buffer", buffer, sizeof buffer)) {
            snprintf(err, err_len, "%s", "swap row names no buffer");
            rc = 1;
        } else if (!text_field(fields, "target", target, sizeof target)) {
            snprintf(err, err_len, "%s", "swap row names no target");
            rc = 1;
        } else {
            rc = verify_bld_transposed(audit, layout, buffer, target, err, err_len);
            if (rc == 0) {
                size_t band[VERIFY_BLD_MAX_BAND];
                size_t band_count;
                if (strlen(buffer) == 2) {
                    band_count = verify_bld_edge_pieces(layout, band, VERIFY_BLD_MAX_BAND);
                } else {
                    band_count = verify_bld_corner_pieces(layout, band, VERIFY_BLD_MAX_BAND);
                }
                int slot_a = verify_bld_slot_index(layout, buffer, err, err_len);
                int slot_b = verify_bld_slot_index(layout, target, err, err_len);
                if (slot_a < 0 || slot_b < 0) {
                    rc = 1;
                } else {
                    size_t skip[2] = {(size_t)slot_a, (size_t)slot_b};
                    rc = others_home(audit, layout, band, band_count, skip, 2, err, err_len);
                }
            }
        }
    } else if (has_kind) {
        snprintf(err, err_len, "row kind %s has no predicate", kind);
        rc = 1;
    } else {
        char letter[VERIFY_BLD_NAME_MAX];
        if (text_field(fields, "letter", letter, sizeof letter)) {
            snprintf(err, err_len, "%s", "letter scheme rows have no state predicate");
        } else {
            snprintf(err, err_len, "%s", "row names neither kind nor letter");
        }
        rc = 1;
    }
    bt_json_free(fields);
    BT_TRACE_EXIT("ev_bld.oldpoch_case");
    return rc;
}

static int m2_case(const char *front_json, const verify_bld_state_audit *audit,
                   const verify_bld_layout *layout, const bt_cube *state, char *err,
                   size_t err_len) {
    BT_TRACE_ENTER("ev_bld.m2_case");
    bt_json_value *fields = parse_fields(front_json, err, err_len);
    if (fields == NULL) {
        BT_TRACE_FAIL("ev_bld.m2_case", "json failed");
        BT_TRACE_EXIT("ev_bld.m2_case");
        return 1;
    }
    char kind[VERIFY_BLD_KIND_MAX];
    bool has_kind = text_field(fields, "kind", kind, sizeof kind);
    size_t edges[VERIFY_BLD_MAX_BAND];
    size_t corners[VERIFY_BLD_MAX_BAND];
    size_t edge_count = verify_bld_edge_pieces(layout, edges, VERIFY_BLD_MAX_BAND);
    size_t corner_count = verify_bld_corner_pieces(layout, corners, VERIFY_BLD_MAX_BAND);
    int rc = 0;
    if (has_kind && strcmp(kind, "parity") == 0) {
        size_t displaced_edges[VERIFY_BLD_MAX_BAND];
        size_t displaced_corners[VERIFY_BLD_MAX_BAND];
        size_t edge_displaced =
            verify_bld_displaced(audit, edges, edge_count, displaced_edges, VERIFY_BLD_MAX_BAND);
        size_t corner_displaced = verify_bld_displaced(audit, corners, corner_count,
                                                       displaced_corners, VERIFY_BLD_MAX_BAND);
        if ((edge_displaced != 2 && edge_displaced != 4) || corner_displaced != 0) {
            snprintf(err, err_len,
                     "m2 parity case displaces %zu edges and %zu corners, expected 2 or 4 edges "
                     "and no corners",
                     edge_displaced, corner_displaced);
            rc = 1;
        }
    } else if (has_kind && strcmp(kind, "buffer edge flip") == 0) {
        char flips[128];
        if (!text_field(fields, "flips", flips, sizeof flips)) {
            snprintf(err, err_len, "%s", "flip row names no flips");
            rc = 1;
        } else {
            char names[8][VERIFY_BLD_NAME_MAX];
            size_t name_count = 0;
            if (split_plus(flips, names, &name_count, err, err_len) == 0) {
                const char *name_ptrs[8];
                for (size_t at = 0; at < name_count; at++) {
                    name_ptrs[at] = names[at];
                }
                rc = confined(audit, layout, name_ptrs, name_count, err, err_len);
                if (rc == 0 && !verify_bld_zero_permutation(audit, layout)) {
                    snprintf(err, err_len, "%s", "flip row permutes pieces");
                    rc = 1;
                }
                if (rc == 0 && bt_cube_is_solved(state)) {
                    snprintf(err, err_len, "%s", "flip row leaves the cube solved");
                    rc = 1;
                }
            } else {
                rc = 1;
            }
        }
    } else {
        char buffer[VERIFY_BLD_NAME_MAX];
        char target[VERIFY_BLD_NAME_MAX];
        if (!text_field(fields, "buffer", buffer, sizeof buffer)) {
            snprintf(err, err_len, "%s", "m2 row names no buffer");
            rc = 1;
        } else if (!text_field(fields, "target", target, sizeof target)) {
            snprintf(err, err_len, "%s", "m2 row names no target");
            rc = 1;
        } else {
            int buffer_slot = verify_bld_slot_index(layout, buffer, err, err_len);
            int target_slot = verify_bld_slot_index(layout, target, err, err_len);
            if (buffer_slot < 0 || target_slot < 0) {
                rc = 1;
            } else if (audit->piece_at[buffer_slot] != (size_t)target_slot) {
                snprintf(err, err_len, "%s does not land in the %s buffer", target, buffer);
                rc = 1;
            }
            if (rc == 0) {
                size_t displaced_edges[VERIFY_BLD_MAX_BAND];
                size_t edge_displaced = verify_bld_displaced(audit, edges, edge_count,
                                                             displaced_edges, VERIFY_BLD_MAX_BAND);
                if (edge_displaced > 4) {
                    snprintf(err, err_len, "m2 execution displaces %zu edges, expected at most 4",
                             edge_displaced);
                    rc = 1;
                }
            }
        }
    }
    bt_json_free(fields);
    BT_TRACE_EXIT("ev_bld.m2_case");
    return rc;
}

static int style_case(const char *front_json, const verify_bld_state_audit *audit,
                      const verify_bld_layout *layout, const bt_cube *state, char *err,
                      size_t err_len) {
    BT_TRACE_ENTER("ev_bld.style_case");
    bt_json_value *fields = parse_fields(front_json, err, err_len);
    if (fields == NULL) {
        BT_TRACE_FAIL("ev_bld.style_case", "json failed");
        BT_TRACE_EXIT("ev_bld.style_case");
        return 1;
    }
    char position[128];
    int rc = 0;
    if (!text_field(fields, "position", position, sizeof position)) {
        snprintf(err, err_len, "%s", "style row names no position");
        rc = 1;
    } else if (strstr(position, " & ") != NULL) {
        char pair[2][VERIFY_BLD_NAME_MAX];
        if (verify_bld_parse_pair(position, '&', pair, err, err_len) != 0) {
            rc = 1;
        } else {
            const char *names[2] = {pair[0], pair[1]};
            rc = confined(audit, layout, names, 2, err, err_len);
            if (rc == 0 && !verify_bld_zero_permutation(audit, layout)) {
                snprintf(err, err_len, "%s row permutes pieces", position);
                rc = 1;
            }
            if (rc == 0 && bt_cube_is_solved(state)) {
                snprintf(err, err_len, "%s row leaves the cube solved", position);
                rc = 1;
            }
        }
    } else {
        char copy[128];
        snprintf(copy, sizeof copy, "%s", position);
        size_t count = 0;
        char *parts[3];
        char *cursor = copy;
        parts[count] = cursor;
        count += 1;
        while (*cursor != '\0' && count < 3) {
            if (*cursor == ',') {
                *cursor = '\0';
                parts[count] = cursor + 1;
                count += 1;
            }
            cursor += 1;
        }
        if (count == 2) {
            char edge[2][VERIFY_BLD_NAME_MAX];
            char corner[2][VERIFY_BLD_NAME_MAX];
            bool edge_ok = (bool)(verify_bld_parse_pair(parts[0], '-', edge, err, err_len) == 0);
            bool corner_ok =
                (bool)(edge_ok && verify_bld_parse_pair(parts[1], '-', corner, err, err_len) == 0);
            if (!edge_ok || !corner_ok) {
                rc = 1;
            } else {
                rc = double_transposition(audit, layout, edge, corner, err, err_len);
            }
        } else {
            snprintf(err, err_len, "position %s names neither a pair nor two swaps", position);
            rc = 1;
        }
    }
    bt_json_free(fields);
    BT_TRACE_EXIT("ev_bld.style_case");
    return rc;
}

static int parse_mapped(const char *alg, char *mapped, size_t mapped_len, bt_alg *out, char *err,
                        size_t err_len) {
    bt_alg_init(out);
    verify_bld_map_wide_suffix(alg, mapped, mapped_len);
    char token[64];
    if (!bt_alg_parse(mapped, out, token, sizeof token)) {
        snprintf(err, err_len, "unknown token: %s", token);
        return 1;
    }
    return 0;
}

static void tier_from_alg(const bt_alg *alg, verify_row_outcome *outcome) {
    char err[VERIFY_ERR_MAX];
    if (verify_bld_round_trip(alg, err, sizeof err) == 0) {
        outcome->tier1_ok = true;
    } else {
        snprintf(outcome->tier1_err, sizeof outcome->tier1_err, "%s", err);
    }
}

static bool row_is_letter(const char *front_json) {
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        return false;
    }
    const bt_json_value *letter = bt_json_get(value, "letter");
    bool present = (bool)(letter != NULL && letter->type == BT_JSON_STRING);
    bt_json_free(value);
    return present;
}

void verify_bld_verify_oldpoch(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_bld.verify_oldpoch");
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char mapped[512];
        if (row->alg[0] == '\0') {
            char note[VERIFY_HONESTY_MAX];
            snprintf(note, sizeof note, "%s holds a prose alg, no mechanical verification exists",
                     row->case_key);
            verify_section_push_honesty(out, note);
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", VERIFY_PROSE_ALG);
        } else {
            bt_alg alg;
            bt_alg_init(&alg);
            char err[VERIFY_ERR_MAX];
            if (parse_mapped(row->alg, mapped, sizeof mapped, &alg, err, sizeof err) == 0) {
                tier_from_alg(&alg, &outcome);
                bt_alg_free(&alg);
            } else {
                snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
            }
        }
        bool is_letter = row_is_letter(row->front_json);
        if (is_letter) {
            char note[VERIFY_HONESTY_MAX];
            snprintf(note, sizeof note,
                     "%s is a letter scheme setup row, the letter to piece map is scheme knowledge",
                     row->case_key);
            verify_section_push_honesty(out, note);
        }
        if (!outcome.tier1_ok) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else if (is_letter) {
            outcome.tier2 = verify_tier2_skipped("letter scheme rows have no predicate");
        } else {
            bt_alg alg;
            bt_alg_init(&alg);
            char err[VERIFY_ERR_MAX];
            parse_mapped(row->alg, mapped, sizeof mapped, &alg, err, sizeof err);
            bt_cube state;
            verify_bld_case_cube(&alg, &state);
            verify_bld_state_audit audit;
            verify_bld_audit(&state, &layout, &audit);
            char case_err[VERIFY_ERR_MAX];
            if (oldpoch_case(row->front_json, &audit, &layout, &state, case_err, sizeof case_err) ==
                0) {
                outcome.tier2 = verify_tier2_pass();
            } else {
                outcome.tier2 = verify_tier2_fail(case_err);
            }
            bt_alg_free(&alg);
        }
        verify_section_push_row(out, &outcome);
    }
    BT_TRACE_EXIT("ev_bld.verify_oldpoch");
}

void verify_bld_verify_m2(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_bld.verify_m2");
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char mapped[512];
        bt_alg alg;
        bt_alg_init(&alg);
        char err[VERIFY_ERR_MAX];
        if (parse_mapped(row->alg, mapped, sizeof mapped, &alg, err, sizeof err) != 0) {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
        } else {
            tier_from_alg(&alg, &outcome);
        }
        if (!outcome.tier1_ok) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else {
            bt_cube state;
            verify_bld_case_cube(&alg, &state);
            verify_bld_state_audit audit;
            verify_bld_audit(&state, &layout, &audit);
            char case_err[VERIFY_ERR_MAX];
            if (m2_case(row->front_json, &audit, &layout, &state, case_err, sizeof case_err) == 0) {
                outcome.tier2 = verify_tier2_pass();
            } else {
                outcome.tier2 = verify_tier2_fail(case_err);
            }
        }
        bt_alg_free(&alg);
        verify_section_push_row(out, &outcome);
    }
    BT_TRACE_EXIT("ev_bld.verify_m2");
}

void verify_bld_verify_3style(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_bld.verify_3style");
    verify_bld_layout layout;
    verify_bld_layout_probe(&layout);
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        char mapped[512];
        bt_alg alg;
        bt_alg_init(&alg);
        char err[VERIFY_ERR_MAX];
        if (parse_mapped(row->alg, mapped, sizeof mapped, &alg, err, sizeof err) != 0) {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
        } else {
            tier_from_alg(&alg, &outcome);
        }
        if (!outcome.tier1_ok) {
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else {
            bt_cube state;
            verify_bld_case_cube(&alg, &state);
            verify_bld_state_audit audit;
            verify_bld_audit(&state, &layout, &audit);
            char case_err[VERIFY_ERR_MAX];
            if (style_case(row->front_json, &audit, &layout, &state, case_err, sizeof case_err) ==
                0) {
                outcome.tier2 = verify_tier2_pass();
            } else {
                outcome.tier2 = verify_tier2_fail(case_err);
            }
        }
        bt_alg_free(&alg);
        verify_section_push_row(out, &outcome);
    }
    BT_TRACE_EXIT("ev_bld.verify_3style");
}
