#include "memofreq/lib.h"

#include "common/bt_harness.h"
#include "data/paths.h"
#include "ga/dist.h"
#include "ga/inventory.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scramble.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sorted_all_distinct(const uint64_t *values, size_t count) {
    for (size_t i = 1; i < count; i++) {
        if (values[i - 1] >= values[i]) {
            return 0;
        }
    }
    return 1;
}

static int cmp_u64(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    if (a < b) {
        return -1;
    }
    return a > b ? 1 : 0;
}

BT_TEST(memofreq_lib, data_ga_path_root_only_keeps_the_drive) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    BT_CHECK(root[1] == ':');
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%2.2s/zz-ga", root);
    char out[BT_PATH_MAX];
    bt_mf_data_ga_path("/zz-ga", out);
    BT_CHECK(strcmp(out, expected) == 0);
    bt_mf_data_ga_path("C:ga", out);
    BT_CHECK(strcmp(out, "C:ga") == 0);
}

BT_TEST(memofreq_lib, draw_seed_is_splitmix_stable) {
    BT_CHECK_EQ(bt_mf_draw_seed(20261012, 0), 16176770643486239525ULL);
    BT_CHECK_EQ(bt_mf_draw_seed(20261012, 1), 10310797173907234146ULL);
    BT_CHECK_EQ(bt_mf_draw_seed(0, 42), 1391454601869358542ULL);
}

BT_TEST(memofreq_lib, pair_index_covers_triangle_without_gaps) {
    unsigned char seen[15];
    memset(seen, 0, sizeof seen);
    for (size_t a = 0; a < 5; a++) {
        for (size_t b = 0; b < 5; b++) {
            seen[bt_mf_pair_index(a, b)] = 1;
        }
    }
    for (size_t i = 0; i < 15; i++) {
        BT_CHECK(seen[i]);
    }
    BT_CHECK_EQ(bt_mf_pair_index(3, 1), bt_mf_pair_index(1, 3));
    BT_CHECK_EQ(bt_mf_pair_index(2, 2), (size_t)5);
}

