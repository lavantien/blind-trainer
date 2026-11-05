#include "common/bt_harness.h"
#include "data/db.h"
#include "data/migrations_embedded.h"
#include "data/repo_seeds.h"
#include "data/seedpack/mod.h"
#include "data/seedpack/normalizer.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned seed_err_counter;

static const char *seed_err_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-seed-errors/%s-%d-%u", base ? base : ".", tag, _getpid(),
             seed_err_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static bt_pack_entry entry_of(const char *event, size_t rows) {
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", "probe.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", event);
    snprintf(entry.method, sizeof entry.method, "%s", "cfop");
    snprintf(entry.stage, sizeof entry.stage, "%s", "pll");
    entry.expected_rows = rows;
    return entry;
}

static const char *HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at";

BT_TEST(seedpack_errors, registry_rejects_each_malformed_shape) {
    struct {
        const char *text;
        bt_seed_error_kind kind;
    } cases[] = {
        {"{}", BT_SEED_ERR_REGISTRY_JSON},
        {"[1]", BT_SEED_ERR_REGISTRY_JSON},
        {"[{\"file\":1,\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":1,\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":1,\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":1,\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\","
         "\"expected_rows\":\"1\"}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"extra\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":\"x\","
         "\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":8,"
         "\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":[],"
         "\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"n\":[2,9],"
         "\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"a/b.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\","
         "\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"..\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"\",\"method\":\"m\",\"stage\":\"s\",\"expected_rows\":1}]",
         BT_SEED_ERR_REGISTRY_RULE,
        },
        {"[{\"file\":\"a.tsv\",\"event\":\"e\",\"method\":\"m\",\"stage\":\"s\","
         "\"expected_rows\":-1}]",
         BT_SEED_ERR_REGISTRY_JSON,
        },
    };
    for (int i = 0; i < 18; i++) {
        bt_pack_entry *entries = NULL;
        size_t count = 0;
        bt_seed_error err;
        BT_CHECK(bt_seedpack_parse_registry(cases[i].text, &entries, &count, &err) != 0);
        BT_CHECK(err.kind == cases[i].kind);
        BT_CHECK(entries == NULL && count == 0);
    }
    const char *good = "[{\"file\":\"a.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                       "\"n\":[3,5],\"expected_rows\":2}]";
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_registry(good, &entries, &count, &err) == 0);
    BT_CHECK(count == 1);
    BT_CHECK(entries != NULL && entries[0].n.kind == BT_NSPEC_SIZES);
    BT_CHECK(entries[0].n.size_count == 2 && entries[0].n.sizes[1] == 5);
    bt_nspec twin = {BT_NSPEC_SIZES, 0, 2, {3, 5}};
    BT_CHECK(bt_nspec_eq(&entries[0].n, &twin) == true);
    twin.sizes[1] = 7;
    BT_CHECK(bt_nspec_eq(&entries[0].n, &twin) == false);
    twin.kind = BT_NSPEC_FIXED;
    twin.fixed = 3;
    BT_CHECK(bt_nspec_eq(&entries[0].n, &twin) == false);
    bt_seedpack_free_entries(entries);
}

BT_TEST(seedpack_errors, seed_error_string_renders_every_kind) {
    struct {
        bt_seed_error_kind kind;
        const char *needle;
    } cases[] = {
        {BT_SEED_ERR_REGISTRY_JSON, "registry parse"},
        {BT_SEED_ERR_REGISTRY_RULE, "registry rule"},
        {BT_SEED_ERR_IO, "io failed"},
        {BT_SEED_ERR_BAD_HEADER, "header must be"},
        {BT_SEED_ERR_RAGGED_ROW, "expected 5 columns"},
        {BT_SEED_ERR_EMPTY_FIELD, "column is empty"},
        {BT_SEED_ERR_BAD_FIELD, "bad"},
        {BT_SEED_ERR_DUPLICATE_KEY, "duplicate case_key"},
        {BT_SEED_ERR_BAD_FRONT_JSON, "front_json is not json"},
        {BT_SEED_ERR_FRONT_JSON_SHAPE, "front_json is not an object"},
        {BT_SEED_ERR_NOTE_COLLISION, "loader_notes"},
        {BT_SEED_ERR_COUNT_DRIFT, "row count drift"},
        {BT_SEED_ERR_NORMALIZE, "unknown token"},
        {BT_SEED_ERR_DRIFT, "seed drift"},
        {BT_SEED_ERR_STALE, "no longer exists"},
        {BT_SEED_ERR_DB, "query"},
    };
    for (int i = 0; i < 16; i++) {
        bt_seed_error err;
        memset(&err, 0, sizeof err);
        err.kind = cases[i].kind;
        snprintf(err.path, sizeof err.path, "%s", "p.tsv");
        snprintf(err.file, sizeof err.file, "%s", "pack.tsv");
        snprintf(err.case_key, sizeof err.case_key, "%s", "T");
        snprintf(err.column, sizeof err.column, "%s", "alg");
        snprintf(err.field, sizeof err.field, "%s", "date");
        snprintf(err.json_error, sizeof err.json_error, "%s", "json detail");
        snprintf(err.normalize, sizeof err.normalize, "%s", "unknown token 'q'");
        snprintf(err.stored, sizeof err.stored, "%s", "hash-a");
        snprintf(err.packed, sizeof err.packed, "%s", "hash-b");
        err.line = 3;
        err.found = 4;
        err.expected = 5;
        err.db.kind = BT_DB_ERR_QUERY;
        snprintf(err.db.step, sizeof err.db.step, "%s", "select card");
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        BT_CHECK(strstr(text, cases[i].needle) != NULL);
    }
}

