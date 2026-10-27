#include "common.h"
#include "data/corpus.h"
#include "unit/data/test_util.h"

#include <stdlib.h>
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

BT_TEST(corpus_oracle, total_row_count_is_1077) {
    bt_corpus corpus;
    loaded(&corpus);
    BT_CHECK(corpus.count == 1077);
    BT_CHECK(bt_corpus_total_rows(&corpus) == 1077);
    BT_CHECK(bt_corpus_is_empty(&corpus) == false);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, per_solver_and_sheet_counts_match_the_dumps) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t indices[BT_CORPUS_INDEX_MAX];
    for (size_t i = 0; i < 12; i++) {
        bt_solver solver;
        bt_sheet_class sheet;
        char sheet_name[32];
        BT_CHECK(bt_solver_parse(TD_SHEETS[i].solver_dir, &solver));
        snprintf(sheet_name, sizeof sheet_name, "%.*s", (int)(strlen(TD_SHEETS[i].file) - 4),
                 TD_SHEETS[i].file);
        BT_CHECK(bt_sheet_class_parse(sheet_name, &sheet));
        size_t count = bt_corpus_sheet_rows(&corpus, solver, sheet, indices, BT_CORPUS_INDEX_MAX);
        BT_CHECK(count == TD_SHEETS[i].expected);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, distinct_position_counts_match_coverage_report) {
    bt_corpus corpus;
    loaded(&corpus);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE) == 506);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_CHERRY, BT_SHEET_CORNER) == 318);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_EGGINS, BT_SHEET_EDGE) == 71);
    BT_CHECK(bt_corpus_distinct_positions(&corpus, BT_SOLVER_EGGINS, BT_SHEET_CORNER) == 38);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, multi_alg_positions_stay_grouped_in_rowid_order) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t indices[BT_CORPUS_INDEX_MAX];
    size_t group_len = bt_corpus_rows_for_position(&corpus, BT_SHEET_EDGE, "UF-UB-UL", indices,
                                                   BT_CORPUS_INDEX_MAX);
    BT_CHECK(group_len == 3);
    size_t cherry = 0;
    size_t cherry_indices[4];
    for (size_t i = 0; i < group_len; i++) {
        if (corpus.rows[indices[i]].solver == BT_SOLVER_CHERRY) {
            cherry_indices[cherry++] = indices[i];
        }
    }
    BT_CHECK(cherry == 2);
    BT_CHECK(corpus.rows[cherry_indices[0]].rowid == 2);
    BT_CHECK(corpus.rows[cherry_indices[1]].rowid == 3);
    BT_CHECK_EQ(corpus.rows[cherry_indices[0]].alg_text, "U M2 U M' U2 M U M2 U'");
    BT_CHECK_EQ(corpus.rows[cherry_indices[1]].alg_text, "U2 R2 U' S' U2 S U' R2 U2");
    BT_CHECK(corpus.rows[indices[2]].solver == BT_SOLVER_EGGINS);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, sheet_class_and_solver_round_trip_names) {
    BT_CHECK((int)BT_SHEET_CLASS_COUNT == 6);
    for (int i = 0; i < BT_SHEET_CLASS_COUNT; i++) {
        bt_sheet_class sheet;
        BT_CHECK(bt_sheet_class_parse(bt_sheet_class_file((bt_sheet_class)i), &sheet));
        BT_CHECK(sheet == (bt_sheet_class)i);
    }
    bt_sheet_class nonsense;
    BT_CHECK(bt_sheet_class_parse("nonsense", &nonsense) == false);
    for (int i = 0; i < BT_SOLVER_VENDOR_COUNT; i++) {
        bt_solver solver;
        BT_CHECK(bt_solver_parse(bt_solver_dir((bt_solver)i), &solver));
        BT_CHECK(solver == (bt_solver)i);
    }
    bt_solver unknown;
    BT_CHECK(bt_solver_parse("nonsense", &unknown) == false);
    BT_CHECK_EQ(bt_solver_dir(BT_SOLVER_USER), "user");
    BT_CHECK(bt_solver_parse("user", &unknown));
    BT_CHECK(unknown == BT_SOLVER_USER);
}

