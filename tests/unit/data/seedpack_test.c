#include "common.h"
#include "common/bt_harness.h"
#include "data/db.h"
#include "data/paths.h"
#include "data/repo_seeds.h"
#include "data/seedpack/hashing.h"
#include "data/seedpack/mod.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"
#include "data/seedpack/rulings.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

#define HEADER "case_key\talg\tfront_json\tsource_url\tsource_at"

static void td_packs(char out[BT_PATH_MAX]) {
    td_packs_root(out);
}

static void write_pack(const char *dir, const char *registry, const char *file, const char *tsv) {
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/registry.json", dir);
    tu_write_file(path, registry);
    snprintf(path, sizeof path, "%s/%s", dir, file);
    tu_write_file(path, tsv);
}

static void mini_registry(char *out, size_t out_size, const char *rows) {
    snprintf(out, out_size,
             "[{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
             "\"n\":3,\"expected_rows\":%s}]",
             rows);
}

static void mini_tsv(char *out, size_t out_size, const char *const keys[], const char *const algs[],
                     size_t count) {
    size_t used = snprintf(out, out_size, "%s", HEADER);
    for (size_t i = 0; i < count; i++) {
        used += (size_t)snprintf(out + used, out_size - used,
                                 "\n%s\t%s\t{\"case\":\"x\"}\thttps://example.test/a\t2026-10-16",
                                 keys[i], algs[i]);
    }
}

static bt_pack_entry parse_entry_of(const char *registry) {
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_parse_registry(registry, &entries, &count, &err) != 0) {
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        BT_ABORT("registry failed: %s", text);
    }
    bt_pack_entry entry = entries[count - 1];
    bt_seedpack_free_entries(entries);
    return entry;
}

static bt_pack_entry mini_entry(void) {
    char registry[256];
    mini_registry(registry, sizeof registry, "1");
    return parse_entry_of(registry);
}

static bt_pack_entry real_entry(const char *file) {
    char packs[BT_PATH_MAX];
    td_packs(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs, &entries, &count, &err) != 0) {
        BT_ABORT("registry load failed");
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].file, file) == 0) {
            bt_pack_entry entry = entries[i];
            bt_seedpack_free_entries(entries);
            return entry;
        }
    }
    bt_seedpack_free_entries(entries);
    BT_ABORT("registry holds no %s", file);
    abort();
}

static void load_real_pack(const char *file, bt_seed_rows *rows) {
    char packs[BT_PATH_MAX];
    td_packs(packs);
    bt_pack_entry entry = real_entry(file);
    bt_seed_error err;
    if (bt_seedpack_load_pack(packs, &entry, rows, &err) != 0) {
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        BT_ABORT("pack %s failed: %s", file, text);
    }
}

static void expect_pack_error(const bt_seed_error *err, const char *a, const char *b) {
    char text[768];
    bt_seed_error_string(err, text, sizeof text);
    if (a != NULL) {
        BT_CHECK(strstr(text, a) != NULL);
    }
    if (b != NULL) {
        BT_CHECK(strstr(text, b) != NULL);
    }
}

BT_TEST(seedpack, classify_failure_splits_parse_apply_and_tier2) {
    bt_failure_class out;
    BT_CHECK(bt_classify_failure("unknown token: q", false, &out));
    BT_CHECK(out == BT_FAILURE_PARSE);
    BT_CHECK(bt_classify_failure("n=4: tier 1 parse failed: bad", false, &out));
    BT_CHECK(out == BT_FAILURE_PARSE);
    BT_CHECK(bt_classify_failure("illegal move: /", false, &out));
    BT_CHECK(out == BT_FAILURE_APPLY);
    BT_CHECK(bt_classify_failure("start state round trip misses solved", false, &out));
    BT_CHECK(out == BT_FAILURE_APPLY);
    BT_CHECK(bt_classify_failure(NULL, true, &out));
    BT_CHECK(out == BT_FAILURE_TIER2);
    BT_CHECK(bt_classify_failure(NULL, false, &out) == false);
}

BT_TEST(seedpack, failure_class_display_names_are_stable) {
    BT_CHECK_EQ(bt_failure_class_name(BT_FAILURE_PARSE), "parse");
    BT_CHECK_EQ(bt_failure_class_name(BT_FAILURE_APPLY), "apply");
    BT_CHECK_EQ(bt_failure_class_name(BT_FAILURE_TIER2), "tier2");
}

