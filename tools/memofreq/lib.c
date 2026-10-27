#include "memofreq/lib.h"

#include "common/bt_trace.h"
#include "data/paths.h"
#include "scheme/memo.h"
#include "scheme/scramble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *bt_mf_tool_id(void) {
    BT_TRACE_ENTER("mf.tool_id");
    BT_TRACE_EXIT("mf.tool_id");
    return "bt-memofreq";
}

void bt_mf_data_ga_path(const char *relative, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("mf.data_ga_path");
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    bt_paths_join(root, relative, out);
    BT_TRACE_EXIT("mf.data_ga_path");
}

size_t bt_mf_edge_count(void) {
    BT_TRACE_ENTER("mf.edge_count");
    BT_TRACE_EXIT("mf.edge_count");
    return BT_MF_EDGE_COUNT;
}

size_t bt_mf_corner_count(void) {
    BT_TRACE_ENTER("mf.corner_count");
    BT_TRACE_EXIT("mf.corner_count");
    return BT_MF_CORNER_COUNT;
}

size_t bt_mf_total_cases(void) {
    BT_TRACE_ENTER("mf.total_cases");
    BT_TRACE_EXIT("mf.total_cases");
    return BT_MF_TOTAL_CASES;
}

uint64_t bt_mf_draw_seed(uint64_t seed, uint64_t index) {
    BT_TRACE_ENTER("mf.draw_seed");
    uint64_t z = seed + index * 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    uint64_t mixed = z ^ (z >> 31U);
    BT_TRACE_EXIT("mf.draw_seed");
    return mixed;
}

size_t bt_mf_triangle(size_t size) {
    BT_TRACE_ENTER("mf.triangle");
    size_t value = size * (size + 1) / 2;
    BT_TRACE_EXIT("mf.triangle");
    return value;
}

size_t bt_mf_pair_index(size_t a, size_t b) {
    BT_TRACE_ENTER("mf.pair_index");
    size_t lo = a <= b ? a : b;
    size_t hi = a <= b ? b : a;
    size_t index = hi * (hi + 1) / 2 + lo;
    BT_TRACE_EXIT("mf.pair_index");
    return index;
}

size_t bt_mf_row_of(size_t index) {
    BT_TRACE_ENTER("mf.row_of");
    size_t hi = 0;
    while (bt_mf_triangle(hi + 1) <= index) {
        hi++;
    }
    BT_TRACE_EXIT("mf.row_of");
    return hi;
}

size_t bt_mf_col_of(size_t index) {
    BT_TRACE_ENTER("mf.col_of");
    size_t col = index - bt_mf_triangle(bt_mf_row_of(index));
    BT_TRACE_EXIT("mf.col_of");
    return col;
}

void bt_mf_add_stream(uint64_t *cases, size_t base, uint64_t *adj, uint64_t *cooc,
                      const size_t *indices, size_t indices_count) {
    BT_TRACE_ENTER("mf.add_stream");
    for (size_t i = 0; i < indices_count; i++) {
        cases[indices[i]] += 1;
    }
    for (size_t i = 1; i < indices_count; i++) {
        adj[bt_mf_pair_index(indices[i - 1] - base, indices[i] - base)] += 1;
    }
    for (size_t i = 0; i < indices_count; i++) {
        for (size_t j = i + 1; j < indices_count; j++) {
            cooc[bt_mf_pair_index(indices[i] - base, indices[j] - base)] += 1;
        }
    }
    BT_TRACE_EXIT("mf.add_stream");
}

