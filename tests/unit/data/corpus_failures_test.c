#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/corpus.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *HEADER = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";

static unsigned corpus_fail_counter;

static const char *corpus_fail_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-corpus-failures/%s-%d-%u", base ? base : ".", tag, _getpid(),
             corpus_fail_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void write_sheet(const char *root, const char *solver, const char *sheet, const char *body) {
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s/%s.tsv", root, solver, sheet);
    tu_write_file(path, body);
}

static void write_full_root(const char *root) {
    static const char *solvers[3] = {"cherry", "eggins", "user"};
    static const char *sheets[6] = {"edge", "corner", "flips", "twists", "parity", "ltct"};
    for (int s = 0; s < 3; s++) {
        for (int c = 0; c < 6; c++) {
            char body[512];
            if (s == 0 && c == 0) {
                snprintf(body, sizeof body,
                         "%s\nUF-UB-UR\tR2 U' S R2 S' R2 U R2\tR2 U':[S,R2]\tl1\tv1\n"
                         "UF-UB-UL\tU M2 U M' U2 M U M2 U'\tU M2 U:[M',U2]\tl2\tv2\n",
                         HEADER);
            } else {
                snprintf(body, sizeof body, "%s\nP%d-%d\tR U R'\t[A,B]\tl%d\tv%d\n", HEADER, s, c,
                         s, c);
            }
            write_sheet(root, solvers[s], sheets[c], body);
        }
    }
}

BT_TEST(corpus_failures, name_parsers_reject_unknown_values) {
    bt_solver solver;
    bt_sheet_class sheet;
    BT_CHECK(bt_solver_parse("cherry", &solver));
    BT_CHECK_EQ((int)solver, 0);
    BT_CHECK(!bt_solver_parse("bogus", &solver));
    BT_CHECK(bt_sheet_class_parse("ltct", &sheet));
    BT_CHECK_EQ((int)sheet, 5);
    BT_CHECK(!bt_sheet_class_parse("bogus", &sheet));
}

BT_TEST(corpus_failures, error_strings_render_each_kind) {
    bt_corpus_error err;
    char text[256];
    memset(&err, 0, sizeof err);
    errno = ENOENT;
    err.kind = BT_CORPUS_ERR_READ;
    snprintf(err.path, sizeof err.path, "%s", "ref/cherry/edge.tsv");
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "read ref/cherry/edge.tsv") == text);
    err.kind = BT_CORPUS_ERR_PARSE;
    err.tsv.kind = BT_TSV_ERR_BAD_HEADER;
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "parse ref/cherry/edge.tsv") == text);
    err.kind = BT_CORPUS_ERR_ALG;
    err.rowid = 3;
    snprintf(err.alg_error, sizeof err.alg_error, "%s", "unknown token: Q");
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "alg in ref/cherry/edge.tsv rowid 3: unknown token: Q");
    err.kind = BT_CORPUS_ERR_OOM;
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "out of memory loading ref/cherry/edge.tsv");
    bt_overlay_error overlay;
    memset(&overlay, 0, sizeof overlay);
    overlay.kind = BT_OVERLAY_ERR_UNKNOWN_SHEET;
    snprintf(overlay.key, sizeof overlay.key, "%s", "bogus/UF");
    bt_overlay_error_string(&overlay, text, sizeof text);
    BT_CHECK_EQ(text, "unknown sheet in overlay key: bogus/UF");
    overlay.kind = BT_OVERLAY_ERR_BAD_ALG;
    snprintf(overlay.position, sizeof overlay.position, "%s", "UF");
    snprintf(overlay.alg_error, sizeof overlay.alg_error, "%s", "unknown token: Q");
    bt_overlay_error_string(&overlay, text, sizeof text);
    BT_CHECK_EQ(text, "bad alg for position UF: unknown token: Q");
}