BT_TEST(memofreq_lib, counts_match_a_sequential_replay) {
    const uint64_t samples = 250;
    const uint64_t seed = 777;
    bt_ga_freq_data *data = bt_mf_count_frequencies(samples, seed);
    BT_CHECK(data != NULL);
    bt_mf_orbit_tables tables;
    bt_mf_orbit_tables_build(&tables);
    uint64_t *case_counts = calloc(bt_mf_total_cases(), sizeof *case_counts);
    uint64_t *edge_adj = calloc(bt_mf_triangle(bt_mf_edge_count()), sizeof *edge_adj);
    uint64_t *corner_adj = calloc(bt_mf_triangle(bt_mf_corner_count()), sizeof *corner_adj);
    uint64_t *edge_cooc = calloc(bt_mf_triangle(bt_mf_edge_count()), sizeof *edge_cooc);
    uint64_t *corner_cooc = calloc(bt_mf_triangle(bt_mf_corner_count()), sizeof *corner_cooc);
    for (uint64_t draw = 0; draw < samples; draw++) {
        uint8_t occ[54];
        bt_scheme_random_occ(bt_mf_draw_seed(seed, draw), occ);
        bt_memo memo;
        bt_scheme_memo_from_array(occ, &memo);
        const bt_case *streams[2] = {memo.edge_pairs, memo.corner_pairs};
        const size_t stream_counts[2] = {memo.edge_pair_count, memo.corner_pair_count};
        const size_t bases[2] = {0, bt_mf_edge_count()};
        uint64_t *adjs[2] = {edge_adj, corner_adj};
        uint64_t *coocs[2] = {edge_cooc, corner_cooc};
        for (int stream = 0; stream < 2; stream++) {
            size_t indices[BT_MEMO_MAX_PAIRS];
            for (size_t i = 0; i < stream_counts[stream]; i++) {
                indices[i] =
                    (size_t)tables.index[streams[stream][i].t1 * 48 + streams[stream][i].t2];
            }
            bt_mf_add_stream(case_counts, bases[stream], adjs[stream], coocs[stream], indices,
                             stream_counts[stream]);
        }
    }
    for (size_t i = 0; i < data->cases_count; i++) {
        if (data->cases[i].count != case_counts[i]) {
            bt_harness_fail(__FILE__, __LINE__, "case count mismatch at %s: %llu vs %llu",
                            data->cases[i].letters, (unsigned long long)data->cases[i].count,
                            (unsigned long long)case_counts[i]);
        }
    }
    size_t replay_adj_count = 0;
    size_t replay_cooc_count = 0;
    bt_ga_pair_count *replay_adj_edge =
        bt_mf_drain_pairs(edge_adj, bt_mf_triangle(bt_mf_edge_count()), 0, &replay_adj_count);
    size_t edge_adj_count = replay_adj_count;
    bt_ga_pair_count *replay_adj_corner = bt_mf_drain_pairs(
        corner_adj, bt_mf_triangle(bt_mf_corner_count()), bt_mf_edge_count(), &replay_adj_count);
    bt_ga_pair_count *replay_cooc_edge =
        bt_mf_drain_pairs(edge_cooc, bt_mf_triangle(bt_mf_edge_count()), 0, &replay_cooc_count);
    size_t edge_cooc_count = replay_cooc_count;
    bt_ga_pair_count *replay_cooc_corner = bt_mf_drain_pairs(
        corner_cooc, bt_mf_triangle(bt_mf_corner_count()), bt_mf_edge_count(), &replay_cooc_count);
    BT_CHECK_EQ(data->adjacent_count, edge_adj_count + replay_adj_count);
    BT_CHECK_EQ(data->cooccurring_count, edge_cooc_count + replay_cooc_count);
    for (size_t i = 0; i < data->adjacent_count; i++) {
        const bt_ga_pair_count *want =
            i < edge_adj_count ? &replay_adj_edge[i] : &replay_adj_corner[i - edge_adj_count];
        if (data->adjacent[i].from != want->from || data->adjacent[i].to != want->to ||
            data->adjacent[i].count != want->count) {
            bt_harness_fail(__FILE__, __LINE__, "adjacent mismatch at %zu", i);
        }
    }
    for (size_t i = 0; i < data->cooccurring_count; i++) {
        const bt_ga_pair_count *want =
            i < edge_cooc_count ? &replay_cooc_edge[i] : &replay_cooc_corner[i - edge_cooc_count];
        if (data->cooccurring[i].from != want->from || data->cooccurring[i].to != want->to ||
            data->cooccurring[i].count != want->count) {
            bt_harness_fail(__FILE__, __LINE__, "cooccurring mismatch at %zu", i);
        }
    }
    free(replay_adj_edge);
    free(replay_adj_corner);
    free(replay_cooc_edge);
    free(replay_cooc_corner);
    free(case_counts);
    free(edge_adj);
    free(corner_adj);
    free(edge_cooc);
    free(corner_cooc);
    bt_ga_freq_data_free(data);
    free(data);
}

