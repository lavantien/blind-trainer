#include "verify/ev_bigcube.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "puzzles/tier1.h"
#include "verify/lib.h"

static const char *const BIGCUBE_HONESTY =
    "wing permutation parity is invisible through facelets, two swapped dedges and four wings in "
    "pairwise flips render identically, the line-count bound is the strongest facelet-seam "
    "predicate";

size_t verify_bigcube_sizes_of(const bt_nspec *n, size_t *out, size_t cap) {
    BT_TRACE_ENTER("ev_bigcube.sizes_of");
    size_t count = 0;
    if (n->kind == BT_NSPEC_FIXED) {
        if (cap > 0) {
            out[0] = (size_t)n->fixed;
            count = 1;
        }
    } else if (n->kind == BT_NSPEC_SIZES) {
        for (size_t at = 0; at < n->size_count && count < cap; at++) {
            out[count] = (size_t)n->sizes[at];
            count += 1;
        }
    }
    BT_TRACE_EXIT("ev_bigcube.sizes_of");
    return count;
}

int verify_bigcube_tagged_size(const char *tag, size_t *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bigcube.tagged_size");
    bool saw_digit = false;
    bool overflow = false;
    size_t value = 0;
    for (const char *cursor = tag; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            continue;
        }
        saw_digit = true;
        if (overflow) {
            continue;
        }
        size_t digit = (size_t)(*cursor - '0');
        if (value > (SIZE_MAX - digit) / 10) {
            overflow = true;
            continue;
        }
        value = value * 10 + digit;
    }
    if (!saw_digit || overflow) {
        snprintf(err, err_len, "tag %s holds no cube size", tag);
        BT_TRACE_FAIL("ev_bigcube.tagged_size", "tag %s", tag);
        BT_TRACE_EXIT("ev_bigcube.tagged_size");
        return 1;
    }
    *out = value;
    BT_TRACE_EXIT("ev_bigcube.tagged_size");
    return 0;
}

static bool is_midge(size_t n, size_t index) {
    return (bool)(n % 2 == 1 && index == n / 2);
}

void verify_bigcube_audit_face(const bt_pz_grid *grid, size_t face, size_t n,
                               verify_bigcube_face_audit *out) {
    const bt_pz_face *face_grid = &grid->faces[face];
    uint8_t center_color = face_grid->rows[1].cells[1];
    out->center_color = center_color;
    out->centers_uniform = true;
    for (size_t row = 1; row + 1 < n; row++) {
        for (size_t col = 1; col + 1 < n; col++) {
            if (face_grid->rows[row].cells[col] != center_color) {
                out->centers_uniform = false;
            }
        }
    }
    size_t corner_rows[4] = {0, 0, n - 1, n - 1};
    size_t corner_cols[4] = {0, n - 1, 0, n - 1};
    out->corners_solved = true;
    for (size_t at = 0; at < 4; at++) {
        if (face_grid->rows[corner_rows[at]].cells[corner_cols[at]] != center_color) {
            out->corners_solved = false;
        }
    }
    out->midges_solved = true;
    for (size_t index = 1; index + 1 < n; index++) {
        if (!is_midge(n, index)) {
            continue;
        }
        if (face_grid->rows[0].cells[index] != center_color ||
            face_grid->rows[n - 1].cells[index] != center_color ||
            face_grid->rows[index].cells[0] != center_color ||
            face_grid->rows[index].cells[n - 1] != center_color) {
            out->midges_solved = false;
        }
    }
    out->disturbed_slots = 0;
    for (size_t segment = 0; segment < 4; segment++) {
        bool disturbed = false;
        for (size_t index = 1; index + 1 < n; index++) {
            size_t row;
            size_t col;
            if (segment == 0) {
                row = 0;
                col = index;
            } else if (segment == 1) {
                row = n - 1;
                col = index;
            } else if (segment == 2) {
                row = index;
                col = 0;
            } else {
                row = index;
                col = n - 1;
            }
            if (!is_midge(n, index) && face_grid->rows[row].cells[col] != center_color) {
                disturbed = true;
            }
        }
        if (disturbed) {
            out->disturbed_slots += 1;
        }
    }
}