bt_ga_pair_count *bt_mf_drain_pairs(const uint64_t *counts, size_t counts_count, size_t base,
                                    size_t *out_count) {
    BT_TRACE_ENTER("mf.drain_pairs");
    size_t kept = 0;
    for (size_t index = 0; index < counts_count; index++) {
        if (counts[index] > 0) {
            kept++;
        }
    }
    *out_count = kept;
    if (kept == 0) {
        BT_TRACE_EXIT("mf.drain_pairs");
        return NULL;
    }
    bt_ga_pair_count *rows = malloc(kept * sizeof *rows);
    if (rows == NULL) {
        *out_count = 0;
        BT_TRACE_FAIL("mf.drain_pairs", "allocation failed for %zu rows", kept);
        return NULL;
    }
    size_t at = 0;
    for (size_t index = 0; index < counts_count; index++) {
        if (counts[index] > 0) {
            rows[at].from = (uint32_t)(base + bt_mf_row_of(index));
            rows[at].to = (uint32_t)(base + bt_mf_col_of(index));
            rows[at].count = counts[index];
            at++;
        }
    }
    BT_TRACE_EXIT("mf.drain_pairs");
    return rows;
}

void bt_mf_orbit_tables_build(bt_mf_orbit_tables *tables) {
    BT_TRACE_ENTER("mf.orbit_tables_build");
    const size_t slots = BT_MF_STICKER_SLOTS * BT_MF_STICKER_SLOTS;
    for (size_t i = 0; i < slots; i++) {
        tables->index[i] = -1;
    }
    const bt_orbit orbits[2] = {BT_ORBIT_EDGE, BT_ORBIT_CORNER};
    const size_t offsets[2] = {0, BT_MF_EDGE_COUNT};
    tables->case_count = 0;
    for (int span = 0; span < 2; span++) {
        size_t enumerated = bt_scheme_enumerate_cases(orbits[span], tables->cases + offsets[span]);
        for (size_t i = 0; i < enumerated; i++) {
            const bt_case *case_id = &tables->cases[offsets[span] + i];
            tables->index[(size_t)case_id->t1 * BT_MF_STICKER_SLOTS + case_id->t2] =
                (int32_t)(offsets[span] + i);
        }
        tables->case_count += enumerated;
    }
    BT_TRACE_EXIT("mf.orbit_tables_build");
}

static int draw_counts_alloc(bt_mf_draw_counts *counts) {
    counts->cases = calloc(bt_mf_total_cases(), sizeof *counts->cases);
    counts->edge_cooc = calloc(bt_mf_triangle(bt_mf_edge_count()), sizeof *counts->edge_cooc);
    counts->corner_cooc = calloc(bt_mf_triangle(bt_mf_corner_count()), sizeof *counts->corner_cooc);
    counts->edge_adj = calloc(bt_mf_triangle(bt_mf_edge_count()), sizeof *counts->edge_adj);
    counts->corner_adj = calloc(bt_mf_triangle(bt_mf_corner_count()), sizeof *counts->corner_adj);
    return counts->cases != NULL && counts->edge_cooc != NULL && counts->corner_cooc != NULL &&
                   counts->edge_adj != NULL && counts->corner_adj != NULL
               ? 0
               : -1;
}

int bt_mf_draw_counts_new(bt_mf_draw_counts *counts) {
    BT_TRACE_ENTER("mf.draw_counts_new");
    memset(counts, 0, sizeof *counts);
    int rc = draw_counts_alloc(counts);
    if (rc != 0) {
        bt_mf_draw_counts_free(counts);
        BT_TRACE_FAIL("mf.draw_counts_new", "allocation failed");
        return -1;
    }
    BT_TRACE_EXIT("mf.draw_counts_new");
    return 0;
}

