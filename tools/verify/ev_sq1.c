#include "verify/ev_sq1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "verify/lib.h"

static const uint8_t TOP_CORNERS[4] = {0, 2, 4, 6};
static const uint8_t BOTTOM_CORNERS[4] = {9, 11, 13, 15};
static const uint8_t TOP_EDGES[4] = {1, 3, 5, 7};
static const uint8_t BOTTOM_EDGES[4] = {8, 10, 12, 14};

enum { VERIFY_SQ1_DEBUG_MAX = 64 };

int verify_sq1_start_case(const char *alg_text, verify_sq1_case *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_sq1.start_case");
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_sq1_engine();
    if (alg == NULL || !eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_sq1.start_case", "parse failed %s", alg_text);
        BT_TRACE_EXIT("ev_sq1.start_case");
        return 1;
    }
    if (bt_pz_alg_is_empty(alg)) {
        snprintf(err, err_len, "%s", "alg is empty");
        bt_pz_alg_destroy(alg);
        BT_TRACE_FAIL("ev_sq1.start_case", "empty alg");
        BT_TRACE_EXIT("ev_sq1.start_case");
        return 1;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    if (!bt_pz_sq1_apply_alg(&state, inverse, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, err_len, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
        BT_TRACE_FAIL("ev_sq1.start_case", "inverse failed %s", alg_text);
        BT_TRACE_EXIT("ev_sq1.start_case");
        return 1;
    }
    memset(out, 0, sizeof *out);
    out->state = state;
    out->class_lens[0] = bt_pz_sq1_canonical_shape(&state, 0, out->classes[0], BT_PZ_SQ1_UNITS);
    out->class_lens[1] = bt_pz_sq1_canonical_shape(&state, 1, out->classes[1], BT_PZ_SQ1_UNITS);
    out->front_json = "";
    if (!bt_pz_sq1_apply_alg(&state, alg, &pz_err) || !bt_pz_sq1_is_solved(&state)) {
        snprintf(err, err_len, "%s", "start state round trip misses solved");
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
        BT_TRACE_FAIL("ev_sq1.start_case", "round trip failed %s", alg_text);
        BT_TRACE_EXIT("ev_sq1.start_case");
        return 1;
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    BT_TRACE_EXIT("ev_sq1.start_case");
    return 0;
}

void verify_sq1_mirrored(const uint8_t *widths, size_t count, uint8_t *out) {
    BT_TRACE_ENTER("ev_sq1.mirrored");
    uint8_t reversed[BT_PZ_SQ1_UNITS];
    for (size_t at = 0; at < count; at++) {
        reversed[at] = widths[count - 1 - at];
    }
    uint8_t best[BT_PZ_SQ1_UNITS];
    memcpy(best, reversed, count);
    for (size_t rotation = 1; rotation < count; rotation++) {
        uint8_t turned[BT_PZ_SQ1_UNITS];
        for (size_t unit = 0; unit < count; unit++) {
            turned[unit] = reversed[(rotation + unit) % count];
        }
        if (memcmp(turned, best, count) < 0) {
            memcpy(best, turned, count);
        }
    }
    memcpy(out, best, count);
    BT_TRACE_EXIT("ev_sq1.mirrored");
}

void verify_sq1_famkey(const uint8_t *widths, size_t count, uint8_t *out) {
    BT_TRACE_ENTER("ev_sq1.famkey");
    uint8_t mirror[BT_PZ_SQ1_UNITS];
    verify_sq1_mirrored(widths, count, mirror);
    if (memcmp(mirror, widths, count) < 0) {
        memcpy(out, mirror, count);
    } else {
        memcpy(out, widths, count);
    }
    BT_TRACE_EXIT("ev_sq1.famkey");
}

int verify_sq1_shape_names(const char *front_json, char *top, char *bottom, size_t cap, char *err,
                           size_t err_len) {
    BT_TRACE_ENTER("ev_sq1.shape_names");
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        snprintf(err, err_len, "%s", json_err.message);
        BT_TRACE_FAIL("ev_sq1.shape_names", "json parse failed");
        BT_TRACE_EXIT("ev_sq1.shape_names");
        return 1;
    }
    bt_json_value *field = bt_json_get(value, "case");
    bool ok = (bool)(field != NULL && field->type == BT_JSON_STRING);
    if (!ok) {
        bt_json_free(value);
        snprintf(err, err_len, "%s", "front_json holds no case name");
        BT_TRACE_FAIL("ev_sq1.shape_names", "no case name");
        BT_TRACE_EXIT("ev_sq1.shape_names");
        return 1;
    }
    const char *case_name = field->as.string;
    const char *sep = strstr(case_name, " / ");
    if (sep == NULL) {
        snprintf(err, err_len, "case name %s does not name two layers", case_name);
        bt_json_free(value);
        BT_TRACE_FAIL("ev_sq1.shape_names", "no pair %s", case_name);
        BT_TRACE_EXIT("ev_sq1.shape_names");
        return 1;
    }
    snprintf(top, cap, "%.*s", (int)(sep - case_name), case_name);
    snprintf(bottom, cap, "%s", sep + 3);
    bt_json_free(value);
    BT_TRACE_EXIT("ev_sq1.shape_names");
    return 0;
}

int verify_sq1_class_invariants(const verify_sq1_case *case_, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_sq1.class_invariants");
    for (size_t layer = 0; layer < 2; layer++) {
        unsigned sum = 0;
        for (size_t at = 0; at < case_->class_lens[layer]; at++) {
            sum += case_->classes[layer][at];
        }
        if (sum != 12) {
            snprintf(err, err_len, "layer %zu ring sums to %u, not 12", layer, sum);
            BT_TRACE_FAIL("ev_sq1.class_invariants", "layer %zu sums %u", layer, sum);
            BT_TRACE_EXIT("ev_sq1.class_invariants");
            return 1;
        }
    }
    size_t corners = 0;
    for (size_t layer = 0; layer < 2; layer++) {
        for (size_t at = 0; at < case_->class_lens[layer]; at++) {
            if (case_->classes[layer][at] == 2) {
                corners += 1;
            }
        }
    }
    if (corners != 8) {
        snprintf(err, err_len, "state holds %zu corners, not 8", corners);
        BT_TRACE_FAIL("ev_sq1.class_invariants", "%zu corners", corners);
        BT_TRACE_EXIT("ev_sq1.class_invariants");
        return 1;
    }
    BT_TRACE_EXIT("ev_sq1.class_invariants");
    return 0;
}

typedef struct {
    char name[VERIFY_SQ1_NAME_MAX];
    uint8_t key[BT_PZ_SQ1_UNITS];
    size_t key_len;
    size_t count;
} verify_sq1_tally_row;

static void tally_bump(const verify_sq1_case *case_, verify_sq1_tally_row **rows, size_t *row_count,
                       size_t *row_cap) {
    char top[VERIFY_SQ1_NAME_MAX];
    char bottom[VERIFY_SQ1_NAME_MAX];
    char err[VERIFY_ERR_MAX];
    if (verify_sq1_shape_names(case_->front_json, top, bottom, VERIFY_SQ1_NAME_MAX, err,
                               sizeof err) != 0) {
        return;
    }
    const char *names[2] = {top, bottom};
    for (size_t at = 0; at < 2; at++) {
        uint8_t key[BT_PZ_SQ1_UNITS];
        verify_sq1_famkey(case_->classes[at], case_->class_lens[at], key);
        size_t len = case_->class_lens[at];
        bool found = false;
        for (size_t row_at = 0; row_at < *row_count; row_at++) {
            verify_sq1_tally_row *row = &(*rows)[row_at];
            if (strcmp(row->name, names[at]) == 0 && row->key_len == len &&
                memcmp(row->key, key, len) == 0) {
                row->count += 1;
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }
        if (*row_count == *row_cap) {
            size_t cap = *row_cap == 0 ? 16 : *row_cap * 2;
            verify_sq1_tally_row *grown = realloc(*rows, cap * sizeof *grown);
            if (grown == NULL) {
                BT_TRACE_FAIL("ev_sq1.tally_bump", "oom at %zu", *row_count);
                return;
            }
            *rows = grown;
            *row_cap = cap;
        }
        verify_sq1_tally_row *row = &(*rows)[*row_count];
        memset(row, 0, sizeof *row);
        snprintf(row->name, sizeof row->name, "%s", names[at]);
        memcpy(row->key, key, len);
        row->key_len = len;
        row->count = 1;
        *row_count += 1;
    }
}

void verify_sq1_resolve_family_keys(const verify_sq1_case *const *cases, size_t count,
                                    verify_sq1_family_index *out) {
    BT_TRACE_ENTER("ev_sq1.resolve_family_keys");
    out->keys = NULL;
    out->key_count = 0;
    out->ambiguous = NULL;
    out->ambiguous_count = 0;
    verify_sq1_tally_row *rows = NULL;
    size_t row_count = 0;
    size_t row_cap = 0;
    size_t key_cap = 0;
    size_t ambiguous_cap = 0;
    for (size_t at = 0; at < count; at++) {
        tally_bump(cases[at], &rows, &row_count, &row_cap);
    }
    bool *judged = calloc(row_count, sizeof *judged);
    for (size_t row_at = 0; row_at < row_count; row_at++) {
        if (judged[row_at]) {
            continue;
        }
        judged[row_at] = true;
        const verify_sq1_tally_row *row = &rows[row_at];
        size_t best = row->count;
        size_t winners = 1;
        for (size_t other = row_at + 1; other < row_count; other++) {
            if (judged[other] || strcmp(rows[other].name, row->name) != 0) {
                continue;
            }
            judged[other] = true;
            if (rows[other].count > best) {
                best = rows[other].count;
                winners = 1;
            } else if (rows[other].count == best) {
                winners += 1;
            }
        }
        const verify_sq1_tally_row *winner = NULL;
        for (size_t other = row_at; other < row_count; other++) {
            if (strcmp(rows[other].name, row->name) == 0 && rows[other].count == best) {
                if (winner == NULL) {
                    winner = &rows[other];
                }
            }
        }
        if (winners != 1 || winner == NULL) {
            if (out->ambiguous_count == ambiguous_cap) {
                size_t cap = ambiguous_cap == 0 ? 8 : ambiguous_cap * 2;
                char (*ambiguous)[VERIFY_SQ1_NAME_MAX] =
                    realloc(out->ambiguous, cap * sizeof *ambiguous);
                if (ambiguous == NULL) {
                    BT_TRACE_FAIL("ev_sq1.resolve_family_keys", "ambiguous oom");
                    break;
                }
                out->ambiguous = ambiguous;
                ambiguous_cap = cap;
            }
            snprintf(out->ambiguous[out->ambiguous_count], VERIFY_SQ1_NAME_MAX, "%s", row->name);
            out->ambiguous_count += 1;
            continue;
        }
        if (out->key_count == key_cap) {
            size_t cap = key_cap == 0 ? 8 : key_cap * 2;
            verify_sq1_family_key *keys = realloc(out->keys, cap * sizeof *keys);
            if (keys == NULL) {
                BT_TRACE_FAIL("ev_sq1.resolve_family_keys", "keys oom");
                break;
            }
            out->keys = keys;
            key_cap = cap;
        }
        verify_sq1_family_key *key = &out->keys[out->key_count];
        snprintf(key->name, sizeof key->name, "%s", winner->name);
        memcpy(key->key, winner->key, winner->key_len);
        key->key_len = winner->key_len;
        out->key_count += 1;
    }
    free(judged);
    free(rows);
    BT_TRACE_EXIT("ev_sq1.resolve_family_keys");
}

void verify_sq1_family_index_free(verify_sq1_family_index *index) {
    free(index->keys);
    index->keys = NULL;
    index->key_count = 0;
    free(index->ambiguous);
    index->ambiguous = NULL;
    index->ambiguous_count = 0;
}

static const verify_sq1_family_key *find_key(const verify_sq1_family_index *index,
                                             const char *name) {
    for (size_t at = 0; at < index->key_count; at++) {
        if (strcmp(index->keys[at].name, name) == 0) {
            return &index->keys[at];
        }
    }
    return NULL;
}

static bool name_is_ambiguous(const verify_sq1_family_index *index, const char *name) {
    for (size_t at = 0; at < index->ambiguous_count; at++) {
        if (strcmp(index->ambiguous[at], name) == 0) {
            return true;
        }
    }
    return false;
}

static void debug_bytes(const uint8_t *bytes, size_t len, char *out, size_t out_len) {
    size_t used = (size_t)snprintf(out, out_len, "[");
    for (size_t at = 0; at < len && used + 1 < out_len; at++) {
        used +=
            (size_t)snprintf(out + used, out_len - used, "%s%u", at == 0 ? "" : ", ", bytes[at]);
    }
    snprintf(out + used, out_len - used, "]");
}

verify_tier2 verify_sq1_cubeshape_tier2(const verify_sq1_case *case_,
                                        const verify_sq1_family_index *index) {
    BT_TRACE_ENTER("ev_sq1.cubeshape_tier2");
    char err[VERIFY_ERR_MAX];
    if (verify_sq1_class_invariants(case_, err, sizeof err) != 0) {
        BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
        return verify_tier2_fail(err);
    }
    char top[VERIFY_SQ1_NAME_MAX];
    char bottom[VERIFY_SQ1_NAME_MAX];
    if (verify_sq1_shape_names(case_->front_json, top, bottom, VERIFY_SQ1_NAME_MAX, err,
                               sizeof err) != 0) {
        BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
        return verify_tier2_fail(err);
    }
    if (strcmp(top, bottom) == 0 &&
        (case_->class_lens[0] != case_->class_lens[1] ||
         memcmp(case_->classes[0], case_->classes[1], case_->class_lens[0]) != 0)) {
        char message[VERIFY_OFFENDER_MAX];
        snprintf(message, sizeof message, "symmetric case %s holds two different layer classes",
                 top);
        BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
        return verify_tier2_fail(message);
    }
    const char *names[2] = {top, bottom};
    for (size_t at = 0; at < 2; at++) {
        if (name_is_ambiguous(index, names[at])) {
            char message[VERIFY_OFFENDER_MAX];
            snprintf(message, sizeof message, "family key for %s is ambiguous across the pack",
                     names[at]);
            BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
            return verify_tier2_fail(message);
        }
    }
    const verify_sq1_family_key *top_key = find_key(index, top);
    const verify_sq1_family_key *bottom_key = find_key(index, bottom);
    if (top_key == NULL || bottom_key == NULL) {
        char message[VERIFY_OFFENDER_MAX];
        snprintf(message, sizeof message, "shape names %s / %s hold no family keys", top, bottom);
        BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
        return verify_tier2_fail(message);
    }
    uint8_t observed[2][BT_PZ_SQ1_UNITS];
    for (size_t at = 0; at < 2; at++) {
        verify_sq1_famkey(case_->classes[at], case_->class_lens[at], observed[at]);
    }
    bool top_ok = (bool)(top_key->key_len == case_->class_lens[0] &&
                         memcmp(top_key->key, observed[0], top_key->key_len) == 0);
    bool bottom_ok = (bool)(bottom_key->key_len == case_->class_lens[1] &&
                            memcmp(bottom_key->key, observed[1], bottom_key->key_len) == 0);
    if (top_ok && bottom_ok) {
        BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
        return verify_tier2_pass();
    }
    char want_top[VERIFY_SQ1_DEBUG_MAX];
    char want_bottom[VERIFY_SQ1_DEBUG_MAX];
    char got_top[VERIFY_SQ1_DEBUG_MAX];
    char got_bottom[VERIFY_SQ1_DEBUG_MAX];
    debug_bytes(top_key->key, top_key->key_len, want_top, sizeof want_top);
    debug_bytes(bottom_key->key, bottom_key->key_len, want_bottom, sizeof want_bottom);
    debug_bytes(observed[0], case_->class_lens[0], got_top, sizeof got_top);
    debug_bytes(observed[1], case_->class_lens[1], got_bottom, sizeof got_bottom);
    char message[VERIFY_OFFENDER_MAX];
    snprintf(message, sizeof message,
             "names crossed: %s / %s declares keys %s / %s, state holds %s / %s", top, bottom,
             want_top, want_bottom, got_top, got_bottom);
    BT_TRACE_FAIL("ev_sq1.cubeshape_tier2", "names crossed %s / %s", top, bottom);
    BT_TRACE_EXIT("ev_sq1.cubeshape_tier2");
    return verify_tier2_fail(message);
}

void verify_sq1_ring_spans(const uint8_t *ring, size_t count, verify_sq1_span *out,
                           size_t *out_count) {
    BT_TRACE_ENTER("ev_sq1.ring_spans");
    size_t begin = 0;
    while (begin < count && ring[begin] == ring[(begin + count - 1) % count]) {
        begin += 1;
    }
    if (begin == count) {
        out[0].id = ring[0];
        out[0].width = (uint8_t)count;
        *out_count = 1;
        BT_TRACE_EXIT("ev_sq1.ring_spans");
        return;
    }
    size_t spans = 0;
    size_t index = begin;
    while (index < begin + count) {
        uint8_t id = ring[index % count];
        size_t width = 0;
        while (ring[(index + width) % count] == id) {
            width += 1;
        }
        out[spans].id = id;
        out[spans].width = (uint8_t)width;
        spans += 1;
        index += width;
    }
    *out_count = spans;
    BT_TRACE_EXIT("ev_sq1.ring_spans");
}

static bool bytes_hold(const uint8_t *set, size_t set_count, uint8_t id) {
    for (size_t at = 0; at < set_count; at++) {
        if (set[at] == id) {
            return true;
        }
    }
    return false;
}

static void layer_holds(const uint8_t *ring, const uint8_t *corners, const uint8_t *edges,
                        bool *corners_ok, bool *edges_ok) {
    verify_sq1_span spans[BT_PZ_SQ1_UNITS];
    size_t count = 0;
    verify_sq1_ring_spans(ring, BT_PZ_SQ1_UNITS, spans, &count);
    *corners_ok = true;
    *edges_ok = true;
    for (size_t at = 0; at < count; at++) {
        if (spans[at].width == 2) {
            *corners_ok = (bool)(*corners_ok && bytes_hold(corners, 4, spans[at].id));
        } else {
            *edges_ok = (bool)(*edges_ok && bytes_hold(edges, 4, spans[at].id));
        }
    }
}

static bool cyclic_rotation(const uint8_t *sequence, const uint8_t *pattern, size_t count) {
    for (size_t shift = 0; shift < count; shift++) {
        bool all = true;
        for (size_t index = 0; index < count; index++) {
            if (sequence[index] != pattern[(index + shift) % count]) {
                all = false;
                break;
            }
        }
        if (all) {
            return true;
        }
    }
    return false;
}

bool verify_sq1_corners_ordered(const bt_pz_sq1_state *state) {
    BT_TRACE_ENTER("ev_sq1.corners_ordered");
    uint8_t top[BT_PZ_SQ1_PIECES_PER_LAYER];
    uint8_t bottom[BT_PZ_SQ1_PIECES_PER_LAYER];
    size_t top_count = 0;
    size_t bottom_count = 0;
    for (size_t layer = 0; layer < 2; layer++) {
        verify_sq1_span spans[BT_PZ_SQ1_UNITS];
        size_t count = 0;
        verify_sq1_ring_spans(&state->units[layer * BT_PZ_SQ1_UNITS], BT_PZ_SQ1_UNITS, spans,
                              &count);
        for (size_t at = 0; at < count; at++) {
            if (spans[at].width != 2) {
                continue;
            }
            if (layer == 0) {
                top[top_count] = spans[at].id;
                top_count += 1;
            } else {
                bottom[bottom_count] = spans[at].id;
                bottom_count += 1;
            }
        }
    }
    bool ordered =
        (bool)(top_count == 4 && bottom_count == 4 && cyclic_rotation(top, TOP_CORNERS, 4) &&
               cyclic_rotation(bottom, BOTTOM_CORNERS, 4));
    BT_TRACE_EXIT("ev_sq1.corners_ordered");
    return ordered;
}

void verify_sq1_stage_predicates(const bt_pz_sq1_state *state, bool *corners, bool *edges,
                                 bool *placed) {
    BT_TRACE_ENTER("ev_sq1.stage_predicates");
    bool top_corners = true;
    bool top_edges = true;
    bool bottom_corners = true;
    bool bottom_edges = true;
    layer_holds(&state->units[0], TOP_CORNERS, TOP_EDGES, &top_corners, &top_edges);
    layer_holds(&state->units[BT_PZ_SQ1_UNITS], BOTTOM_CORNERS, BOTTOM_EDGES, &bottom_corners,
                &bottom_edges);
    bool corners_placed = true;
    for (size_t k = 0; k < 4; k++) {
        if (state->units[3 * k] != TOP_CORNERS[k] || state->units[3 * k + 1] != TOP_CORNERS[k]) {
            corners_placed = false;
        }
    }
    for (size_t k = 0; k < 4; k++) {
        uint8_t corner = BOTTOM_CORNERS[k];
        if (state->units[BT_PZ_SQ1_UNITS + 3 * k + 1] != corner ||
            state->units[BT_PZ_SQ1_UNITS + 3 * k + 2] != corner) {
            corners_placed = false;
        }
    }
    *corners = (bool)(top_corners && bottom_corners);
    *edges = (bool)(top_edges && bottom_edges);
    *placed = corners_placed;
    BT_TRACE_EXIT("ev_sq1.stage_predicates");
}

static bool classes_are_square(const verify_sq1_case *case_) {
    return (
        bool)(case_->class_lens[0] == BT_PZ_SQ1_PIECES_PER_LAYER &&
              memcmp(case_->classes[0], BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER) == 0 &&
              case_->class_lens[1] == BT_PZ_SQ1_PIECES_PER_LAYER &&
              memcmp(case_->classes[1], BT_PZ_SQ1_SQUARE_SHAPE, BT_PZ_SQ1_PIECES_PER_LAYER) == 0);
}

verify_tier2 verify_sq1_stage_outcome(verify_sq1_stage stage, const verify_sq1_case *case_) {
    BT_TRACE_ENTER("ev_sq1.stage_outcome");
    if (!classes_are_square(case_)) {
        BT_TRACE_EXIT("ev_sq1.stage_outcome");
        return verify_tier2_fail("start state is not square on both layers");
    }
    bool corners = false;
    bool edges = false;
    bool placed = false;
    verify_sq1_stage_predicates(&case_->state, &corners, &edges, &placed);
    verify_tier2 outcome;
    switch (stage) {
    case VERIFY_SQ1_STAGE_CO:
        outcome = corners ? verify_tier2_fail("corners already oriented") : verify_tier2_pass();
        break;
    case VERIFY_SQ1_STAGE_EO:
        if (!corners) {
            outcome = verify_tier2_fail("corners not oriented");
        } else if (!edges) {
            outcome = verify_tier2_pass();
        } else {
            outcome = verify_tier2_fail("edges already oriented");
        }
        break;
    case VERIFY_SQ1_STAGE_CP:
        if (!corners) {
            outcome = verify_tier2_fail("corners not oriented");
        } else if (!edges) {
            outcome = verify_tier2_fail("edges not oriented");
        } else if (!placed) {
            outcome = verify_tier2_pass();
        } else {
            outcome = verify_tier2_fail("corners already placed");
        }
        break;
    default:
        if (!corners) {
            outcome = verify_tier2_fail("corners not oriented");
        } else if (!verify_sq1_corners_ordered(&case_->state)) {
            outcome = verify_tier2_fail("corners not in solved order");
        } else if (bt_pz_sq1_is_solved(&case_->state)) {
            outcome = verify_tier2_fail("start state is already solved");
        } else {
            outcome = verify_tier2_pass();
        }
        break;
    }
    BT_TRACE_EXIT("ev_sq1.stage_outcome");
    return outcome;
}

static void verify_stage(const bt_seed_rows *rows, verify_sq1_stage stage,
                         verify_section_result *out) {
    BT_TRACE_ENTER("ev_sq1.verify_stage");
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        verify_row_outcome outcome = {0};
        snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
        verify_sq1_case case_;
        char err[VERIFY_ERR_MAX];
        if (verify_sq1_start_case(row->alg, &case_, err, sizeof err) != 0) {
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err, "%s", err);
            outcome.tier2 = verify_tier2_skipped("tier 1 failed");
        } else {
            outcome.tier1_ok = true;
            case_.front_json = row->front_json;
            outcome.tier2 = verify_sq1_stage_outcome(stage, &case_);
        }
        verify_section_push_row(out, &outcome);
    }
    BT_TRACE_EXIT("ev_sq1.verify_stage");
}

void verify_sq1_verify_cubeshape(const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("ev_sq1.verify_cubeshape");
    verify_section_init(out);
    verify_sq1_case *cases = malloc(rows->count * sizeof *cases);
    const verify_sq1_case **observed = malloc(rows->count * sizeof *observed);
    verify_row_outcome *outcomes = malloc(rows->count * sizeof *outcomes);
    if (cases == NULL || observed == NULL || outcomes == NULL) {
        BT_TRACE_FAIL("ev_sq1.verify_cubeshape", "oom over %zu rows", rows->count);
        exit(1);
    }
    size_t observed_count = 0;
    for (size_t at = 0; at < rows->count; at++) {
        const bt_seed_row *row = &rows->items[at];
        outcomes[at] = (verify_row_outcome){0};
        snprintf(outcomes[at].case_key, sizeof outcomes[at].case_key, "%s", row->case_key);
        char err[VERIFY_ERR_MAX];
        if (verify_sq1_start_case(row->alg, &cases[at], err, sizeof err) != 0) {
            snprintf(outcomes[at].tier1_err, sizeof outcomes[at].tier1_err, "%s", err);
            outcomes[at].tier2 = verify_tier2_skipped("tier 1 failed");
            continue;
        }
        outcomes[at].tier1_ok = true;
        cases[at].front_json = row->front_json;
        observed[observed_count] = &cases[at];
        observed_count += 1;
    }
    verify_sq1_family_index index;
    verify_sq1_resolve_family_keys(observed, observed_count, &index);
    for (size_t at = 0; at < rows->count; at++) {
        verify_row_outcome *outcome = &outcomes[at];
        if (outcome->tier1_ok) {
            outcome->tier2 = verify_sq1_cubeshape_tier2(&cases[at], &index);
        }
        verify_section_push_row(out, outcome);
    }
    verify_sq1_family_index_free(&index);
    free(cases);
    free(observed);
    free(outcomes);
    BT_TRACE_EXIT("ev_sq1.verify_cubeshape");
}

void verify_sq1_verify_co(const bt_seed_rows *rows, verify_section_result *out) {
    verify_stage(rows, VERIFY_SQ1_STAGE_CO, out);
}

void verify_sq1_verify_eo(const bt_seed_rows *rows, verify_section_result *out) {
    verify_stage(rows, VERIFY_SQ1_STAGE_EO, out);
}

void verify_sq1_verify_cp(const bt_seed_rows *rows, verify_section_result *out) {
    verify_stage(rows, VERIFY_SQ1_STAGE_CP, out);
}

void verify_sq1_verify_ep(const bt_seed_rows *rows, verify_section_result *out) {
    verify_stage(rows, VERIFY_SQ1_STAGE_EP, out);
}