BT_TEST(seedpack, every_ruling_row_pairs_a_class_with_its_reason) {
    size_t count = 0;
    const bt_expected_failure_row *rows = bt_seedpack_expected_failures(&count);
    for (size_t i = 0; i < count; i++) {
        BT_CHECK_EQ(
            bt_expected_failure(rows[i].event, rows[i].method, rows[i].stage, rows[i].case_key),
            rows[i].reason);
        bt_failure_class class;
        BT_CHECK(bt_expected_failure_class(rows[i].event, rows[i].method, rows[i].stage,
                                           rows[i].case_key, &class));
        BT_CHECK(class == rows[i].class);
    }
    bt_failure_class none;
    BT_CHECK(bt_expected_failure_class("sq1", "lbl", "cubeshape", "cs.001", &none) == false);
}

BT_TEST(seedpack, representative_rulings_pin_their_recorded_classes) {
    bt_failure_class out;
    BT_CHECK(bt_expected_failure_class("sq1", "lbl", "cubeshape", "cs.119", &out));
    BT_CHECK(out == BT_FAILURE_APPLY);
    BT_CHECK(bt_expected_failure_class("sq1", "lbl", "cubeshape", "cs.075", &out));
    BT_CHECK(out == BT_FAILURE_TIER2);
    BT_CHECK(bt_expected_failure_class("3bld", "m2", "concept", "m2.18", &out));
    BT_CHECK(out == BT_FAILURE_TIER2);
}

BT_TEST(seedpack, registry_parses_the_real_registry_with_21_rows_and_all_three_n_shapes) {
    char packs[BT_PATH_MAX];
    td_packs(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_load_registry(packs, &entries, &count, &err) == 0);
    BT_CHECK(count == 21);
    if (count != 21) {
        bt_seedpack_free_entries(entries);
        return;
    }
    BT_CHECK(entries[0].n.kind == BT_NSPEC_UNSET);
    const bt_pack_entry *parity = NULL;
    const bt_pack_entry *pll = NULL;
    const bt_pack_entry *oh = NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].file, "bigcube_parity.tsv") == 0) {
            parity = &entries[i];
        }
        if (strcmp(entries[i].event, "3x3") == 0 && strcmp(entries[i].stage, "pll") == 0) {
            pll = &entries[i];
        }
        if (strcmp(entries[i].event, "oh") == 0 && strcmp(entries[i].stage, "pll") == 0) {
            oh = &entries[i];
        }
    }
    BT_CHECK(parity != NULL);
    if (parity != NULL) {
        BT_CHECK(parity->n.kind == BT_NSPEC_SIZES);
        BT_CHECK(parity->n.size_count == 4);
        BT_CHECK(parity->n.sizes[0] == 4 && parity->n.sizes[1] == 5 && parity->n.sizes[2] == 6 &&
                 parity->n.sizes[3] == 7);
    }
    BT_CHECK(pll != NULL);
    if (pll != NULL) {
        BT_CHECK(pll->n.kind == BT_NSPEC_FIXED);
        BT_CHECK(pll->n.fixed == 3);
    }
    BT_CHECK(oh != NULL);
    if (oh != NULL) {
        BT_CHECK_EQ(oh->file, pll->file);
    }
    bt_seedpack_free_entries(entries);
}

BT_TEST(seedpack, registry_rejects_unknown_fields) {
    const char *text =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,"
        "\"expected_rows\":1,\"oops\":true}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
    expect_pack_error(&err, "oops", NULL);
}

BT_TEST(seedpack, registry_rejects_a_malformed_n_value) {
    const char *text =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":\"3\","
        "\"expected_rows\":1}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
    expect_pack_error(&err, "n", NULL);
}

BT_TEST(seedpack, registry_rejects_a_missing_expected_rows_field) {
    const char *text =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
}

BT_TEST(seedpack, registry_rejects_a_path_separator_in_the_file_name) {
    const char *text =
        "[{\"file\":\"../escape.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\","
        "\"n\":null,\"expected_rows\":1}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
    expect_pack_error(&err, "file", NULL);
}

BT_TEST(seedpack, registry_rejects_the_dotdot_file_name) {
    const char *text =
        "[{\"file\":\"..\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,"
        "\"expected_rows\":1}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
    expect_pack_error(&err, "bare name", NULL);
}