int verify_bigcube_reduction_state(const bt_pz_nxn *engine, const bt_pz_nxn_state *state, char *err,
                                   size_t err_len) {
    BT_TRACE_ENTER("ev_bigcube.reduction_state");
    size_t n = bt_pz_nxn_n(engine);
    bt_pz_grid grid;
    bt_pz_nxn_facelets(engine, state, &grid);
    verify_bigcube_face_audit audits[6];
    for (size_t face = 0; face < 6; face++) {
        verify_bigcube_audit_face(&grid, face, n, &audits[face]);
    }
    for (size_t face = 0; face < 6; face++) {
        if (!audits[face].centers_uniform) {
            snprintf(err, err_len, "%s", "center block of some face is not uniform");
            BT_TRACE_FAIL("ev_bigcube.reduction_state", "center uniformity");
            BT_TRACE_EXIT("ev_bigcube.reduction_state");
            return -1;
        }
    }
    bool seen[256] = {false};
    size_t distinct = 0;
    for (size_t face = 0; face < 6; face++) {
        uint8_t color = audits[face].center_color;
        if (!seen[color]) {
            seen[color] = true;
            distinct += 1;
        }
    }
    if (distinct != 6) {
        snprintf(err, err_len, "the six center blocks hold only %zu distinct colors", distinct);
        BT_TRACE_FAIL("ev_bigcube.reduction_state", "%zu distinct", distinct);
        BT_TRACE_EXIT("ev_bigcube.reduction_state");
        return -1;
    }
    for (size_t face = 0; face < 6; face++) {
        if (!audits[face].corners_solved) {
            snprintf(err, err_len, "%s", "some corner sticker left its face color");
            BT_TRACE_FAIL("ev_bigcube.reduction_state", "corners");
            BT_TRACE_EXIT("ev_bigcube.reduction_state");
            return -1;
        }
    }
    for (size_t face = 0; face < 6; face++) {
        if (!audits[face].midges_solved) {
            snprintf(err, err_len, "%s", "some midge sticker left its face color");
            BT_TRACE_FAIL("ev_bigcube.reduction_state", "midges");
            BT_TRACE_EXIT("ev_bigcube.reduction_state");
            return -1;
        }
    }
    size_t slots = 0;
    for (size_t face = 0; face < 6; face++) {
        slots += audits[face].disturbed_slots;
    }
    if (slots % 2 != 0) {
        snprintf(err, err_len, "%zu half-lines disturbed, dedge lines must pair across faces",
                 slots);
        BT_TRACE_FAIL("ev_bigcube.reduction_state", "%zu half-lines", slots);
        BT_TRACE_EXIT("ev_bigcube.reduction_state");
        return -1;
    }
    BT_TRACE_EXIT("ev_bigcube.reduction_state");
    return (int)(slots / 2);
}

static void debug_sizes(const size_t *sizes, size_t count, char *out, size_t out_len) {
    size_t used = (size_t)snprintf(out, out_len, "[");
    for (size_t at = 0; at < count && used + 1 < out_len; at++) {
        used +=
            (size_t)snprintf(out + used, out_len - used, "%s%zu", at == 0 ? "" : ", ", sizes[at]);
    }
    snprintf(out + used, out_len - used, "]");
}

int verify_bigcube_row_sizes(const bt_pack_entry *entry, const char *tag, size_t *out, size_t cap,
                             char *err, size_t err_len) {
    BT_TRACE_ENTER("ev_bigcube.row_sizes");
    size_t registry[VERIFY_BIGCUBE_MAX_SIZES];
    size_t registry_count = verify_bigcube_sizes_of(&entry->n, registry, VERIFY_BIGCUBE_MAX_SIZES);
    if (registry_count == 0) {
        snprintf(err, err_len, "%s", "registry holds no cube sizes for this pack");
        BT_TRACE_FAIL("ev_bigcube.row_sizes", "empty registry");
        BT_TRACE_EXIT("ev_bigcube.row_sizes");
        return -1;
    }
    if (strcmp(entry->method, "yau") != 0) {
        size_t count = registry_count < cap ? registry_count : cap;
        for (size_t at = 0; at < count; at++) {
            out[at] = registry[at];
        }
        BT_TRACE_EXIT("ev_bigcube.row_sizes");
        return (int)count;
    }
    size_t size = 0;
    if (verify_bigcube_tagged_size(tag, &size, err, err_len) != 0) {
        BT_TRACE_FAIL("ev_bigcube.row_sizes", "tag %s", tag);
        BT_TRACE_EXIT("ev_bigcube.row_sizes");
        return -1;
    }
    bool contains = false;
    for (size_t at = 0; at < registry_count; at++) {
        if (registry[at] == size) {
            contains = true;
        }
    }
    if (!contains) {
        char debug[128];
        debug_sizes(registry, registry_count, debug, sizeof debug);
        snprintf(err, err_len, "tag size %zu sits outside the registry sizes %s", size, debug);
        BT_TRACE_FAIL("ev_bigcube.row_sizes", "tag %zu outside", size);
        BT_TRACE_EXIT("ev_bigcube.row_sizes");
        return -1;
    }
    if (cap > 0) {
        out[0] = size;
    }
    BT_TRACE_EXIT("ev_bigcube.row_sizes");
    return 1;
}