BT_TEST(memofreq_lib, output_is_byte_identical_across_runs) {
    bt_ga_freq_data *a = bt_mf_count_frequencies(400, 20261012);
    bt_ga_freq_data *b = bt_mf_count_frequencies(400, 20261012);
    BT_CHECK(a != NULL && b != NULL);
    bt_strbuf bytes_a;
    bt_strbuf bytes_b;
    bt_strbuf_init(&bytes_a, 65536);
    bt_strbuf_init(&bytes_b, 65536);
    BT_CHECK_EQ(bt_mf_freq_data_text(a, &bytes_a), 0);
    BT_CHECK_EQ(bt_mf_freq_data_text(b, &bytes_b), 0);
    BT_CHECK_EQ(bytes_a.len, bytes_b.len);
    BT_CHECK(memcmp(bytes_a.data, bytes_b.data, bytes_a.len) == 0);
    char sha_a[BT_GA_SHA256_HEX_LEN];
    char sha_b[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(bytes_a.data, bytes_a.len, sha_a);
    bt_ga_sha256_hex(bytes_b.data, bytes_b.len, sha_b);
    BT_CHECK(strcmp(sha_a, sha_b) == 0);
    bt_strbuf_free(&bytes_a);
    bt_strbuf_free(&bytes_b);
    bt_ga_freq_data_free(a);
    bt_ga_freq_data_free(b);
    free(a);
    free(b);
}

BT_TEST(memofreq_lib, every_case_is_hit_over_a_long_run) {
    bt_ga_freq_data *data = bt_mf_count_frequencies(30000, 20261012);
    BT_CHECK(data != NULL);
    size_t zero_count = 0;
    char **zeros = bt_mf_zero_hit_cases(data, &zero_count);
    BT_CHECK_EQ(zero_count, (size_t)0);
    BT_CHECK_EQ(data->cases_count, bt_mf_total_cases());
    BT_CHECK_EQ(data->total_draws, (uint64_t)30000);
    bt_mf_str_list_free(zeros, zero_count);
    bt_ga_freq_data_free(data);
    free(data);
}

BT_TEST(memofreq_lib, zero_hit_checker_lists_misses) {
    bt_ga_case_row rows[3];
    snprintf(rows[0].orbit, sizeof rows[0].orbit, "edge");
    snprintf(rows[0].letters, sizeof rows[0].letters, "AB");
    rows[0].count = 5;
    snprintf(rows[1].orbit, sizeof rows[1].orbit, "edge");
    snprintf(rows[1].letters, sizeof rows[1].letters, "AD");
    rows[1].count = 0;
    snprintf(rows[2].orbit, sizeof rows[2].orbit, "corner");
    snprintf(rows[2].letters, sizeof rows[2].letters, "AB");
    rows[2].count = 0;
    bt_ga_freq_data data = {0};
    data.cases = rows;
    data.cases_count = 3;
    data.total_draws = 10;
    data.seed = 1;
    size_t count = 0;
    char **misses = bt_mf_zero_hit_cases(&data, &count);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK_EQ(misses[0], "edge/AD");
    BT_CHECK_EQ(misses[1], "corner/AB");
    bt_mf_str_list_free(misses, count);
}

BT_TEST(memofreq_lib, crate_id_matches_package) {
    BT_CHECK_EQ(bt_mf_tool_id(), "bt-memofreq");
}

BT_TEST(memofreq_lib, orbit_counts_match_the_scheme_enumeration) {
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    BT_CHECK_EQ(bt_mf_edge_count(), bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge));
    BT_CHECK_EQ(bt_mf_corner_count(), bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner));
    BT_CHECK_EQ(bt_mf_total_cases(), bt_mf_edge_count() + bt_mf_corner_count());
}

BT_TEST(memofreq_lib, draw_indices_never_alias_within_a_run) {
    uint64_t drawn[128];
    for (uint64_t i = 0; i < 128; i++) {
        drawn[i] = bt_mf_draw_seed(20261014, i);
    }
    qsort(drawn, 128, sizeof *drawn, cmp_u64);
    BT_CHECK(sorted_all_distinct(drawn, 128));
    uint64_t other[128];
    for (uint64_t i = 0; i < 128; i++) {
        other[i] = bt_mf_draw_seed(1, i);
    }
    qsort(other, 128, sizeof *other, cmp_u64);
    BT_CHECK(sorted_all_distinct(other, 128));
}

BT_TEST(memofreq_lib, add_stream_counts_a_hand_built_stream_exactly) {
    uint64_t cases[8] = {0};
    uint64_t adj[36] = {0};
    uint64_t cooc[36] = {0};
    const size_t stream[] = {0, 2, 1, 2};
    bt_mf_add_stream(cases, 0, adj, cooc, stream, 4);
    const uint64_t want_cases[8] = {1, 1, 2, 0, 0, 0, 0, 0};
    BT_CHECK(memcmp(cases, want_cases, sizeof want_cases) == 0);
    uint64_t want_adj[36] = {0};
    want_adj[bt_mf_pair_index(0, 2)] = 1;
    want_adj[bt_mf_pair_index(1, 2)] = 2;
    BT_CHECK(memcmp(adj, want_adj, sizeof want_adj) == 0);
    uint64_t want_cooc[36] = {0};
    want_cooc[bt_mf_pair_index(0, 1)] = 1;
    want_cooc[bt_mf_pair_index(0, 2)] = 2;
    want_cooc[bt_mf_pair_index(1, 2)] = 2;
    want_cooc[bt_mf_pair_index(2, 2)] = 1;
    BT_CHECK(memcmp(cooc, want_cooc, sizeof want_cooc) == 0);
}