BT_TEST(seedpack, registry_rejects_a_duplicate_event_method_stage_triplet) {
    const char *text =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,"
        "\"expected_rows\":1},{\"file\":\"b.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\","
        "\"n\":null,\"expected_rows\":1}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(text, &entries, &count, &err) != 0);
    expect_pack_error(&err, "s", NULL);
}

BT_TEST(seedpack, registry_rejects_an_empty_event_or_method_field) {
    const char *empty_event =
        "[{\"file\":\"a.tsv\",\"event\":\"\",\"method\":\"m\",\"stage\":\"s\",\"n\":null,"
        "\"expected_rows\":1}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(empty_event, &entries, &count, &err) != 0);
    expect_pack_error(&err, "empty event, method, or stage", NULL);
    const char *empty_method =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"\",\"stage\":\"s\",\"n\":null,"
        "\"expected_rows\":1}]";
    BT_CHECK(bt_seedpack_parse_registry(empty_method, &entries, &count, &err) != 0);
    expect_pack_error(&err, "empty event, method, or stage", NULL);
}

BT_TEST(seedpack, registry_rejects_zero_expected_rows_and_out_of_range_n) {
    char registry[256];
    mini_registry(registry, sizeof registry, "0");
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(registry, &entries, &count, &err) != 0);
    const char *negative =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":-3,"
        "\"expected_rows\":1}]";
    BT_CHECK(bt_seedpack_parse_registry(negative, &entries, &count, &err) != 0);
    const char *sizes =
        "[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":[4,9],"
        "\"expected_rows\":1}]";
    BT_CHECK(bt_seedpack_parse_registry(sizes, &entries, &count, &err) != 0);
}