int bt_mf_draw_counts_clone(const bt_mf_draw_counts *src, bt_mf_draw_counts *dst) {
    BT_TRACE_ENTER("mf.draw_counts_clone");
    memset(dst, 0, sizeof *dst);
    if (draw_counts_alloc(dst) != 0) {
        bt_mf_draw_counts_free(dst);
        BT_TRACE_FAIL("mf.draw_counts_clone", "allocation failed");
        return -1;
    }
    memcpy(dst->cases, src->cases, bt_mf_total_cases() * sizeof *dst->cases);
    memcpy(dst->edge_cooc, src->edge_cooc,
           bt_mf_triangle(bt_mf_edge_count()) * sizeof *dst->edge_cooc);
    memcpy(dst->corner_cooc, src->corner_cooc,
           bt_mf_triangle(bt_mf_corner_count()) * sizeof *dst->corner_cooc);
    memcpy(dst->edge_adj, src->edge_adj,
           bt_mf_triangle(bt_mf_edge_count()) * sizeof *dst->edge_adj);
    memcpy(dst->corner_adj, src->corner_adj,
           bt_mf_triangle(bt_mf_corner_count()) * sizeof *dst->corner_adj);
    BT_TRACE_EXIT("mf.draw_counts_clone");
    return 0;
}

void bt_mf_draw_counts_free(bt_mf_draw_counts *counts) {
    BT_TRACE_ENTER("mf.draw_counts_free");
    free(counts->cases);
    free(counts->edge_cooc);
    free(counts->corner_cooc);
    free(counts->edge_adj);
    free(counts->corner_adj);
    counts->cases = NULL;
    counts->edge_cooc = NULL;
    counts->corner_cooc = NULL;
    counts->edge_adj = NULL;
    counts->corner_adj = NULL;
    BT_TRACE_EXIT("mf.draw_counts_free");
}

void bt_mf_draw_counts_merge(bt_mf_draw_counts *self, const bt_mf_draw_counts *other) {
    BT_TRACE_ENTER("mf.draw_counts_merge");
    for (size_t i = 0; i < bt_mf_total_cases(); i++) {
        self->cases[i] += other->cases[i];
    }
    for (size_t i = 0; i < bt_mf_triangle(bt_mf_edge_count()); i++) {
        self->edge_cooc[i] += other->edge_cooc[i];
    }
    for (size_t i = 0; i < bt_mf_triangle(bt_mf_corner_count()); i++) {
        self->corner_cooc[i] += other->corner_cooc[i];
    }
    for (size_t i = 0; i < bt_mf_triangle(bt_mf_edge_count()); i++) {
        self->edge_adj[i] += other->edge_adj[i];
    }
    for (size_t i = 0; i < bt_mf_triangle(bt_mf_corner_count()); i++) {
        self->corner_adj[i] += other->corner_adj[i];
    }
    BT_TRACE_EXIT("mf.draw_counts_merge");
}

void bt_mf_draw_counts_add_draw(bt_mf_draw_counts *counts, const bt_mf_orbit_tables *tables,
                                uint64_t seed) {
    BT_TRACE_ENTER("mf.add_draw");
    uint8_t occ[54];
    bt_scheme_random_occ(seed, occ);
    bt_memo memo;
    bt_scheme_memo_from_array(occ, &memo);
    size_t edge[BT_MEMO_MAX_PAIRS];
    size_t corner[BT_MEMO_MAX_PAIRS];
    for (size_t i = 0; i < memo.edge_pair_count; i++) {
        edge[i] = (size_t)tables->index[(size_t)memo.edge_pairs[i].t1 * BT_MF_STICKER_SLOTS +
                                        memo.edge_pairs[i].t2];
    }
    for (size_t i = 0; i < memo.corner_pair_count; i++) {
        corner[i] = (size_t)tables->index[(size_t)memo.corner_pairs[i].t1 * BT_MF_STICKER_SLOTS +
                                          memo.corner_pairs[i].t2];
    }
    bt_mf_add_stream(counts->cases, 0, counts->edge_adj, counts->edge_cooc, edge,
                     memo.edge_pair_count);
    bt_mf_add_stream(counts->cases, bt_mf_edge_count(), counts->corner_adj, counts->corner_cooc,
                     corner, memo.corner_pair_count);
    BT_TRACE_EXIT("mf.add_draw");
}

