#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "core/alg.h"
#include "data/commutator.h"
#include "data/fsrs.h"
#include "data/srs.h"
#include "data/tsv.h"
#include "data/verify.h"

static const char *SHEET_TEXT =
    "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
    "UF-UB-UR\tR2 U' S R2 S' R2 U R2\tR2 U':[S,R2]\tlink one\tvideo one\n"
    "UF-UB-UL\tU M2 U M' U2 M U M2 U'\tU M2 U:[M',U2]\tlink two\tvideo two\n";

static const char *BAD_HEADER_TEXT =
    "Position\tAlg\tCommutator\tReconstruction Link\tChannel\n"
    "UF-UB-UR\tR2 U' S R2 S' R2 U R2\tR2 U':[S,R2]\tlink one\tvideo one\n";

static const char *SHORT_ROW_TEXT = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
                                    "UF-UB-UR\tR2 U' S R2 S' R2 U R2\tR2 U':[S,R2]\tlink one\n";

static const char *EMPTY_FIELD_TEXT =
    "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n"
    "UF-UB-UR\t\tR2 U':[S,R2]\tlink one\tvideo one\n";

static bt_alg wrong_alg(void) {
    bt_alg alg;
    bt_alg_init(&alg);
    BT_CHECK(bt_alg_try_parse("R U R'", &alg));
    return alg;
}

BT_TEST(data_oom, verify_fails_closed_when_the_problem_push_starves) {
    bt_alg alg = wrong_alg();
    bt_verdict verdict;
    bt_verdict_init(&verdict);
    bt_verify(&alg, "UF-UB-UR", BT_SHEET_EDGE, &verdict);
    BT_CHECK(!bt_verdict_ok(&verdict));
    bt_verdict_free(&verdict);
    bt_alloc_fail_arm(0);
    bt_verdict starved;
    bt_verdict_init(&starved);
    bt_verify(&alg, "UF-UB-UR", BT_SHEET_EDGE, &starved);
    bt_alloc_fail_disarm();
    BT_CHECK(!bt_verdict_ok(&starved));
    BT_CHECK_EQ((int)bt_alloc_fail_count() > 0, 1);
    char text[128];
    bt_verdict_string(&starved, text, sizeof text);
    BT_CHECK_EQ(text, "out of memory while verifying");
    bt_verdict_free(&starved);
    bt_alg_free(&alg);
}

BT_TEST(data_oom, verify_survives_each_interior_allocation_failure) {
    bt_alg alg = wrong_alg();
    for (size_t ok = 1; ok < 8; ok++) {
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_alloc_fail_arm(ok);
        bt_verify(&alg, "UF-UB-UR", BT_SHEET_EDGE, &verdict);
        bt_alloc_fail_disarm();
        BT_CHECK(!bt_verdict_ok(&verdict));
        bt_verdict_free(&verdict);
    }
    bt_alg_free(&alg);
}

BT_TEST(data_oom, tsv_parse_fails_closed_under_every_allocation_failure) {
    for (size_t ok = 0; ok < 12; ok++) {
        bt_tsv_rows rows;
        bt_tsv_error err;
        bt_alloc_fail_arm(ok);
        int rc = bt_tsv_parse_sheet(SHEET_TEXT, &rows, &err);
        bt_alloc_fail_disarm();
        BT_CHECK(rc != 0);
        bt_tsv_rows_free(&rows);
    }
    bt_tsv_rows rows;
    bt_tsv_error err;
    BT_CHECK_EQ(bt_tsv_parse_sheet(SHEET_TEXT, &rows, &err), 0);
    BT_CHECK_EQ((int)rows.count, 2);
    BT_CHECK_EQ(strcmp(rows.items[0].row.position, "UF-UB-UR"), 0);
    bt_tsv_rows_free(&rows);
}

BT_TEST(data_oom, tsv_error_strings_name_each_rejection_kind) {
    bt_tsv_rows rows;
    bt_tsv_error err;
    char text[128];
    BT_CHECK(bt_tsv_parse_sheet(BAD_HEADER_TEXT, &rows, &err) != 0);
    bt_tsv_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "header must be") != NULL);
    BT_CHECK(bt_tsv_parse_sheet(SHORT_ROW_TEXT, &rows, &err) != 0);
    bt_tsv_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "expected 5 columns, found 4") != NULL);
    BT_CHECK(bt_tsv_parse_sheet(EMPTY_FIELD_TEXT, &rows, &err) != 0);
    bt_tsv_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "Alg column is empty") != NULL);
}

BT_TEST(data_oom, commutator_decompose_fails_closed_under_allocation_failure) {
    for (size_t ok = 0; ok < 16; ok++) {
        bt_expanded_list list;
        bt_expanded_list_init(&list);
        bt_alloc_fail_arm(ok);
        bool ok_out = bt_decompose("[R,U]+[F,D]", &list);
        bt_alloc_fail_disarm();
        BT_CHECK(!ok_out || list.count > 0);
        bt_expanded_list_free(&list);
    }
    bt_expanded_list list;
    bt_expanded_list_init(&list);
    BT_CHECK(bt_decompose("[R,U]+[F,D]", &list));
    BT_CHECK_EQ((int)list.count, 2);
    bt_expanded_list_free(&list);
}

BT_TEST(data_oom, card_clone_fails_closed_when_the_review_copy_starves) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 100);
    bt_card_review(&card, BT_GRADE_GOOD, 100, &cfg);
    bt_card_review(&card, BT_GRADE_EASY, 101, &cfg);
    for (size_t ok = 0; ok < 4; ok++) {
        bt_card clone;
        bt_alloc_fail_arm(ok);
        int rc = bt_card_clone(&card, &clone);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)clone.review_count, 2);
            bt_card_free(&clone);
        }
    }
    bt_card clone;
    bt_alloc_fail_arm(1000);
    BT_CHECK_EQ(bt_card_clone(&card, &clone), 0);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ((int)clone.review_count, 2);
    bt_card_free(&clone);
    bt_card_free(&card);
}

BT_TEST(data_oom, card_review_keeps_the_log_intact_when_the_growth_realloc_starves) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 100);
    for (long long day = 100; day < 104; day++) {
        bt_card_review(&card, BT_GRADE_GOOD, day, &cfg);
    }
    size_t before = card.review_count;
    unsigned int reps = card.reps;
    long long due = card.due_day;
    BT_CHECK_EQ((int)card.review_cap, 4);
    bt_alloc_fail_arm(0);
    bt_card_review(&card, BT_GRADE_HARD, 104, &cfg);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ((int)bt_alloc_fail_count() > 0, 1);
    BT_CHECK_EQ((int)card.review_count, (int)before);
    BT_CHECK_EQ(card.reps, reps);
    BT_CHECK_EQ(card.due_day, due);
    bt_card_review(&card, BT_GRADE_EASY, 105, &cfg);
    BT_CHECK_EQ((int)card.review_count, (int)before + 1);
    bt_card_free(&card);
}
