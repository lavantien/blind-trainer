#include "common.h"
#include "common/bt_harness.h"
#include "data/corpus.h"
#include "data/paths.h"
#include "data/tsv.h"
#include "data/verify.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char HEADER[] = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";
static const char X2[] = "R' D' R D R' D' R D";
static const char TPERM[] = "R U R' U' R' F R2 U' R' U' R U R' F'";
static const char EDGE_ALG[] = "M2 U M U2 M' U M2";
static const char CORNER_ALG[] = "R U R' D' R U' R' D";
static const char FLIP_ALG[] = "M' U M' U M' U2 M U M U M U2";

static void twist_alg(char *out, size_t out_size) {
    snprintf(out, out_size, "%s U %s %s U'", X2, X2, X2);
}

static void ltct_alg(char *out, size_t out_size) {
    snprintf(out, out_size, "%s %s U %s U %s U2", TPERM, X2, X2, X2);
}

static void line_of(char *out, size_t out_size, const char *position, const char *alg) {
    snprintf(out, out_size, "%s\t%s\tNot found.\t\tnote\n", position, alg);
}

static void sheet_text(char *out, size_t out_size, const char (*rows)[256], size_t count) {
    size_t used = snprintf(out, out_size, "%s", HEADER);
    for (size_t i = 0; i < count; i++) {
        used += (size_t)snprintf(out + used, out_size - used, "\n%s", rows[i]);
    }
}

static const char *mini_root(const char *label) {
    const char *root = tu_temp_dir("bt-corpus-more", label);
    char dir[BT_TU_PATH_MAX];
    const char *solvers[2] = {"cherry", "eggins"};
    for (int s = 0; s < 2; s++) {
        snprintf(dir, sizeof dir, "%s/%s", root, solvers[s]);
        tu_mkdir_p(dir);
        char empty[128];
        sheet_text(empty, sizeof empty, NULL, 0);
        for (int sheet = 0; sheet < BT_SHEET_CLASS_COUNT; sheet++) {
            char path[BT_TU_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s.tsv", dir,
                     bt_sheet_class_file((bt_sheet_class)sheet));
            tu_write_file(path, empty);
        }
    }
    struct {
        bt_sheet_class sheet;
        const char *positions[2];
        const char *algs[2];
        size_t count;
    } cherry[6] = {
        {BT_SHEET_EDGE, {"UF-UR-UL", "UF-UR-UL"}, {EDGE_ALG, EDGE_ALG}, 2},
        {BT_SHEET_CORNER, {"UFR-RDF-BDR", ""}, {CORNER_ALG, ""}, 1},
        {BT_SHEET_FLIPS, {"UF & UB", ""}, {FLIP_ALG, ""}, 1},
        {BT_SHEET_TWISTS, {"UFR & UBR", ""}, {"", ""}, 1},
        {BT_SHEET_PARITY, {"UR-UL, UFR-UBR", ""}, {TPERM, ""}, 1},
        {BT_SHEET_LTCT, {"UR-UL, UFR-RUB[LUB]", ""}, {"", ""}, 1},
    };
    char twist[256];
    twist_alg(twist, sizeof twist);
    cherry[3].algs[0] = twist;
    char ltct[256];
    ltct_alg(ltct, sizeof ltct);
    cherry[5].algs[0] = ltct;
    for (int c = 0; c < 6; c++) {
        char rows[2][256];
        for (size_t r = 0; r < cherry[c].count; r++) {
            line_of(rows[r], sizeof rows[r], cherry[c].positions[r], cherry[c].algs[r]);
        }
        char text[640];
        sheet_text(text, sizeof text, rows, cherry[c].count);
        char path[BT_TU_PATH_MAX];
        snprintf(path, sizeof path, "%s/cherry/%s.tsv", root, bt_sheet_class_file(cherry[c].sheet));
        tu_write_file(path, text);
    }
    char eggins_rows[1][256];
    line_of(eggins_rows[0], sizeof eggins_rows[0], "UF-UB-UL", "U M2 U M' U2 M U M2 U'");
    char eggins[384];
    sheet_text(eggins, sizeof eggins, eggins_rows, 1);
    char eggins_path[BT_TU_PATH_MAX];
    snprintf(eggins_path, sizeof eggins_path, "%s/eggins/edge.tsv", root);
    tu_write_file(eggins_path, eggins);
    return root;
}

