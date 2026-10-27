#include "common.h"
#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/cube.h"
#include "core/expand.h"
#include "data/commutator.h"
#include "data/corpus.h"
#include "data/seedpack/normalizer.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_cube state_of(const bt_alg *alg) {
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, alg);
    return cube;
}

static void assert_state_true(const char *text, const bt_alg *expanded, const bt_alg *reference) {
    (void)text;
    bt_cube left = state_of(expanded);
    bt_cube right = state_of(reference);
    BT_CHECK(bt_cube_eq(&left, &right));
    char formatted[4096];
    bt_strbuf sb;
    bt_strbuf_init(&sb, 128);
    bt_alg_format(expanded, &sb);
    snprintf(formatted, sizeof formatted, "%s", sb.data);
    bt_strbuf_free(&sb);
    bt_alg again;
    bt_alg_init(&again);
    BT_CHECK(bt_expand(formatted, &again));
    BT_CHECK(bt_alg_eq(&again, expanded));
    bt_alg_free(&again);
}

static void load_corpus(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        BT_ABORT("corpus load failed");
    }
}

BT_TEST(expand_oracle, every_corpus_row_expands_state_true_and_idempotently) {
    bt_corpus corpus;
    load_corpus(&corpus);
    BT_CHECK(bt_corpus_total_rows(&corpus) == 1077);
    for (size_t i = 0; i < corpus.count; i++) {
        bt_alg expanded;
        bt_alg_init(&expanded);
        if (!bt_expand(corpus.rows[i].alg_text, &expanded)) {
            BT_ABORT("flat alg must expand: %s", corpus.rows[i].alg_text);
        }
        assert_state_true(corpus.rows[i].alg_text, &expanded, &corpus.rows[i].alg);
        bt_alg_free(&expanded);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(expand_oracle, every_corpus_commutator_expands_state_true_through_verification) {
    bt_corpus corpus;
    load_corpus(&corpus);
    size_t decomposable = 0;
    size_t written_order = 0;
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    for (size_t i = 0; i < corpus.count; i++) {
        if (!bt_decompose(corpus.rows[i].commutator, &units)) {
            continue;
        }
        decomposable += 1;
        if (!bt_decompose_verified(corpus.rows[i].commutator, &corpus.rows[i].alg, &units)) {
            BT_ABORT("verified decomposition: %s", corpus.rows[i].commutator);
        }
        bt_alg joined;
        bt_alg_init(&joined);
        for (size_t u = 0; u < units.count; u++) {
            bt_alg expansion = bt_expanded_expansion(&units.items[u]);
            for (size_t m = 0; m < bt_alg_len(&expansion); m++) {
                bt_alg_push(&joined, expansion.items[m]);
            }
            bt_alg_free(&expansion);
        }
        bt_alg cancelled;
        bt_alg_init(&cancelled);
        bt_expand_cancel(&joined, &cancelled);
        assert_state_true(corpus.rows[i].commutator, &cancelled, &corpus.rows[i].alg);
        bool all_written_order = true;
        for (size_t u = 0; u < units.count; u++) {
            if (units.items[u].b_first) {
                all_written_order = false;
            }
        }
        if (all_written_order) {
            written_order += 1;
            bt_alg expanded;
            bt_alg_init(&expanded);
            if (!bt_expand(corpus.rows[i].commutator, &expanded)) {
                BT_ABORT("written order expands: %s", corpus.rows[i].commutator);
            }
            assert_state_true(corpus.rows[i].commutator, &expanded, &corpus.rows[i].alg);
            bt_alg_free(&expanded);
        }
        bt_alg_free(&joined);
        bt_alg_free(&cancelled);
    }
    bt_expanded_list_free(&units);
    BT_CHECK(decomposable == 1009);
    BT_CHECK(written_order == 1009);
    bt_corpus_free(&corpus);
}

static void bld_entries(bt_pack_entry **out, size_t *out_count) {
    char packs[BT_PATH_MAX];
    td_packs_root(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs, &entries, &count, &err) != 0) {
        BT_ABORT("registry failed");
    }
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].event, "3bld") == 0) {
            entries[kept++] = entries[i];
        }
    }
    *out = entries;
    *out_count = kept;
}