int verify_bigcube_tag_of(const char *front_json, char *out, size_t out_len) {
    BT_TRACE_ENTER("ev_bigcube.tag_of");
    bt_json_value *value;
    bt_json_error json_err;
    if (bt_json_parse(front_json, strlen(front_json), &value, &json_err) != 0) {
        snprintf(out, out_len, "%s", json_err.message);
        BT_TRACE_FAIL("ev_bigcube.tag_of", "json failed");
        BT_TRACE_EXIT("ev_bigcube.tag_of");
        return 1;
    }
    const bt_json_value *field = bt_json_get(value, "case");
    bool ok = (bool)(field != NULL && field->type == BT_JSON_STRING);
    if (ok) {
        snprintf(out, out_len, "%s", field->as.string);
    } else {
        snprintf(out, out_len, "%s", "front_json holds no case tag");
    }
    bt_json_free(value);
    if (!ok) {
        BT_TRACE_FAIL("ev_bigcube.tag_of", "no case tag");
        BT_TRACE_EXIT("ev_bigcube.tag_of");
        return 1;
    }
    BT_TRACE_EXIT("ev_bigcube.tag_of");
    return 0;
}

static void tier1_step(const bt_pz_nxn *engine, const char *alg, size_t size,
                       verify_row_outcome *outcome) {
    char err[VERIFY_ERR_MAX];
    bt_pz_engine eng = bt_pz_nxn_engine(engine);
    bt_pz_error pz_err;
    void *alg_ptr = bt_pz_alg_create();
    if (alg_ptr == NULL || !eng.parse_alg(&eng, alg, strlen(alg), alg_ptr, &pz_err)) {
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_pz_error_message(&pz_err, &sb);
        snprintf(err, sizeof err, "%s", sb.data);
        bt_strbuf_free(&sb);
        bt_pz_alg_destroy(alg_ptr);
    } else if (bt_pz_alg_is_empty(alg_ptr)) {
        snprintf(err, sizeof err, "%s", "alg is empty");
        bt_pz_alg_destroy(alg_ptr);
    } else {
        bt_pz_alg_destroy(alg_ptr);
        bt_tier1_error tier1_err;
        if (bt_verify_tier1(&eng, alg, &tier1_err) == BT_TIER1_OK) {
            outcome->tier1_ok = true;
            return;
        }
        bt_strbuf sb;
        bt_strbuf_init(&sb, VERIFY_ERR_STRBUF_INIT);
        bt_tier1_error_message(&tier1_err, &sb);
        snprintf(err, sizeof err, "%s", sb.data);
        bt_strbuf_free(&sb);
    }
    snprintf(outcome->tier1_err, sizeof outcome->tier1_err, "n=%zu: %s", size, err);
}