static bt_corpus load_mini(const char *root) {
    bt_corpus corpus;
    bt_corpus_error err;
    if (bt_corpus_load(root, &corpus, &err) != 0) {
        char text[512];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("mini corpus load failed: %s", text);
    }
    return corpus;
}

BT_TEST(corpus_more, mini_corpus_loads_and_every_row_verifies) {
    const char *root = mini_root("load");
    bt_corpus corpus = load_mini(root);
    BT_CHECK(bt_corpus_total_rows(&corpus) == 8);
    BT_CHECK(bt_corpus_is_empty(&corpus) == false);
    for (size_t i = 0; i < corpus.count; i++) {
        bt_verdict verdict;
        bt_verify(&corpus.rows[i].alg, corpus.rows[i].position, corpus.rows[i].sheet, &verdict);
        char text[512];
        bt_verdict_string(&verdict, text, sizeof text);
        BT_CHECK(bt_verdict_ok(&verdict));
        bt_verdict_free(&verdict);
    }
    const bt_corpus_row *first = &corpus.rows[0];
    BT_CHECK(bt_alg_qtm(&first->alg) == 10);
    BT_CHECK(bt_alg_htm(&first->alg) == 7);
    BT_CHECK_EQ(first->alg_text, EDGE_ALG);
    BT_CHECK_EQ(first->recon_link, "");
    BT_CHECK_EQ(first->youtube_link, "note");
    BT_CHECK_EQ(first->commutator, "Not found.");
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, mini_corpus_indexing_and_queries) {
    const char *root = mini_root("index");
    bt_corpus corpus = load_mini(root);
    size_t indices[BT_CORPUS_INDEX_MAX];
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2) != NULL);
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 3) == NULL);
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_USER, BT_SHEET_EDGE, 1) == NULL);
    BT_CHECK_EQ(corpus.rows[1].position, "UF-UR-UL");
    BT_CHECK(bt_corpus_sheet_rows(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, indices,
                                  BT_CORPUS_INDEX_MAX) == 2);
    BT_CHECK(bt_corpus_sheet_rows(&corpus, BT_SOLVER_EGGINS, BT_SHEET_CORNER, indices,
                                  BT_CORPUS_INDEX_MAX) == 0);
    size_t group = bt_corpus_rows_for_position(&corpus, BT_SHEET_EDGE, "UF-UR-UL", indices,
                                               BT_CORPUS_INDEX_MAX);
    BT_CHECK(group == 2);
    for (size_t i = 0; i < group; i++) {
        BT_CHECK(corpus.rows[indices[i]].solver == BT_SOLVER_CHERRY);
    }
    BT_CHECK(bt_corpus_rows_for_position(&corpus, BT_SHEET_LTCT, "nope", indices,
                                         BT_CORPUS_INDEX_MAX) == 0);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE) == 1);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_EGGINS, BT_SHEET_EDGE) == 1);
    BT_CHECK(bt_corpus_rows_for_position(&corpus, BT_SHEET_LTCT, "UR-UL, UFR-RUB[LUB]", indices,
                                         BT_CORPUS_INDEX_MAX) == 1);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, missing_sheet_file_reports_the_path) {
    const char *root = mini_root("missing");
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/eggins/twists.tsv", root);
    BT_CHECK(remove(missing) == 0);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_READ);
    BT_CHECK(strcmp(err.path, missing) == 0);
    char text[512];
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "twists.tsv") != NULL);
    BT_CHECK(strncmp(text, "read ", 5) == 0);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, corrupt_sheet_reports_parse_error_with_line) {
    const char *root = mini_root("parse");
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/eggins/parity.tsv", root);
    tu_write_file(path, "junk\n");
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_PARSE);
    BT_CHECK(strstr(err.path, "parity.tsv") != NULL);
    bt_tsv_error_string(&err.tsv, path, sizeof path);
    BT_CHECK(strcmp(path,
                    "line 1: header must be Position/Alg/Commutator/Reconstruction Link/Youtube "
                    "Link") == 0);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, unparsable_alg_reports_file_and_rowid) {
    const char *root = mini_root("badalg");
    char rows[1][256];
    line_of(rows[0], sizeof rows[0], "UF & UB", "Q9");
    char text[384];
    sheet_text(text, sizeof text, rows, 1);
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/eggins/flips.tsv", root);
    tu_write_file(path, text);
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    BT_CHECK(err.kind == BT_CORPUS_ERR_ALG);
    BT_CHECK(strstr(err.path, "flips.tsv") != NULL);
    BT_CHECK(err.rowid == 2);
    char shown[512];
    bt_corpus_error_string(&err, shown, sizeof shown);
    BT_CHECK(strstr(shown, "rowid 2") != NULL);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, overlay_rows_join_the_mini_corpus) {
    const char *root = mini_root("overlay");
    bt_corpus corpus = load_mini(root);
    bt_user_algs empty;
    bt_user_algs_init(&empty);
    bt_corpus untouched;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &empty, &untouched, &oerr) == 0);
    BT_CHECK(bt_corpus_total_rows(&untouched) == bt_corpus_total_rows(&corpus));
    bt_corpus_free(&untouched);
    bt_user_algs_free(&empty);
    bt_user_algs overlay;
    bt_user_algs_init(&overlay);
    bt_user_algs_set(&overlay, "edge", "UF-UR-UL", EDGE_ALG);
    bt_user_algs_set(&overlay, "flips", "UF & UB", FLIP_ALG);
    bt_corpus merged;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) == 0);
    BT_CHECK(bt_corpus_total_rows(&merged) == bt_corpus_total_rows(&corpus) + 2);
    size_t indices[BT_CORPUS_INDEX_MAX];
    size_t group = bt_corpus_rows_for_position(&merged, BT_SHEET_EDGE, "UF-UR-UL", indices,
                                               BT_CORPUS_INDEX_MAX);
    BT_CHECK(group == 3);
    if (group == 3) {
        const bt_corpus_row *user = &merged.rows[indices[2]];
        BT_CHECK(user->solver == BT_SOLVER_USER);
        BT_CHECK(user->rowid == 1);
        BT_CHECK_EQ(user->commutator, "");
        BT_CHECK_EQ(user->recon_link, "");
        BT_CHECK_EQ(user->youtube_link, "");
        bt_verdict verdict;
        bt_verify(&user->alg, user->position, BT_SHEET_EDGE, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict));
        bt_verdict_free(&verdict);
    }
    bt_corpus_free(&merged);
    bt_user_algs_free(&overlay);
    bt_corpus_free(&corpus);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, overlay_rejects_unknown_sheet_keys_and_bad_algs) {
    const char *root = mini_root("badoverlay");
    bt_corpus corpus = load_mini(root);
    const char *dir = tu_temp_dir("bt-corpus-more", "badoverlay-user");
    char algs_path[BT_TU_PATH_MAX];
    snprintf(algs_path, sizeof algs_path, "%s/algs/user-algs.json", dir);
    tu_write_file(algs_path, "{\"entries\":{\"bogus/UF-UR-UL\":{\"alg\":\"R U R'\"},"
                             "\"noslash\":{\"alg\":\"R U R'\"}}}");
    bt_user_algs overlay;
    bt_paths_error perr;
    BT_CHECK(bt_user_algs_load_at(&overlay, dir, &perr) == BT_DATA_READ_OK);
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) != 0);
    BT_CHECK(oerr.kind == BT_OVERLAY_ERR_UNKNOWN_SHEET);
    BT_CHECK_EQ(oerr.key, "bogus/UF-UR-UL");
    bt_user_algs_free(&overlay);
    bt_user_algs bad_alg;
    bt_user_algs_init(&bad_alg);
    bt_user_algs_set(&bad_alg, "corner", "UFR-RDF-BDR", "Q9");
    BT_CHECK(bt_corpus_with_overlay(&corpus, &bad_alg, &merged, &oerr) != 0);
    BT_CHECK(oerr.kind == BT_OVERLAY_ERR_BAD_ALG);
    BT_CHECK_EQ(oerr.position, "UFR-RDF-BDR");
    char text[512];
    bt_overlay_error_string(&oerr, text, sizeof text);
    BT_CHECK(strstr(text, "bad alg for position UFR-RDF-BDR") != NULL);
    bt_user_algs_free(&bad_alg);
    bt_corpus_free(&corpus);
    tu_remove_tree(dir);
    tu_remove_tree(root);
}