BT_TEST(corpus_oracle, rowid_lookup_hits_vendor_rows_and_misses_others) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->position, "UF-UB-UL");
        BT_CHECK(bt_alg_qtm(&row->alg) == 12);
        BT_CHECK(bt_alg_htm(&row->alg) == 9);
    }
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 99999) == NULL);
    BT_CHECK(bt_corpus_row_at(&corpus, BT_SOLVER_USER, BT_SHEET_EDGE, 2) == NULL);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, position_lookup_spans_solvers_deterministically) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t indices[BT_CORPUS_INDEX_MAX];
    size_t group_len = bt_corpus_rows_for_position(&corpus, BT_SHEET_CORNER, "UFR-BDR-BUL", indices,
                                                   BT_CORPUS_INDEX_MAX);
    BT_CHECK(group_len == 2);
    BT_CHECK(corpus.rows[indices[0]].solver == BT_SOLVER_CHERRY);
    BT_CHECK(corpus.rows[indices[1]].solver == BT_SOLVER_EGGINS);
    for (size_t i = 0; i < group_len; i++) {
        BT_CHECK_EQ(corpus.rows[indices[i]].position, "UFR-BDR-BUL");
    }
    BT_CHECK(bt_corpus_rows_for_position(&corpus, BT_SHEET_LTCT, "NOPE", indices,
                                         BT_CORPUS_INDEX_MAX) == 0);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, qtm_and_position_agree_with_simcheck_for_all_rows) {
    bt_corpus corpus;
    loaded(&corpus);
    td_simcheck_rows records;
    td_load_simcheck(&records);
    BT_CHECK(records.count == 1077);
    for (size_t i = 0; i < records.count; i++) {
        bt_solver solver;
        bt_sheet_class sheet;
        BT_CHECK(bt_solver_parse(records.cols[i][0], &solver));
        BT_CHECK(bt_sheet_class_parse(records.cols[i][1], &sheet));
        size_t rowid = (size_t)strtoul(records.cols[i][2], NULL, 10);
        const bt_corpus_row *row = bt_corpus_row_at(&corpus, solver, sheet, rowid);
        BT_CHECK(row != NULL);
        if (row == NULL) {
            continue;
        }
        BT_CHECK_EQ(row->position, records.cols[i][3]);
        unsigned long expected_qtm = strtoul(records.cols[i][5], NULL, 10);
        BT_CHECK(bt_alg_qtm(&row->alg) == expected_qtm);
    }
    td_simcheck_free(&records);
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, rows_keep_link_columns_from_the_dump) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 5);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->position, "UF-UB-LB");
        BT_CHECK_EQ(row->alg_text, "U' R E' R' U2 R E R' U'");
        BT_CHECK_EQ(row->commutator, "U':[R E' R',U2]");
        BT_CHECK_EQ(row->recon_link, "https://bit.ly/2Ymdwpw");
        BT_CHECK_EQ(row->youtube_link, "3BLD: 11.89 Single, 14.52 Mo3 (With Commentary)");
    }
    bt_corpus_free(&corpus);
}

BT_TEST(corpus_oracle, missing_root_fails_with_file_context) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    strcat(root, "/does-not-exist");
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(root, &corpus, &err) != 0);
    char text[512];
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "edge.tsv") != NULL);
    BT_CHECK(strstr(text, "does-not-exist") != NULL);
}

BT_TEST(corpus_oracle, unparsable_alg_fails_with_sheet_and_rowid) {
    const char *dir = tu_temp_dir("bt-data-corpus", "badalg");
    char sheet_dir[BT_TU_PATH_MAX];
    snprintf(sheet_dir, sizeof sheet_dir, "%s/cherry", dir);
    tu_mkdir_p(sheet_dir);
    char sheet[BT_TU_PATH_MAX];
    snprintf(sheet, sizeof sheet, "%s/edge.tsv", sheet_dir);
    tu_write_file(sheet, "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
                         "A-B-C\tQ9\tNot found.\t\t\n");
    bt_corpus corpus;
    bt_corpus_error err;
    BT_CHECK(bt_corpus_load(dir, &corpus, &err) != 0);
    char text[512];
    bt_corpus_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "rowid 2") != NULL);
    tu_remove_tree(dir);
}