BT_TEST(seedpack_errors, embedded_migrations_reject_out_of_range_indices) {
    size_t count = bt_db_embedded_migration_count();
    BT_CHECK(count == (size_t)bt_db_migration_count());
    BT_CHECK(bt_db_embedded_migration_name(count) == NULL);
    BT_CHECK(bt_db_embedded_migration_sql(count) == NULL);
    BT_CHECK(bt_db_embedded_migration_sql(999) == NULL);
}

static void parse_probe(const bt_pack_entry *entry, const char *text, bt_seed_rows *rows,
                        bt_seed_error *err, int *rc) {
    *rc = bt_seedpack_parse_pack(entry, text, rows, err);
}

BT_TEST(seedpack_errors, parse_pack_rejects_each_bad_row) {
    struct {
        const char *alg;
        const char *front;
        const char *source_at;
        bt_seed_error_kind kind;
    } cases[] = {
        {"R U R'", "{}", "202a-01-01", BT_SEED_ERR_BAD_FIELD},
        {"R U R'", "not json", "2026-01-01", BT_SEED_ERR_BAD_FRONT_JSON},
        {"R U R'", "[1]", "2026-01-01", BT_SEED_ERR_FRONT_JSON_SHAPE},
        {"R U R'", "{\"loader_notes\":[]}", "2026-01-01", BT_SEED_ERR_NOTE_COLLISION},
        {"8R", "{}", "2026-01-01", BT_SEED_ERR_NORMALIZE},
        {"", "{}", "2026-01-01", BT_SEED_ERR_EMPTY_FIELD},
    };
    for (int i = 0; i < 6; i++) {
        char text[512];
        snprintf(text, sizeof text, "%s\nT\t%s\t%s\thttps://x\t%s\n", HEADER, cases[i].alg,
                 cases[i].front, cases[i].source_at);
        bt_pack_entry entry = entry_of("3x3", 1);
        bt_seed_rows rows;
        bt_seed_error err;
        int rc = 0;
        parse_probe(&entry, text, &rows, &err, &rc);
        BT_CHECK(rc != 0);
        BT_CHECK(err.kind == cases[i].kind);
    }
    bt_pack_entry entry = entry_of("3x3", 1);
    bt_seed_rows rows;
    bt_seed_error err;
    int rc = 0;
    parse_probe(&entry, "no header at all\n", &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_BAD_HEADER);
    parse_probe(&entry, "", &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_BAD_HEADER);
    char ragged[256];
    snprintf(ragged, sizeof ragged, "%s\nT\tR U\t{}\n", HEADER);
    parse_probe(&entry, ragged, &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_RAGGED_ROW && err.found == 3);
    char padded[256];
    snprintf(padded, sizeof padded, "%s\n T\tR U\t{}\thttps://x\t2026-01-01\n", HEADER);
    parse_probe(&entry, padded, &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_BAD_FIELD);
    bt_pack_entry zero = entry_of("3x3", 0);
    parse_probe(&zero, HEADER, &rows, &err, &rc);
    BT_CHECK(rc == 0 && rows.count == 0);
    bt_seed_rows_free(&rows);
    char drift_text[256];
    snprintf(drift_text, sizeof drift_text,
             "%s\nT\tR U\t{}\thttps://x\t2026-01-01\nJ\tR U'\t{}\thttps://x\t2026-01-01\n", HEADER);
    parse_probe(&entry, drift_text, &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_COUNT_DRIFT && err.expected == 1 && err.found == 2);
    bt_pack_entry two = entry_of("3x3", 2);
    char duplicate[256];
    snprintf(duplicate, sizeof duplicate,
             "%s\nT\tR U\t{}\thttps://x\t2026-01-01\nT\tR U'\t{}\thttps://x\t2026-01-01\n", HEADER);
    parse_probe(&two, duplicate, &rows, &err, &rc);
    BT_CHECK(rc != 0 && err.kind == BT_SEED_ERR_DUPLICATE_KEY);
}