BT_TEST(corpus_more, solver_and_sheet_parsing_round_trip) {
    bt_solver solvers[3] = {BT_SOLVER_CHERRY, BT_SOLVER_EGGINS, BT_SOLVER_USER};
    for (int i = 0; i < 3; i++) {
        bt_solver parsed;
        BT_CHECK(bt_solver_parse(bt_solver_dir(solvers[i]), &parsed));
        BT_CHECK(parsed == solvers[i]);
    }
    bt_solver capped;
    BT_CHECK(bt_solver_parse("Cherry", &capped) == false);
    BT_CHECK(bt_solver_parse("", &capped) == false);
    BT_CHECK_EQ(bt_solver_dir((bt_solver)0), "cherry");
    BT_CHECK_EQ(bt_solver_dir((bt_solver)1), "eggins");
    BT_CHECK((int)BT_SHEET_CLASS_COUNT == 6);
    for (int i = 0; i < BT_SHEET_CLASS_COUNT; i++) {
        bt_sheet_class parsed;
        BT_CHECK(bt_sheet_class_parse(bt_sheet_class_file((bt_sheet_class)i), &parsed));
        BT_CHECK(parsed == (bt_sheet_class)i);
        BT_CHECK(strcmp(bt_sheet_class_file((bt_sheet_class)i),
                        bt_sheet_class_file((bt_sheet_class)i)) == 0);
    }
    bt_sheet_class edged;
    BT_CHECK(bt_sheet_class_parse("edges", &edged) == false);
    BT_CHECK(bt_sheet_class_parse("", &edged) == false);
}