BT_TEST(memofreq_lib, add_stream_subtracts_the_base_before_indexing_pairs) {
    uint64_t cases[20] = {0};
    uint64_t adj[210] = {0};
    uint64_t cooc[210] = {0};
    const size_t stream[] = {10, 13, 10};
    bt_mf_add_stream(cases, 10, adj, cooc, stream, 3);
    BT_CHECK_EQ(cases[10], (uint64_t)2);
    BT_CHECK_EQ(cases[13], (uint64_t)1);
    BT_CHECK_EQ(adj[bt_mf_pair_index(0, 3)], (uint64_t)2);
    BT_CHECK_EQ(cooc[bt_mf_pair_index(0, 3)], (uint64_t)2);
    BT_CHECK_EQ(cooc[bt_mf_pair_index(0, 0)], (uint64_t)1);
}

BT_TEST(memofreq_lib, merge_sums_every_table_and_new_is_the_identity) {
    bt_mf_draw_counts a;
    BT_CHECK_EQ(bt_mf_draw_counts_new(&a), 0);
    a.cases[3] = 5;
    a.edge_cooc[bt_mf_pair_index(2, 2)] = 7;
    a.corner_adj[bt_mf_pair_index(1, 4)] = 9;
    bt_mf_draw_counts b;
    BT_CHECK_EQ(bt_mf_draw_counts_new(&b), 0);
    b.cases[3] = 1;
    b.edge_cooc[bt_mf_pair_index(2, 2)] = 2;
    b.corner_adj[bt_mf_pair_index(1, 4)] = 3;
    bt_mf_draw_counts merged;
    BT_CHECK_EQ(bt_mf_draw_counts_clone(&a, &merged), 0);
    bt_mf_draw_counts_merge(&merged, &b);
    BT_CHECK_EQ(merged.cases[3], (uint64_t)6);
    BT_CHECK_EQ(merged.edge_cooc[bt_mf_pair_index(2, 2)], (uint64_t)9);
    BT_CHECK_EQ(merged.corner_adj[bt_mf_pair_index(1, 4)], (uint64_t)12);
    bt_mf_draw_counts untouched;
    BT_CHECK_EQ(bt_mf_draw_counts_clone(&a, &untouched), 0);
    bt_mf_draw_counts fresh;
    BT_CHECK_EQ(bt_mf_draw_counts_new(&fresh), 0);
    bt_mf_draw_counts_merge(&untouched, &fresh);
    BT_CHECK_EQ(untouched.cases[3], (uint64_t)5);
    bt_mf_draw_counts zeroed;
    BT_CHECK_EQ(bt_mf_draw_counts_new(&zeroed), 0);
    bt_mf_draw_counts_merge(&zeroed, &b);
    BT_CHECK_EQ(zeroed.corner_adj[bt_mf_pair_index(1, 4)], (uint64_t)3);
    bt_mf_draw_counts_free(&fresh);
    bt_mf_draw_counts_free(&untouched);
    bt_mf_draw_counts_free(&merged);
    bt_mf_draw_counts_free(&b);
    bt_mf_draw_counts_free(&a);
    bt_mf_draw_counts_free(&zeroed);
}

BT_TEST(memofreq_lib, row_and_col_invert_pair_index_over_the_triangle) {
    for (size_t hi = 0; hi < 40; hi++) {
        for (size_t lo = 0; lo <= hi; lo++) {
            size_t index = bt_mf_pair_index(lo, hi);
            BT_CHECK_EQ(bt_mf_row_of(index), hi);
            BT_CHECK_EQ(bt_mf_col_of(index), lo);
        }
    }
    BT_CHECK_EQ(bt_mf_row_of(bt_mf_triangle(4) - 1), (size_t)3);
    BT_CHECK_EQ(bt_mf_col_of(bt_mf_triangle(4) - 1), (size_t)3);
}