bt_ga_freq_data *bt_mf_count_frequencies(uint64_t samples, uint64_t seed) {
    BT_TRACE_ENTER("mf.count_frequencies");
    bt_mf_orbit_tables tables;
    bt_mf_orbit_tables_build(&tables);
    bt_mf_draw_counts counts;
    if (bt_mf_draw_counts_new(&counts) != 0) {
        BT_TRACE_FAIL("mf.count_frequencies", "draw counts allocation failed");
        return NULL;
    }
    for (uint64_t draw = 0; draw < samples; draw++) {
        bt_mf_draw_counts_add_draw(&counts, &tables, bt_mf_draw_seed(seed, draw));
    }
    bt_ga_freq_data *data = calloc(1, sizeof *data);
    if (data == NULL) {
        bt_mf_draw_counts_free(&counts);
        BT_TRACE_FAIL("mf.count_frequencies", "result allocation failed");
        return NULL;
    }
    data->cases = malloc((tables.case_count > 0 ? tables.case_count : 1) * sizeof *data->cases);
    if (data->cases == NULL) {
        bt_mf_draw_counts_free(&counts);
        free(data);
        BT_TRACE_FAIL("mf.count_frequencies", "case rows allocation failed");
        return NULL;
    }
    data->cases_count = tables.case_count;
    for (size_t i = 0; i < tables.case_count; i++) {
        const char *orbit = tables.cases[i].orbit == BT_ORBIT_EDGE ? "edge" : "corner";
        char letters[2];
        bt_scheme_case_letters(&tables.cases[i], letters);
        snprintf(data->cases[i].orbit, sizeof data->cases[i].orbit, "%s", orbit);
        snprintf(data->cases[i].letters, sizeof data->cases[i].letters, "%c%c", letters[0],
                 letters[1]);
        data->cases[i].count = counts.cases[i];
    }
    size_t edge_adj_count = 0;
    size_t corner_adj_count = 0;
    size_t edge_cooc_count = 0;
    size_t corner_cooc_count = 0;
    bt_ga_pair_count *edge_adj =
        bt_mf_drain_pairs(counts.edge_adj, bt_mf_triangle(bt_mf_edge_count()), 0, &edge_adj_count);
    bt_ga_pair_count *corner_adj =
        bt_mf_drain_pairs(counts.corner_adj, bt_mf_triangle(bt_mf_corner_count()),
                          bt_mf_edge_count(), &corner_adj_count);
    bt_ga_pair_count *edge_cooc = bt_mf_drain_pairs(
        counts.edge_cooc, bt_mf_triangle(bt_mf_edge_count()), 0, &edge_cooc_count);
    bt_ga_pair_count *corner_cooc =
        bt_mf_drain_pairs(counts.corner_cooc, bt_mf_triangle(bt_mf_corner_count()),
                          bt_mf_edge_count(), &corner_cooc_count);
    data->adjacent_count = edge_adj_count + corner_adj_count;
    data->adjacent =
        malloc((data->adjacent_count > 0 ? data->adjacent_count : 1) * sizeof *data->adjacent);
    data->cooccurring_count = edge_cooc_count + corner_cooc_count;
    data->cooccurring = malloc((data->cooccurring_count > 0 ? data->cooccurring_count : 1) *
                               sizeof *data->cooccurring);
    if (data->adjacent == NULL || data->cooccurring == NULL) {
        free(edge_adj);
        free(corner_adj);
        free(edge_cooc);
        free(corner_cooc);
        bt_ga_freq_data_free(data);
        free(data);
        bt_mf_draw_counts_free(&counts);
        BT_TRACE_FAIL("mf.count_frequencies", "pair rows allocation failed");
        return NULL;
    }
    if (edge_adj_count > 0) {
        memcpy(data->adjacent, edge_adj, edge_adj_count * sizeof *data->adjacent);
    }
    if (corner_adj_count > 0) {
        memcpy(data->adjacent + edge_adj_count, corner_adj,
               corner_adj_count * sizeof *data->adjacent);
    }
    if (edge_cooc_count > 0) {
        memcpy(data->cooccurring, edge_cooc, edge_cooc_count * sizeof *data->cooccurring);
    }
    if (corner_cooc_count > 0) {
        memcpy(data->cooccurring + edge_cooc_count, corner_cooc,
               corner_cooc_count * sizeof *data->cooccurring);
    }
    free(edge_adj);
    free(corner_adj);
    free(edge_cooc);
    free(corner_cooc);
    data->total_draws = samples;
    data->seed = seed;
    bt_mf_draw_counts_free(&counts);
    BT_TRACE_EXIT("mf.count_frequencies");
    return data;
}

