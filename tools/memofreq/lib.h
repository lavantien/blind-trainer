#ifndef BT_MF_LIB_H
#define BT_MF_LIB_H

#include "common/strbuf.h"
#include "ga/dist.h"
#include "ga/hash.h"
#include "scheme/pairs.h"

#include <stddef.h>
#include <stdint.h>

#define BT_MF_EDGE_COUNT ((size_t)440)
#define BT_MF_CORNER_COUNT ((size_t)378)
#define BT_MF_TOTAL_CASES (BT_MF_EDGE_COUNT + BT_MF_CORNER_COUNT)
#define BT_MF_STICKER_SLOTS ((size_t)48)

const char *bt_mf_tool_id(void);
size_t bt_mf_edge_count(void);
size_t bt_mf_corner_count(void);
size_t bt_mf_total_cases(void);

uint64_t bt_mf_draw_seed(uint64_t seed, uint64_t index);

size_t bt_mf_triangle(size_t size);
size_t bt_mf_pair_index(size_t a, size_t b);
size_t bt_mf_row_of(size_t index);
size_t bt_mf_col_of(size_t index);

void bt_mf_add_stream(uint64_t *cases, size_t base, uint64_t *adj, uint64_t *cooc,
                      const size_t *indices, size_t indices_count);

bt_ga_pair_count *bt_mf_drain_pairs(const uint64_t *counts, size_t counts_count, size_t base,
                                    size_t *out_count);

typedef struct {
    int32_t index[BT_MF_STICKER_SLOTS * BT_MF_STICKER_SLOTS];
    bt_case cases[BT_MF_TOTAL_CASES];
    size_t case_count;
} bt_mf_orbit_tables;

void bt_mf_orbit_tables_build(bt_mf_orbit_tables *tables);

typedef struct {
    uint64_t *cases;
    uint64_t *edge_cooc;
    uint64_t *corner_cooc;
    uint64_t *edge_adj;
    uint64_t *corner_adj;
} bt_mf_draw_counts;

int bt_mf_draw_counts_new(bt_mf_draw_counts *counts);
int bt_mf_draw_counts_clone(const bt_mf_draw_counts *src, bt_mf_draw_counts *dst);
void bt_mf_draw_counts_free(bt_mf_draw_counts *counts);
void bt_mf_draw_counts_merge(bt_mf_draw_counts *self, const bt_mf_draw_counts *other);
void bt_mf_draw_counts_add_draw(bt_mf_draw_counts *counts, const bt_mf_orbit_tables *tables,
                                uint64_t seed);

bt_ga_freq_data *bt_mf_count_frequencies(uint64_t samples, uint64_t seed);

char **bt_mf_zero_hit_cases(const bt_ga_freq_data *data, size_t *out_count);
void bt_mf_str_list_free(char **items, size_t count);

int bt_mf_freq_data_text(const bt_ga_freq_data *data, bt_strbuf *out);

#endif
