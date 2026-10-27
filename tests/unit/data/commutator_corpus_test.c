#include "common.h"
#include "common/bt_harness.h"
#include "data/commutator.h"
#include "data/corpus.h"

#include <string.h>

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[512];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("corpus load failed: %s", text);
    }
}

BT_TEST(commutator_corpus, aggregate_coverage_is_pinned) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t parsed = 0;
    size_t verified = 0;
    size_t not_found = 0;
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    for (size_t i = 0; i < corpus.count; i++) {
        const char *commutator = corpus.rows[i].commutator;
        char trimmed[256];
        snprintf(trimmed, sizeof trimmed, "%s", commutator);
        size_t len = strlen(trimmed);
        while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t' ||
                           trimmed[len - 1] == '\n' || trimmed[len - 1] == '\r')) {
            trimmed[--len] = '\0';
        }
        while (trimmed[0] == ' ' || trimmed[0] == '\t') {
            memmove(trimmed, trimmed + 1, strlen(trimmed));
        }
        if (strcmp(trimmed, "Not found.") == 0) {
            not_found += 1;
            continue;
        }
        if (bt_decompose(trimmed, &units)) {
            parsed += 1;
        }
        if (bt_decompose_verified(trimmed, &corpus.rows[i].alg, &units)) {
            verified += 1;
        }
    }
    bt_expanded_list_free(&units);
    BT_CHECK(not_found == 68);
    BT_CHECK(parsed == 1009);
    BT_CHECK(verified == 1009);
    bt_corpus_free(&corpus);
}

BT_TEST(commutator_corpus, combo_and_nested_rows_parse_and_verify) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t combos = 0;
    size_t nested = 0;
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    for (size_t i = 0; i < corpus.count; i++) {
        const char *commutator = corpus.rows[i].commutator;
        char trimmed[256];
        snprintf(trimmed, sizeof trimmed, "%s", commutator);
        size_t len = strlen(trimmed);
        while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t' ||
                           trimmed[len - 1] == '\n' || trimmed[len - 1] == '\r')) {
            trimmed[--len] = '\0';
        }
        while (trimmed[0] == ' ' || trimmed[0] == '\t') {
            memmove(trimmed, trimmed + 1, strlen(trimmed));
        }
        if (strcmp(trimmed, "Not found.") == 0) {
            continue;
        }
        if (strchr(trimmed, '+') != NULL) {
            combos += 1;
            BT_CHECK(bt_decompose(trimmed, &units));
            BT_CHECK(bt_decompose_verified(trimmed, &corpus.rows[i].alg, &units));
        }
        size_t brackets = 0;
        for (const char *p = trimmed; *p != '\0'; p++) {
            if (*p == '[') {
                brackets += 1;
            }
        }
        if (brackets > 1) {
            nested += 1;
        }
    }
    bt_expanded_list_free(&units);
    BT_CHECK(combos == 19);
    BT_CHECK(nested == 19);
    bt_corpus_free(&corpus);
}

BT_TEST(commutator_corpus, named_nested_shapes_parse_and_verify) {
    bt_corpus corpus;
    loaded(&corpus);
    const char *targets[2] = {"U:[[R' E' R,U2]+U2:[S,R2]]", "S R':[[E,R2]+[F2,R S' R']]"};
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    for (int t = 0; t < 2; t++) {
        size_t matches = 0;
        for (size_t i = 0; i < corpus.count; i++) {
            const char *commutator = corpus.rows[i].commutator;
            if (strncmp(commutator, targets[t], strlen(targets[t])) == 0 &&
                (commutator[strlen(targets[t])] == '\0' || commutator[strlen(targets[t])] == ' ' ||
                 commutator[strlen(targets[t])] == '\r')) {
                matches += 1;
                BT_CHECK(bt_decompose(targets[t], &units));
                BT_CHECK(bt_decompose_verified(targets[t], &corpus.rows[i].alg, &units));
            }
        }
        BT_CHECK(matches > 0);
    }
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 4);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK(strcmp(row->commutator, "R2 U':[S,R2]") == 0);
        BT_CHECK(bt_decompose_verified(row->commutator, &row->alg, &units));
    }
    bt_expanded_list_free(&units);
    bt_corpus_free(&corpus);
}