BT_TEST(seedpack, tsv_rejects_a_ragged_row_with_four_columns) {
    const char *dir = tu_temp_dir("bt-seedpack", "ragged4");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv", HEADER "\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "5", "mini.tsv");
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_a_ragged_row_with_six_columns) {
    const char *dir = tu_temp_dir("bt-seedpack", "ragged6");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv",
               HEADER "\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a\t2026-10-16\textra");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_a_renamed_header) {
    const char *dir = tu_temp_dir("bt-seedpack", "header");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv",
               "key\talg\tfront_json\tsource_url\tsource_at\na1\tR U\t{\"case\":\"x\"}\t"
               "https://example.test/a\t2026-10-16");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "header", NULL);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_row_count_drift_in_both_directions) {
    const char *dir = tu_temp_dir("bt-seedpack", "drift-count");
    char registry[256];
    mini_registry(registry, sizeof registry, "2");
    char tsv[512];
    const char *keys[] = {"a1"};
    const char *algs[] = {"R U"};
    mini_tsv(tsv, sizeof tsv, keys, algs, 1);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_pack_entry entry = parse_entry_of(registry);
    bt_seed_error err;
    bt_seed_rows rows;
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "expected 2", "found 1");
    const char *keys2[] = {"a1", "a2"};
    const char *algs2[] = {"R U", "R U'"};
    mini_tsv(tsv, sizeof tsv, keys2, algs2, 2);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_pack_entry entry_one = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry_one, &rows, &err) != 0);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_a_duplicate_case_key) {
    const char *dir = tu_temp_dir("bt-seedpack", "dup-key");
    char registry[256];
    mini_registry(registry, sizeof registry, "2");
    char tsv[512];
    const char *keys[] = {"a1", "a1"};
    const char *algs[] = {"R U", "R U'"};
    mini_tsv(tsv, sizeof tsv, keys, algs, 2);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_pack_entry entry = parse_entry_of(registry);
    bt_seed_error err;
    bt_seed_rows rows;
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "a1", NULL);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_front_json_that_is_not_an_object) {
    const char *dir = tu_temp_dir("bt-seedpack", "bad-json");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv", HEADER "\na1\tR U\t[1,2]\thttps://example.test/a\t2026-10-16");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_an_empty_or_padded_case_key_and_a_bad_source_date) {
    const char *dir = tu_temp_dir("bt-seedpack", "bad-key");
    char registry[256];
    mini_registry(registry, sizeof registry, "1");
    write_pack(dir, registry, "mini.tsv",
               HEADER "\n a1 \tR U\t{\"case\":\"x\"}\thttps://example.test/a\t2026-10-16");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    write_pack(dir, registry, "mini.tsv",
               HEADER "\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a\t2026/10/16");
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_pins_the_physical_line_number_of_a_ragged_row) {
    const char *dir = tu_temp_dir("bt-seedpack", "ragged-line");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv", HEADER "\n\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "line 3", NULL);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, tsv_rejects_a_short_source_date) {
    const char *dir = tu_temp_dir("bt-seedpack", "short-date");
    write_pack(dir,
               "{\"file\":\"mini.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"t\","
               "\"n\":3,\"expected_rows\":1}",
               "mini.tsv", HEADER "\na1\tR U\t{\"case\":\"x\"}\thttps://example.test/a\t1234-56-7");
    bt_seed_error err;
    bt_seed_rows rows;
    bt_pack_entry entry = mini_entry();
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    expect_pack_error(&err, "source_at", NULL);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, whitespace_and_comma_variants_hash_identically) {
    bt_pack_entry entry = mini_entry();
    bt_seed_rows base;
    bt_seed_rows spaced;
    bt_seed_rows grouped;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR, U\t{\"case\":\"x\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &base, &err) == 0);
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR   U\t{\"case\":\"x\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &spaced, &err) == 0);
    BT_CHECK_EQ(base.items[0].content_hash, spaced.items[0].content_hash);
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\t(R U)\t{\"case\":\"x\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &grouped, &err) == 0);
    BT_CHECK_EQ(base.items[0].content_hash, grouped.items[0].content_hash);
    bt_seed_rows_free(&base);
    bt_seed_rows_free(&spaced);
    bt_seed_rows_free(&grouped);
}

BT_TEST(seedpack, canonical_hash_pins_the_reference_vector) {
    bt_seed_rows rows;
    load_real_pack("sq1_cubeshape.tsv", &rows);
    const bt_seed_row *row = NULL;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.items[i].case_key, "cs.002") == 0) {
            row = &rows.items[i];
            break;
        }
    }
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->content_hash,
                    "e43d1ec256ede1a9fe0878cf40fd4fd9f022a11f154addf3c5297af5ad4fa805");
    }
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack, hash_covers_every_field) {
    bt_pack_entry entry = mini_entry();
    bt_seed_error err;
    bt_seed_rows base;
    bt_seed_rows other_url;
    bt_seed_rows other_json;
    bt_seed_rows other_alg;
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR U\t{\"case\":\"x\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &base, &err) == 0);
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR U\t{\"case\":\"x\"}\t"
                                           "https://example.test/b\t2026-10-16",
                                    &other_url, &err) == 0);
    BT_CHECK(strcmp(base.items[0].content_hash, other_url.items[0].content_hash) != 0);
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR U\t{\"case\":\"y\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &other_json, &err) == 0);
    BT_CHECK(strcmp(base.items[0].content_hash, other_json.items[0].content_hash) != 0);
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR U'\t{\"case\":\"x\"}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &other_alg, &err) == 0);
    BT_CHECK(strcmp(base.items[0].content_hash, other_alg.items[0].content_hash) != 0);
    bt_seed_rows_free(&base);
    bt_seed_rows_free(&other_url);
    bt_seed_rows_free(&other_json);
    bt_seed_rows_free(&other_alg);
}

BT_TEST(seedpack, enriched_front_json_sorts_keys_and_preserves_source_fields) {
    bt_seed_rows rows;
    load_real_pack("sq1_ep.tsv", &rows);
    const bt_seed_row *row = NULL;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.items[i].case_key, "ep.19") == 0) {
            row = &rows.items[i];
            break;
        }
    }
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->front_json,
                    "{\"case\":\"W-Adj\",\"loader_notes\":[\"equator marker stripped: *\","
                    "\"auf prefix stripped: (U)\"]}");
        BT_CHECK_EQ(row->alg, "(1,0) / (3,0) / (-1,-1) / (3,0) / (-3,0) / (1,1) / (-3,0) / (5,0)");
    }
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack, front_json_rejects_a_loader_notes_collision) {
    bt_pack_entry entry = mini_entry();
    bt_seed_error err;
    bt_seed_rows rows;
    BT_CHECK(bt_seedpack_parse_pack(&entry,
                                    HEADER "\na1\tR U\t{\"loader_notes\":[]}\t"
                                           "https://example.test/a\t2026-10-16",
                                    &rows, &err) != 0);
    expect_pack_error(&err, "loader_notes", NULL);
}