BT_TEST(corpus_more, mini_sheets_parse_with_expected_counts) {
    const char *root = mini_root("tsv");
    struct {
        const char *solver;
        bt_sheet_class sheet;
        size_t count;
    } expected[12] = {
        {"cherry", BT_SHEET_EDGE, 2},   {"cherry", BT_SHEET_CORNER, 1},
        {"cherry", BT_SHEET_FLIPS, 1},  {"cherry", BT_SHEET_TWISTS, 1},
        {"cherry", BT_SHEET_PARITY, 1}, {"cherry", BT_SHEET_LTCT, 1},
        {"eggins", BT_SHEET_EDGE, 1},   {"eggins", BT_SHEET_CORNER, 0},
        {"eggins", BT_SHEET_FLIPS, 0},  {"eggins", BT_SHEET_TWISTS, 0},
        {"eggins", BT_SHEET_PARITY, 0}, {"eggins", BT_SHEET_LTCT, 0},
    };
    for (int i = 0; i < 12; i++) {
        char path[BT_TU_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s/%s.tsv", root, expected[i].solver,
                 bt_sheet_class_file(expected[i].sheet));
        char *text = td_read_file(path);
        bt_tsv_rows rows;
        bt_tsv_error err;
        BT_CHECK(bt_tsv_parse_sheet(text, &rows, &err) == 0);
        BT_CHECK(rows.count == expected[i].count);
        bt_tsv_rows_free(&rows);
        free(text);
    }
    tu_remove_tree(root);
}