BT_TEST(corpus_failures, load_reports_the_first_missing_sheet) {
    const char *root = corpus_fail_dir("missing");
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_READ);
    BT_CHECK(strstr(err.path, "cherry/edge.tsv") != NULL);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, load_sets_the_error_kind_when_an_allocation_starves) {
    const char *root = corpus_fail_dir("oom-starve");
    write_full_root(root);
    int saw_corpus_oom = 0;
    for (size_t ok = 0; ok < 24; ok++) {
        bt_corpus corpus;
        bt_corpus_error err;
        memset(&err, 0xAA, sizeof err);
        bt_alloc_fail_arm(ok);
        int rc = bt_corpus_load(root, &corpus, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            bt_corpus_free(&corpus);
            continue;
        }
        BT_CHECK(err.kind == BT_CORPUS_ERR_READ || err.kind == BT_CORPUS_ERR_PARSE ||
                 err.kind == BT_CORPUS_ERR_ALG || err.kind == BT_CORPUS_ERR_OOM);
        char text[256];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_CHECK(text[0] != '\0');
        if (err.kind == BT_CORPUS_ERR_OOM) {
            saw_corpus_oom = 1;
            BT_CHECK(strstr(err.path, ".tsv") != NULL);
        }
    }
    BT_CHECK(saw_corpus_oom == 1);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) == 0);
    BT_CHECK(bt_corpus_total_rows(&corpus) == 13);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, load_reports_a_malformed_sheet) {
    const char *root = corpus_fail_dir("malformed");
    write_sheet(root, "cherry", "edge", "wrong header\n");
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_PARSE);
    BT_CHECK(strstr(err.path, "cherry/edge.tsv") != NULL);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, load_reports_a_bad_alg_row) {
    const char *root = corpus_fail_dir("bad-alg");
    char body[256];
    snprintf(body, sizeof body, "%s\nUF-UB-UR\tR Q\t[A,B]\tl\tv\n", HEADER);
    write_sheet(root, "cherry", "edge", body);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_ALG);
    BT_CHECK_EQ((int)err.rowid, 2);
    BT_CHECK_EQ(err.alg_error, "Q");
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, a_full_root_loads_and_the_queries_clip_their_buffers) {
    const char *root = corpus_fail_dir("full");
    write_full_root(root);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) == 0);
    BT_CHECK_EQ((int)bt_corpus_total_rows(&corpus), 13);
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2) != NULL);
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 9) == NULL);
    size_t hits[4];
    BT_CHECK_EQ((int)bt_corpus_sheet_rows(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, hits, 1), 2);
    BT_CHECK_EQ((int)bt_corpus_sheet_rows(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, hits, 4), 2);
    BT_CHECK_EQ((int)bt_corpus_rows_for_position(&corpus, BT_SHEET_EDGE, "UF-UB-UR", hits, 4), 1);
    BT_CHECK_EQ((int)bt_corpus_rows_for_position(&corpus, BT_SHEET_EDGE, "UF-UB-XX", hits, 4), 0);
    BT_CHECK_EQ((int)bt_corpus_distinct_positions(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE), 2);
    bt_corpus empty;
    bt_corpus_init(&empty);
    BT_CHECK(bt_corpus_is_empty(&empty));
    bt_corpus_free(&empty);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, the_overlay_rejects_unknown_sheets_and_bad_algs) {
    const char *root = corpus_fail_dir("overlay");
    write_full_root(root);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) == 0);
    bt_user_algs overlay;
    bt_user_algs_init(&overlay);
    bt_user_algs_set(&overlay, "bogus", "UF", "R U R'");
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) != 0);
    BT_CHECK(oerr.kind == BT_OVERLAY_ERR_UNKNOWN_SHEET);
    bt_user_algs_free(&overlay);
    bt_user_algs_init(&overlay);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UR", "R Q");
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) != 0);
    BT_CHECK(oerr.kind == BT_OVERLAY_ERR_BAD_ALG);
    bt_user_algs_free(&overlay);
    bt_user_algs_init(&overlay);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UR", "R U R'");
    bt_user_algs_set(&overlay, "corner", "UFR", "R U' R'");
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) == 0);
    BT_CHECK_EQ((int)bt_corpus_total_rows(&merged), 15);
    const bt_corpus_row *row = bt_corpus_row_at(&merged, BT_SOLVER_USER, BT_SHEET_EDGE, 1);
    BT_CHECK(row != NULL);
    BT_CHECK_EQ(row->position, "UF-UB-UR");
    BT_CHECK_EQ(row->alg_text, "R U R'");
    row = bt_corpus_row_at(&merged, BT_SOLVER_USER, BT_SHEET_CORNER, 1);
    BT_CHECK(row != NULL);
    BT_CHECK_EQ(row->position, "UFR");
    bt_corpus_free(&merged);
    bt_user_algs_free(&overlay);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_failures, load_and_overlay_survive_each_allocation_failure) {
    const char *root = corpus_fail_dir("oom");
    write_full_root(root);
    for (size_t ok = 0; ok < 24; ok++) {
        bt_corpus corpus;
        bt_corpus_error err;
        bt_alloc_fail_arm(ok);
        int rc = bt_corpus_load(root, &corpus, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_corpus_total_rows(&corpus), 13);
        }
        bt_corpus_free(&corpus);
    }
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) == 0);
    bt_user_algs overlay;
    bt_user_algs_init(&overlay);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UR", "R U R'");
    for (size_t ok = 0; ok < 24; ok++) {
        bt_corpus merged;
        bt_overlay_error oerr;
        bt_alloc_fail_arm(ok);
        int rc = bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_corpus_total_rows(&merged), 14);
        }
        bt_corpus_free(&merged);
    }
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) == 0);
    BT_CHECK_EQ((int)bt_corpus_total_rows(&merged), 14);
    bt_corpus_free(&merged);
    bt_user_algs_free(&overlay);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}