static void verify_row(const bt_pack_entry *entry, const bt_seed_row *row,
                       verify_row_outcome *outcome) {
    BT_TRACE_ENTER("ev_bigcube.verify_row");
    memset(outcome, 0, sizeof *outcome);
    snprintf(outcome->case_key, sizeof outcome->case_key, "%s", row->case_key);
    char tag[64];
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    char err[VERIFY_ERR_MAX];
    err[0] = '\0';
    if (verify_bigcube_tag_of(row->front_json, tag, sizeof tag) != 0) {
        snprintf(outcome->tier1_err, sizeof outcome->tier1_err, "%s", tag);
        outcome->tier2 = verify_tier2_skipped("tier 1 failed");
        BT_TRACE_FAIL("ev_bigcube.verify_row", "tag or sizes failed %s", row->case_key);
        BT_TRACE_EXIT("ev_bigcube.verify_row");
        return;
    }
    if (verify_bigcube_row_sizes(entry, tag, sizes, VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err) <
        0) {
        snprintf(outcome->tier1_err, sizeof outcome->tier1_err, "%s", err);
        outcome->tier2 = verify_tier2_skipped("tier 1 failed");
        BT_TRACE_FAIL("ev_bigcube.verify_row", "tag or sizes failed %s", row->case_key);
        BT_TRACE_EXIT("ev_bigcube.verify_row");
        return;
    }
    size_t size_count = (size_t)verify_bigcube_row_sizes(entry, tag, sizes,
                                                         VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err);
    size_t low = strcmp(entry->method, "yau") == 0 ? 1 : 2;
    size_t high = 2;
    outcome->tier1_ok = true;
    outcome->tier2 = verify_tier2_pass();
    for (size_t at = 0; at < size_count; at++) {
        size_t size = sizes[at];
        bt_pz_nxn engine;
        if (!bt_pz_nxn_init(&engine, size)) {
            outcome->tier1_ok = false;
            snprintf(outcome->tier1_err, sizeof outcome->tier1_err,
                     "engine init failed at size %zu, registry sizes sit inside the engine range",
                     size);
            outcome->tier2 = verify_tier2_skipped("tier 1 failed");
            BT_TRACE_FAIL("ev_bigcube.verify_row", "engine init %zu failed", size);
            BT_TRACE_EXIT("ev_bigcube.verify_row");
            return;
        }
        verify_row_outcome step = {0};
        tier1_step(&engine, row->alg, size, &step);
        if (!step.tier1_ok) {
            outcome->tier1_ok = false;
            snprintf(outcome->tier1_err, sizeof outcome->tier1_err, "%s", step.tier1_err);
            outcome->tier2 = verify_tier2_skipped("tier 1 failed");
            return;
        }
        bt_pz_error pz_err;
        bt_pz_engine eng = bt_pz_nxn_engine(&engine);
        void *alg = bt_pz_alg_create();
        void *inverse = bt_pz_alg_create();
        eng.parse_alg(&eng, row->alg, strlen(row->alg), alg, &pz_err);
        eng.invert_alg(&eng, alg, inverse);
        bt_pz_nxn_state state;
        bt_pz_nxn_solved_state(&engine, &state);
        bt_pz_nxn_apply_alg(&engine, &state, inverse, &pz_err);
        bt_pz_alg_destroy(alg);
        bt_pz_alg_destroy(inverse);
        char reduction_err[VERIFY_ERR_MAX];
        int lines =
            verify_bigcube_reduction_state(&engine, &state, reduction_err, sizeof reduction_err);
        if (lines >= 0 && lines >= (int)low && lines <= (int)high) {
            continue;
        }
        if (lines >= 0) {
            char message[VERIFY_ERR_MAX];
            snprintf(message, sizeof message, "n=%zu: %s disturbs %d dedge lines", size, tag,
                     lines);
            outcome->tier2 = verify_tier2_fail(message);
        } else {
            char message[VERIFY_ERR_MAX];
            snprintf(message, sizeof message, "n=%zu: %s", size, reduction_err);
            outcome->tier2 = verify_tier2_fail(message);
        }
        BT_TRACE_FAIL("ev_bigcube.verify_row", "size %zu out of bounds", size);
        BT_TRACE_EXIT("ev_bigcube.verify_row");
        return;
    }
    BT_TRACE_EXIT("ev_bigcube.verify_row");
}

void verify_bigcube_verify_parity(const bt_pack_entry *entry, const bt_seed_rows *rows,
                                  verify_section_result *out) {
    BT_TRACE_ENTER("ev_bigcube.verify_parity");
    verify_section_init(out);
    for (size_t at = 0; at < rows->count; at++) {
        verify_row_outcome outcome;
        verify_row(entry, &rows->items[at], &outcome);
        verify_section_push_row(out, &outcome);
    }
    verify_section_push_honesty(out, BIGCUBE_HONESTY);
    BT_TRACE_EXIT("ev_bigcube.verify_parity");
}