BT_TEST(seedpack_errors, parse_pack_accepts_crlf_and_yields_hashed_rows) {
    char text[512];
    snprintf(text, sizeof text, "%s\r\nT\tR U R'\t{}\thttps://x\t2026-01-01\r\n", HEADER);
    bt_pack_entry entry = entry_of("3x3", 1);
    bt_seed_rows rows;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_parse_pack(&entry, text, &rows, &err) == 0);
    BT_CHECK(rows.count == 1);
    BT_CHECK(strcmp(rows.items[0].alg, "R U R'") == 0);
    BT_CHECK(rows.items[0].content_hash[0] != '\0');
    bt_seed_rows_free(&rows);
}

BT_TEST(seedpack_errors, load_pack_reports_a_missing_file) {
    const char *dir = seed_err_dir("load-missing");
    bt_pack_entry entry = entry_of("3x3", 1);
    bt_seed_rows rows;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_IO);
    BT_CHECK(strstr(err.path, "probe.tsv") != NULL);
}

BT_TEST(seedpack_errors, normalizer_cube_dialect_shapes) {
    struct {
        const char *raw;
        const char *alg;
        const char *note;
    } ok_cases[] = {
        {"[R U]*3", "R U R U R U", NULL},
        {"(R U)2", "R U R U", NULL},
        {"X R", "x R", "mapped X to x"},
        {"R U -> whole cube note", "R U", "chain note"},
        {"rotate the whole cube", "", "prose alg"},
        {"  R   U  ", "R U", NULL},
    };
    for (int i = 0; i < 6; i++) {
        bt_normalized normalized;
        bt_normalize_error err;
        BT_CHECK(bt_seedpack_normalize_alg("3x3", ok_cases[i].raw, &normalized, &err) == true);
        BT_CHECK(strcmp(normalized.alg, ok_cases[i].alg) == 0);
        if (ok_cases[i].note != NULL) {
            BT_CHECK(normalized.note_count >= 1);
            BT_CHECK(strstr(normalized.notes[0], ok_cases[i].note) != NULL);
        }
        bt_normalized_free(&normalized);
    }
}