char **bt_mf_zero_hit_cases(const bt_ga_freq_data *data, size_t *out_count) {
    BT_TRACE_ENTER("mf.zero_hit_cases");
    size_t misses = 0;
    for (size_t i = 0; i < data->cases_count; i++) {
        if (data->cases[i].count == 0) {
            misses++;
        }
    }
    *out_count = misses;
    if (misses == 0) {
        BT_TRACE_EXIT("mf.zero_hit_cases");
        return NULL;
    }
    char **items = malloc(misses * sizeof *items);
    if (items == NULL) {
        *out_count = 0;
        BT_TRACE_FAIL("mf.zero_hit_cases", "allocation failed for %zu misses", misses);
        return NULL;
    }
    size_t at = 0;
    for (size_t i = 0; i < data->cases_count; i++) {
        if (data->cases[i].count == 0) {
            char text[32];
            snprintf(text, sizeof text, "%s/%s", data->cases[i].orbit, data->cases[i].letters);
            items[at] = _strdup(text);
            at++;
        }
    }
    BT_TRACE_EXIT("mf.zero_hit_cases");
    return items;
}

void bt_mf_str_list_free(char **items, size_t count) {
    BT_TRACE_ENTER("mf.str_list_free");
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
    BT_TRACE_EXIT("mf.str_list_free");
}

static int append_indent(bt_strbuf *out, size_t depth) {
    for (size_t i = 0; i < depth; i++) {
        if (bt_strbuf_append_str(out, "  ") != 0) {
            return -1;
        }
    }
    return 0;
}

static int emit_string(bt_strbuf *out, const char *text) {
    if (bt_strbuf_append_str(out, "\"") != 0) {
        return -1;
    }
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        unsigned char c = (unsigned char)*cursor;
        const char *escape = NULL;
        if (c == '"') {
            escape = "\\\"";
        } else if (c == '\\') {
            escape = "\\\\";
        } else if (c < 0x20) {
            char buffer[7];
            snprintf(buffer, sizeof buffer, "\\u%04x", c);
            if (bt_strbuf_append_str(out, buffer) != 0) {
                return -1;
            }
            continue;
        }
        if (escape != NULL) {
            if (bt_strbuf_append_str(out, escape) != 0) {
                return -1;
            }
            continue;
        }
        if (bt_strbuf_append(out, cursor, 1) != 0) {
            return -1;
        }
    }
    return bt_strbuf_append_str(out, "\"") == 0 ? 0 : -1;
}

static int emit_case_rows(const bt_ga_case_row *rows, size_t count, bt_strbuf *out) {
    if (count == 0) {
        return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
    }
    if (bt_strbuf_append_str(out, "[\n") != 0) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (append_indent(out, 2) != 0 || bt_strbuf_append_str(out, "{\n") != 0 ||
            append_indent(out, 3) != 0 || bt_strbuf_append_str(out, "\"orbit\": ") != 0 ||
            emit_string(out, rows[i].orbit) != 0 || bt_strbuf_append_str(out, ",\n") != 0 ||
            append_indent(out, 3) != 0 || bt_strbuf_append_str(out, "\"letters\": ") != 0 ||
            emit_string(out, rows[i].letters) != 0 || bt_strbuf_append_str(out, ",\n") != 0 ||
            append_indent(out, 3) != 0 ||
            bt_strbuf_appendf(out, "\"count\": %llu", (unsigned long long)rows[i].count) != 0 ||
            bt_strbuf_append_str(out, "\n") != 0 || append_indent(out, 2) != 0 ||
            bt_strbuf_append_str(out, "}") != 0) {
            return -1;
        }
        if (i + 1 < count && bt_strbuf_append_str(out, ",") != 0) {
            return -1;
        }
        if (bt_strbuf_append_str(out, "\n") != 0) {
            return -1;
        }
    }
    if (append_indent(out, 1) != 0 || bt_strbuf_append_str(out, "]") != 0) {
        return -1;
    }
    return 0;
}