BT_TEST(memofreq_lib, drain_pairs_offsets_indices_and_drops_zero_cells) {
    uint64_t counts[10] = {0};
    counts[bt_mf_pair_index(0, 0)] = 2;
    counts[bt_mf_pair_index(1, 3)] = 7;
    size_t drained_count = 0;
    bt_ga_pair_count *drained = bt_mf_drain_pairs(counts, 10, 5, &drained_count);
    BT_CHECK_EQ(drained_count, (size_t)2);
    BT_CHECK_EQ(drained[0].from, (uint32_t)5);
    BT_CHECK_EQ(drained[0].to, (uint32_t)5);
    BT_CHECK_EQ(drained[0].count, (uint64_t)2);
    BT_CHECK_EQ(drained[1].from, (uint32_t)8);
    BT_CHECK_EQ(drained[1].to, (uint32_t)6);
    BT_CHECK_EQ(drained[1].count, (uint64_t)7);
    free(drained);
    uint64_t zeros[10] = {0};
    size_t empty_count = 99;
    bt_ga_pair_count *empty = bt_mf_drain_pairs(zeros, 10, 9, &empty_count);
    BT_CHECK_EQ(empty_count, (size_t)0);
    BT_CHECK(empty == NULL || empty_count == 0);
    free(empty);
}

BT_TEST(memofreq_lib, zero_samples_reports_an_all_zero_catalog) {
    bt_ga_freq_data *data = bt_mf_count_frequencies(0, 7);
    BT_CHECK(data != NULL);
    BT_CHECK_EQ(data->total_draws, (uint64_t)0);
    BT_CHECK_EQ(data->seed, (uint64_t)7);
    BT_CHECK_EQ(data->cases_count, bt_mf_total_cases());
    for (size_t i = 0; i < data->cases_count; i++) {
        if (data->cases[i].count != 0) {
            bt_harness_fail(__FILE__, __LINE__, "case %s should be zero", data->cases[i].letters);
        }
    }
    BT_CHECK_EQ(data->adjacent_count, (size_t)0);
    BT_CHECK_EQ(data->cooccurring_count, (size_t)0);
    size_t misses_count = 0;
    char **misses = bt_mf_zero_hit_cases(data, &misses_count);
    BT_CHECK_EQ(misses_count, bt_mf_total_cases());
    BT_CHECK_EQ(misses[0], "edge/AB");
    BT_CHECK_EQ(misses[bt_mf_edge_count()], "corner/AB");
    BT_CHECK_EQ(misses[bt_mf_total_cases() - 1], "corner/XW");
    bt_mf_str_list_free(misses, misses_count);
    bt_ga_freq_data_free(data);
    free(data);
}

BT_TEST(memofreq_lib, case_catalog_follows_the_scheme_enumeration_order) {
    bt_ga_freq_data *data = bt_mf_count_frequencies(3, 5);
    BT_CHECK(data != NULL);
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    BT_CHECK_EQ(edge_count + corner_count, bt_mf_total_cases());
    BT_CHECK_EQ(data->cases_count, bt_mf_total_cases());
    for (size_t i = 0; i < data->cases_count; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        const char *orbit = case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner";
        char letters[2];
        bt_scheme_case_letters(case_id, letters);
        if (strcmp(data->cases[i].orbit, orbit) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "orbit mismatch at %zu: %s vs %s", i,
                            data->cases[i].orbit, orbit);
        }
        if (strncmp(data->cases[i].letters, letters, 2) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "letters mismatch at %zu: %s vs %c%c", i,
                            data->cases[i].letters, letters[0], letters[1]);
        }
    }
    BT_CHECK_EQ(data->total_draws, (uint64_t)3);
    bt_ga_freq_data_free(data);
    free(data);
}