BT_TEST(seedpack, every_real_pack_loads_with_its_declared_row_count_and_stable_hashes) {
    static char seen_keys[1115][224];
    size_t seen = 0;
    char packs[BT_PATH_MAX];
    td_packs(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_load_registry(packs, &entries, &count, &err) == 0);
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        bt_seed_rows rows;
        BT_CHECK(bt_seedpack_load_pack(packs, &entries[i], &rows, &err) == 0);
        BT_CHECK(rows.count == entries[i].expected_rows);
        total += entries[i].expected_rows;
        for (size_t r = 0; r < rows.count; r++) {
            char key[224];
            snprintf(key, sizeof key, "%s/%s/%s/%s", rows.items[r].event, rows.items[r].method,
                     rows.items[r].stage, rows.items[r].case_key);
            for (size_t k = 0; k < seen; k++) {
                BT_CHECK(strcmp(seen_keys[k], key) != 0);
            }
            snprintf(seen_keys[seen], sizeof seen_keys[seen], "%s", key);
            seen += 1;
        }
        bt_seed_rows_free(&rows);
    }
    BT_CHECK(total == 1115);
    BT_CHECK(seen == 1115);
    for (size_t i = 0; i < count; i++) {
        bt_seed_rows first;
        BT_CHECK(bt_seedpack_load_pack(packs, &entries[i], &first, &err) == 0);
        bt_seed_rows second;
        BT_CHECK(bt_seedpack_load_pack(packs, &entries[i], &second, &err) == 0);
        BT_CHECK(first.count == second.count);
        for (size_t r = 0; r < first.count && r < second.count; r++) {
            BT_CHECK_EQ(first.items[r].content_hash, second.items[r].content_hash);
        }
        bt_seed_rows_free(&first);
        bt_seed_rows_free(&second);
    }
    bt_seedpack_free_entries(entries);
}

BT_TEST(seedpack, expected_failure_marks_exactly_the_nine_ruled_rows) {
    struct {
        const char *event;
        const char *method;
        const char *stage;
        const char *key;
    } expected[9] = {
        {"sq1", "lbl", "cubeshape", "cs.119"}, {"sq1", "lbl", "cubeshape", "cs.075"},
        {"sq1", "lbl", "cubeshape", "cs.078"}, {"sq1", "lbl", "cubeshape", "cs.084"},
        {"sq1", "lbl", "cubeshape", "cs.089"}, {"sq1", "lbl", "cubeshape", "cs.097"},
        {"sq1", "lbl", "cubeshape", "cs.109"}, {"sq1", "lbl", "cubeshape", "cs.118"},
        {"sq1", "lbl", "cubeshape", "cs.129"},
    };
    for (int i = 0; i < 9; i++) {
        const char *reason = bt_expected_failure(expected[i].event, expected[i].method,
                                                 expected[i].stage, expected[i].key);
        BT_CHECK(reason != NULL);
        BT_CHECK(reason != NULL && reason[0] != '\0');
    }
    BT_CHECK(bt_expected_failure("sq1", "lbl", "cubeshape", "cs.119x") == NULL);
    BT_CHECK(bt_expected_failure("sq1", "lbl", "cubeshape", "cs.001") == NULL);
    BT_CHECK(bt_expected_failure("sq1", "lbl", "cubeshape", "cs.122") == NULL);
    BT_CHECK(bt_expected_failure("sq1", "lbl", "ep", "ep.19") == NULL);
    BT_CHECK(bt_expected_failure("3x3", "cfop", "f2l", "f2l.fr.07") == NULL);
    BT_CHECK(bt_expected_failure("3bld", "old_pochmann", "concept", "op.09") == NULL);
    bt_seed_rows rows;
    load_real_pack("sq1_cubeshape.tsv", &rows);
    size_t marked = 0;
    for (size_t i = 0; i < rows.count; i++) {
        if (bt_expected_failure(rows.items[i].event, rows.items[i].method, rows.items[i].stage,
                                rows.items[i].case_key) != NULL) {
            marked += 1;
        }
    }
    BT_CHECK(marked == 9);
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack, packs_root_resolves_from_the_hub_and_the_env_override) {
    bt_json_value *config = bt_json_new_object();
    bt_json_value *paths = bt_json_new_object();
    bt_json_object_set(paths, "packs", bt_json_new_string("ref_ground/packs"));
    bt_json_object_set(config, "paths", paths);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    bt_paths_error err;
    char out[BT_PATH_MAX];
    BT_CHECK(bt_data_packs_root_from(NULL, config, out, &err) == 0);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/ref_ground/packs", root);
    BT_CHECK(strcmp(out, want) == 0);
    char absolute[BT_TU_PATH_MAX];
    const char *temp = tu_temp_dir("bt-seedpack", "rg");
    snprintf(absolute, sizeof absolute, "%s", temp);
    BT_CHECK(bt_data_packs_root_from(absolute, config, out, &err) == 0);
    snprintf(want, sizeof want, "%s/packs", absolute);
    BT_CHECK(strcmp(out, want) == 0);
    bt_json_value *missing_paths = bt_json_new_object();
    bt_json_object_set(missing_paths, "paths", bt_json_new_object());
    BT_CHECK(bt_data_packs_root_from(NULL, missing_paths, out, &err) != 0);
    char text[256];
    bt_paths_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "paths.packs") != NULL);
    bt_json_free(config);
    bt_json_free(missing_paths);
    tu_remove_tree(temp);
}