static int emit_pair_rows(const bt_ga_pair_count *rows, size_t count, bt_strbuf *out) {
    if (count == 0) {
        return bt_strbuf_append_str(out, "[]") == 0 ? 0 : -1;
    }
    if (bt_strbuf_append_str(out, "[\n") != 0) {
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (append_indent(out, 2) != 0 || bt_strbuf_append_str(out, "{\n") != 0 ||
            append_indent(out, 3) != 0 ||
            bt_strbuf_appendf(out, "\"from\": %u", (unsigned)rows[i].from) != 0 ||
            bt_strbuf_append_str(out, ",\n") != 0 || append_indent(out, 3) != 0 ||
            bt_strbuf_appendf(out, "\"to\": %u", (unsigned)rows[i].to) != 0 ||
            bt_strbuf_append_str(out, ",\n") != 0 || append_indent(out, 3) != 0 ||
            bt_strbuf_appendf(out, "\"count\": %llu", (unsigned long long)rows[i].count) != 0 ||
            bt_strbuf_append_str(out, "\n") != 0 || append_indent(out, 2) != 0 ||
            bt_strbuf_append_str(out, "}") != 0) {
            return -1;
        }
        if (i + 1 < count && bt_strbuf_append_str(out, ",") != 0) {
            return -1;
        }
        if (bt_strbuf_append_str(out, "\n") != 0) {
            return -1;
        }
    }
    if (append_indent(out, 1) != 0 || bt_strbuf_append_str(out, "]") != 0) {
        return -1;
    }
    return 0;
}

int bt_mf_freq_data_text(const bt_ga_freq_data *data, bt_strbuf *out) {
    BT_TRACE_ENTER("mf.freq_data_text");
    int rc = bt_strbuf_append_str(out, "{\n") == 0 && append_indent(out, 1) == 0 &&
             bt_strbuf_append_str(out, "\"cases\": ") == 0 &&
             emit_case_rows(data->cases, data->cases_count, out) == 0 &&
             bt_strbuf_append_str(out, ",\n") == 0 && append_indent(out, 1) == 0 &&
             bt_strbuf_append_str(out, "\"adjacent\": ") == 0 &&
             emit_pair_rows(data->adjacent, data->adjacent_count, out) == 0 &&
             bt_strbuf_append_str(out, ",\n") == 0 && append_indent(out, 1) == 0 &&
             bt_strbuf_append_str(out, "\"cooccurring\": ") == 0 &&
             emit_pair_rows(data->cooccurring, data->cooccurring_count, out) == 0 &&
             bt_strbuf_append_str(out, ",\n") == 0 && append_indent(out, 1) == 0 &&
             bt_strbuf_appendf(out, "\"total_draws\": %llu",
                               (unsigned long long)data->total_draws) == 0 &&
             bt_strbuf_append_str(out, ",\n") == 0 && append_indent(out, 1) == 0 &&
             bt_strbuf_appendf(out, "\"seed\": %llu", (unsigned long long)data->seed) == 0 &&
             bt_strbuf_append_str(out, "\n}") == 0;
    if (rc) {
        BT_TRACE_EXIT("mf.freq_data_text");
        return 0;
    }
    BT_TRACE_FAIL("mf.freq_data_text", "serialization failed");
    return -1;
}