BT_TEST(expand_oracle, every_bld_seed_row_expands_state_true_and_idempotently) {
    char packs[BT_PATH_MAX];
    td_packs_root(packs);
    bt_pack_entry *entries = NULL;
    size_t entry_count = 0;
    bld_entries(&entries, &entry_count);
    size_t rows = 0;
    size_t skipped = 0;
    bt_seed_error err;
    for (size_t e = 0; e < entry_count; e++) {
        bt_seed_rows pack;
        if (bt_seedpack_load_pack(packs, &entries[e], &pack, &err) != 0) {
            BT_ABORT("pack failed");
        }
        for (size_t r = 0; r < pack.count; r++) {
            bt_alg parsed;
            bt_alg_init(&parsed);
            char parse_err[64];
            if (pack.items[r].alg[0] == '\0' ||
                !bt_alg_parse(pack.items[r].alg, &parsed, parse_err, sizeof parse_err)) {
                bt_alg_free(&parsed);
                skipped += 1;
                continue;
            }
            bt_alg expanded;
            bt_alg_init(&expanded);
            if (!bt_expand(pack.items[r].alg, &expanded)) {
                BT_ABORT("seed alg must expand: %s", pack.items[r].alg);
            }
            assert_state_true(pack.items[r].alg, &expanded, &parsed);
            rows += 1;
            bt_alg_free(&parsed);
            bt_alg_free(&expanded);
        }
        bt_seed_rows_free(&pack);
    }
    bt_seedpack_free_entries(entries);
    BT_CHECK(rows > 0);
    BT_CHECK(skipped == 11);
}

BT_TEST(expand_oracle, every_bld_raw_pack_alg_expands_to_its_normalized_state) {
    char packs[BT_PATH_MAX];
    td_packs_root(packs);
    bt_pack_entry *entries = NULL;
    size_t entry_count = 0;
    bld_entries(&entries, &entry_count);
    size_t raws = 0;
    size_t skipped = 0;
    for (size_t e = 0; e < entry_count; e++) {
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", packs, entries[e].file);
        char *text = td_read_file(path);
        char *cursor = text;
        bool header = true;
        while (*cursor != '\0') {
            char *line = cursor;
            char *newline = strchr(line, '\n');
            if (newline != NULL) {
                *newline = '\0';
                cursor = newline + 1;
            } else {
                cursor = line + strlen(line);
            }
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\r') {
                line[len - 1] = '\0';
            }
            bool blank = true;
            for (char *p = line; *p != '\0'; p++) {
                if (*p != ' ' && *p != '\t') {
                    blank = false;
                }
            }
            if (header) {
                header = false;
                continue;
            }
            if (blank) {
                continue;
            }
            char *first_tab = strchr(line, '\t');
            if (first_tab == NULL) {
                continue;
            }
            char *alg_start = first_tab + 1;
            char *next_tab = strchr(alg_start, '\t');
            if (next_tab != NULL) {
                *next_tab = '\0';
            }
            bt_normalized normalized;
            bt_normalize_error nerr;
            if (!bt_seedpack_normalize_alg(entries[e].event, alg_start, &normalized, &nerr)) {
                continue;
            }
            bt_alg parsed;
            bt_alg_init(&parsed);
            char parse_err[64];
            if (normalized.alg[0] == '\0' ||
                !bt_alg_parse(normalized.alg, &parsed, parse_err, sizeof parse_err)) {
                bt_alg_free(&parsed);
                bt_normalized_free(&normalized);
                skipped += 1;
                continue;
            }
            bt_alg expanded;
            bt_alg_init(&expanded);
            if (!bt_expand(alg_start, &expanded)) {
                BT_ABORT("raw pack alg must expand: %s", alg_start);
            }
            assert_state_true(alg_start, &expanded, &parsed);
            raws += 1;
            bt_alg_free(&parsed);
            bt_alg_free(&expanded);
            bt_normalized_free(&normalized);
        }
        free(text);
    }
    bt_seedpack_free_entries(entries);
    BT_CHECK(raws > 0);
    BT_CHECK(skipped == 11);
}