BT_TEST(seedpack, seed_all_loads_every_real_row_and_a_rerun_verifies_without_duplicates) {
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    char packs[BT_PATH_MAX];
    td_packs(packs);
    bt_seed_report *reports = NULL;
    size_t report_count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, packs, &reports, &report_count, &err) == 0);
    BT_CHECK(report_count == 21);
    size_t inserted = 0;
    for (size_t i = 0; i < report_count; i++) {
        inserted += reports[i].inserted;
    }
    BT_CHECK(inserted == 1115);
    bt_seed_reports_free(reports);
    reports = NULL;
    BT_CHECK(bt_seed_all(&db, packs, &reports, &report_count, &err) == 0);
    size_t verified = 0;
    for (size_t i = 0; i < report_count; i++) {
        verified += reports[i].verified;
    }
    BT_CHECK(verified == 1115);
    bt_seed_reports_free(reports);
    long long count = 0;
    BT_CHECK(bt_seed_row_count(&db, &count, &err) == 0);
    BT_CHECK(count == 1115);
    bt_seed_rows rows;
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) == 0);
    BT_CHECK(rows.count == 1115);
    const bt_seed_row *cs002 = NULL;
    for (size_t i = 0; i < rows.count; i++) {
        if (strcmp(rows.items[i].case_key, "cs.002") == 0) {
            cs002 = &rows.items[i];
            break;
        }
    }
    BT_CHECK(cs002 != NULL);
    if (cs002 != NULL) {
        BT_CHECK_EQ(cs002->alg, "/ (3,3) /");
        BT_CHECK_EQ(cs002->event, "sq1");
        BT_CHECK_EQ(cs002->method, "lbl");
        BT_CHECK_EQ(cs002->stage, "cubeshape");
        BT_CHECK_EQ(cs002->source_url, "https://cubingapp.com/algorithms/SQ1-Cube-Shape");
        BT_CHECK_EQ(cs002->source_at, "2026-10-10");
        BT_CHECK_EQ(cs002->content_hash,
                    "e43d1ec256ede1a9fe0878cf40fd4fd9f022a11f154addf3c5297af5ad4fa805");
    }
    bt_seed_rows_free(&rows);
    bt_db_close(&db);
}