BT_TEST(seedpack_errors, normalizer_cube_rejects_each_bad_shape) {
    const char *bad[] = {
        "8R",
        "[R U*3",
        "[R [U]*2]",
        "[R U]3",
        "[R U]*3 x",
        "(R)0",
        "(R)100",
        "(R",
        ") R",
        "R )",
        "(",
    };
    bt_normalize_error_kind kinds[] = {
        BT_NORMALIZE_ERR_UNKNOWN_TOKEN,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_BAD_REPEAT,
        BT_NORMALIZE_ERR_UNBALANCED,
        BT_NORMALIZE_ERR_UNBALANCED,
        BT_NORMALIZE_ERR_UNBALANCED,
        BT_NORMALIZE_ERR_UNBALANCED,
    };
    for (int i = 0; i < 11; i++) {
        bt_normalized normalized;
        bt_normalize_error err;
        err.kind = BT_NORMALIZE_ERR_EMPTY_ALG;
        BT_CHECK(bt_seedpack_normalize_alg("3x3", bad[i], &normalized, &err) == false);
        BT_CHECK(err.kind == kinds[i]);
        char text[BT_SEEDPACK_ALG_MAX + 64];
        bt_normalize_error_string(&err, text, sizeof text);
        BT_CHECK(text[0] != '\0');
        bt_normalized_free(&normalized);
    }
    bt_normalized normalized;
    bt_normalize_error err;
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "", &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_EMPTY_ALG);
    BT_CHECK(bt_seedpack_normalize_alg("3x3", "   ", &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_EMPTY_ALG);
    char deep[1024];
    size_t at = 0;
    for (int i = 0; i < 257; i++) {
        deep[at++] = '(';
    }
    deep[at++] = ')';
    deep[at] = '\0';
    BT_CHECK(bt_seedpack_normalize_alg("3x3", deep, &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_UNBALANCED);
    char huge[1200];
    at = 0;
    huge[at++] = '[';
    for (int i = 0; i < 1000; i++) {
        huge[at++] = 'R';
        if (i < 999) {
            huge[at++] = ' ';
        }
    }
    huge[at++] = ']';
    snprintf(huge + at, sizeof huge - at, "*99");
    BT_CHECK(bt_seedpack_normalize_alg("3x3", huge, &normalized, &err) == false);
    BT_CHECK(err.kind == BT_NORMALIZE_ERR_BAD_REPEAT);
}

BT_TEST(seedpack_errors, normalizer_sq1_dialect_shapes) {
    struct {
        const char *raw;
        const char *alg;
        const char *note;
    } ok_cases[] = {
        {"(1,0) / (3,3)", "(1,0) / (3,3)", NULL},
        {"-6,0 /", "(6,0) /", "wraparound"},
        {"(-6,0)", "(6,0)", "wraparound"},
        {"(0,-6) /", "(0,6) /", "wraparound"},
        {"(U2 D') / (1,0)", "/ (1,0)", "auf prefix stripped"},
        {"(U) / (1,0)", "/ (1,0)", "auf prefix stripped"},
        {"(1,0) *", "(1,0)", "equator marker"},
        {"(2,-1) / (-4,5)", "(2,-1) / (-4,5)", NULL},
    };
    for (int i = 0; i < 8; i++) {
        bt_normalized normalized;
        bt_normalize_error err;
        BT_CHECK(bt_seedpack_normalize_alg("sq1", ok_cases[i].raw, &normalized, &err) == true);
        BT_CHECK(strcmp(normalized.alg, ok_cases[i].alg) == 0);
        if (ok_cases[i].note != NULL) {
            BT_CHECK(normalized.note_count >= 1);
            bool found = false;
            for (size_t n = 0; n < normalized.note_count; n++) {
                if (strstr(normalized.notes[n], ok_cases[i].note) != NULL) {
                    found = true;
                }
            }
            BT_CHECK(found);
        } else {
            BT_CHECK(normalized.note_count == 0);
        }
        bt_normalized_free(&normalized);
    }
}

BT_TEST(seedpack_errors, normalizer_sq1_rejects_each_bad_shape) {
    struct {
        const char *raw;
        bt_normalize_error_kind kind;
    } bad[] = {
        {"(1,0", BT_NORMALIZE_ERR_UNBALANCED},
        {"z", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"*", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(0,0)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(01,0)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(1,-0)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(99999999999999999999,0)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(1,x)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"(R) / (1,0)", BT_NORMALIZE_ERR_UNKNOWN_TOKEN},
        {"", BT_NORMALIZE_ERR_EMPTY_ALG},
        {"   ", BT_NORMALIZE_ERR_EMPTY_ALG},
        {"(U)", BT_NORMALIZE_ERR_EMPTY_ALG},
    };
    for (int i = 0; i < 12; i++) {
        bt_normalized normalized;
        bt_normalize_error err;
        err.kind = BT_NORMALIZE_ERR_EMPTY_ALG;
        BT_CHECK(bt_seedpack_normalize_alg("sq1", bad[i].raw, &normalized, &err) == false);
        BT_CHECK(err.kind == bad[i].kind);
        bt_normalized_free(&normalized);
    }
}

BT_TEST(seedpack_errors, seed_pack_wraps_load_and_parse) {
    const char *dir = seed_err_dir("seed-pack-io");
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/probe.tsv", dir);
    char text[512];
    snprintf(text, sizeof text, "%s\nT\tR U R'\t{}\thttps://x\t2026-01-01\n", HEADER);
    tu_write_file(path, text);
    bt_pack_entry entry = entry_of("3x3", 1);
    bt_seed_rows rows;
    bt_seed_error err;
    BT_CHECK(bt_seedpack_load_pack(dir, &entry, &rows, &err) == 0);
    BT_CHECK(rows.count == 1 && strcmp(rows.items[0].case_key, "T") == 0);
    bt_seed_rows_free(&rows);
    char registry[BT_TU_PATH_MAX];
    snprintf(registry, sizeof registry, "%s/registry.json", dir);
    tu_write_file(registry, "[{\"file\":\"probe.tsv\",\"event\":\"3x3\",\"method\":\"cfop\","
                            "\"stage\":\"pll\",\"expected_rows\":1}]");
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    BT_CHECK(bt_seedpack_load_registry(dir, &entries, &count, &err) == 0);
    BT_CHECK(count == 1 && strcmp(entries[0].file, "probe.tsv") == 0);
    bt_seedpack_free_entries(entries);
}