BT_TEST(seedpack, a_rerun_of_an_unchanged_pack_reports_only_verifies) {
    const char *dir = tu_temp_dir("bt-seedpack", "noop");
    char registry[256];
    mini_registry(registry, sizeof registry, "1");
    char tsv[512];
    const char *keys[] = {"a1"};
    const char *algs[] = {"R U"};
    mini_tsv(tsv, sizeof tsv, keys, algs, 1);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_pack_entry entry = mini_entry();
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, dir, &entry, &report, &err) == 0);
    BT_CHECK(report.inserted == 1 && report.verified == 0);
    BT_CHECK(bt_seed_pack(&db, dir, &entry, &report, &err) == 0);
    BT_CHECK(report.inserted == 0 && report.verified == 1);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, a_drifted_rerun_fails_hard_naming_the_file_and_row) {
    const char *dir = tu_temp_dir("bt-seedpack", "drift");
    char registry[256];
    mini_registry(registry, sizeof registry, "2");
    char tsv[512];
    const char *keys[] = {"a1", "a2"};
    const char *algs[] = {"R U", "R U'"};
    mini_tsv(tsv, sizeof tsv, keys, algs, 2);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_pack_entry entry = parse_entry_of(registry);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, dir, &entry, &report, &err) == 0);
    const char *algs2[] = {"R U", "R U2"};
    mini_tsv(tsv, sizeof tsv, keys, algs2, 2);
    write_pack(dir, registry, "mini.tsv", tsv);
    BT_CHECK(bt_seed_pack(&db, dir, &entry, &report, &err) != 0);
    char text[768];
    bt_seed_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "mini.tsv") != NULL);
    BT_CHECK(strstr(text, "a2") != NULL);
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(bt_db_conn(&db), "SELECT alg FROM seed_alg WHERE case_key = 'a2'", -1, &stmt,
                       NULL);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(strcmp((const char *)sqlite3_column_text(stmt, 0), "R U'") == 0);
    sqlite3_finalize(stmt);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, a_row_dropped_from_a_pack_fails_hard_naming_the_orphan) {
    const char *dir = tu_temp_dir("bt-seedpack", "orphan");
    char registry[256];
    mini_registry(registry, sizeof registry, "2");
    char tsv[512];
    const char *keys[] = {"a1", "a2"};
    const char *algs[] = {"R U", "R U'"};
    mini_tsv(tsv, sizeof tsv, keys, algs, 2);
    write_pack(dir, registry, "mini.tsv", tsv);
    bt_pack_entry entry = parse_entry_of(registry);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, dir, &entry, &report, &err) == 0);
    char registry_one[256];
    mini_registry(registry_one, sizeof registry_one, "1");
    const char *keys_one[] = {"a1"};
    const char *algs_one[] = {"R U"};
    mini_tsv(tsv, sizeof tsv, keys_one, algs_one, 1);
    write_pack(dir, registry_one, "mini.tsv", tsv);
    bt_pack_entry shrunk = parse_entry_of(registry_one);
    BT_CHECK(bt_seed_pack(&db, dir, &shrunk, &report, &err) != 0);
    char text[768];
    bt_seed_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "a2") != NULL);
    BT_CHECK(strstr(text, "mini.tsv") != NULL);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(seedpack, nspec_eq_distinguishes_every_field) {
    bt_nspec unset_a = {0};
    bt_nspec unset_b = {0};
    BT_CHECK(bt_nspec_eq(&unset_a, &unset_b));
    bt_nspec fixed_a = {BT_NSPEC_FIXED, 4, 0, {0}};
    bt_nspec fixed_b = {BT_NSPEC_FIXED, 4, 0, {0}};
    BT_CHECK(bt_nspec_eq(&fixed_a, &fixed_b));
    bt_nspec fixed_other = {BT_NSPEC_FIXED, 5, 0, {0}};
    BT_CHECK(!bt_nspec_eq(&fixed_a, &fixed_other));
    bt_nspec sizes_a = {BT_NSPEC_SIZES, 0, 2, {3, 5}};
    bt_nspec sizes_b = {BT_NSPEC_SIZES, 0, 2, {3, 5}};
    BT_CHECK(bt_nspec_eq(&sizes_a, &sizes_b));
    bt_nspec sizes_count = {BT_NSPEC_SIZES, 0, 1, {3, 5}};
    BT_CHECK(!bt_nspec_eq(&sizes_a, &sizes_count));
    bt_nspec sizes_first = {BT_NSPEC_SIZES, 0, 2, {4, 5}};
    BT_CHECK(!bt_nspec_eq(&sizes_a, &sizes_first));
    bt_nspec sizes_second = {BT_NSPEC_SIZES, 0, 2, {3, 6}};
    BT_CHECK(!bt_nspec_eq(&sizes_a, &sizes_second));
    BT_CHECK(!bt_nspec_eq(&fixed_a, &sizes_a));
    BT_CHECK(!bt_nspec_eq(&unset_a, &fixed_a));
}
