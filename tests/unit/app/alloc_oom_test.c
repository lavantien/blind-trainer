#include "app/config.h"
#include "app/persist.h"
#include "app/usage_script.h"
#include "app/views/algs/browse.h"
#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/menu.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "app/views/stats.h"

#include "common.h"
#include "common/alloc.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

BT_TEST(alloc_oom, str_list_push_fails_closed_on_both_allocation_arms) {
    bt_str_list list;
    bt_str_list_init(&list);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_push(&list, "alpha"), -1);
    BT_CHECK_EQ(list.count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_push(&list, "alpha"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_push(&list, "beta"), -1);
    BT_CHECK_EQ(list.count, 1U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_push(&list, "beta"), 0);
    BT_CHECK_EQ(list.count, 2U);
    bt_str_list_free(&list);
}

BT_TEST(alloc_oom, str_list_sorted_insert_fails_closed_under_failure) {
    bt_str_list list;
    bt_str_list_init(&list);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "b"), -1);
    BT_CHECK_EQ(list.count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "b"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "a"), -1);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_str_list_sorted_insert_unique(&list, "a"), 0);
    BT_CHECK_EQ(list.count, 2U);
    bt_str_list_free(&list);
}

BT_TEST(alloc_oom, syllable_book_insert_fails_closed_on_every_allocation_arm) {
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "lai"), -1);
    BT_CHECK_EQ(book.count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "lai"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "edge", "AB", "mo"), -1);
    BT_CHECK(strcmp(bt_syllable_book_for(&book, "edge", "AB"), "lai") == 0);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), -1);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(1);
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), -1);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_syllable_book_insert(&book, "corner", "CD", "nhu"), 0);
    BT_CHECK_EQ(book.count, 2U);
    bt_syllable_book_free(&book);
}

BT_TEST(alloc_oom, solve_broken_push_fails_closed_and_keeps_the_count) {
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_solve_outcome_push_broken(&outcome, "UB"), -1);
    BT_CHECK_EQ(outcome.broken_count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_solve_outcome_push_broken(&outcome, "UB"), 0);
    BT_CHECK_EQ(outcome.broken_count, 1U);
    bt_solve_outcome_free(&outcome);
}

BT_TEST(alloc_oom, menu_warning_context_returns_null_under_allocation_failure) {
    bt_str_list warnings;
    bt_str_list_init(&warnings);
    BT_CHECK_EQ(bt_str_list_push(&warnings, "the sq1 pack lost a row and hid its event"), 0);
    BT_CHECK_EQ(bt_str_list_push(&warnings, "the megaminx pack lost a row and hid its event"), 0);
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_view_menu_warning_context(&warnings) == NULL);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(1);
    BT_CHECK(bt_view_menu_warning_context(&warnings) == NULL);
    bt_alloc_fail_disarm();
    char *joined = bt_view_menu_warning_context(&warnings);
    BT_CHECK(joined != NULL);
    BT_CHECK(strstr(joined, "sq1 pack") != NULL);
    BT_CHECK(strstr(joined, "megaminx pack") != NULL);
    free(joined);
    bt_str_list_free(&warnings);
}

BT_TEST(alloc_oom, repo_browser_default_survives_a_failed_arms_allocation) {
    bt_repo_browser browser;
    bt_alloc_fail_arm(0);
    bt_repo_browser_default(&browser);
    bt_alloc_fail_disarm();
    BT_CHECK(bt_repo_browser_has_seed_arms(&browser) == false);
    bt_repo_browser_free(&browser);
}

BT_TEST(alloc_oom, repo_arms_survives_allocation_failure_at_every_depth) {
    bt_pack_entry packs[2];
    memset(packs, 0, sizeof packs);
    snprintf(packs[0].event, sizeof packs[0].event, "3x3");
    snprintf(packs[0].method, sizeof packs[0].method, "cfop");
    snprintf(packs[0].stage, sizeof packs[0].stage, "pll");
    snprintf(packs[1].event, sizeof packs[1].event, "2x2");
    snprintf(packs[1].method, sizeof packs[1].method, "lbl");
    snprintf(packs[1].stage, sizeof packs[1].stage, "oll");
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    for (size_t ok = 0; ok <= 16; ok++) {
        bt_alloc_fail_arm(ok);
        size_t arm_count = 99;
        bt_repo_node *arms = bt_repo_arms(packs, 2, &hidden, NULL, 0, &arm_count);
        bt_alloc_fail_disarm();
        if (arms != NULL) {
            for (size_t i = 0; i < arm_count; i++) {
                bt_repo_node_free(&arms[i]);
            }
            free(arms);
        } else {
            BT_CHECK_EQ(arm_count, 0U);
        }
    }
    bt_str_list_free(&hidden);
}

BT_TEST(alloc_oom, usage_script_degrades_to_a_prefix_under_allocation_failure) {
    size_t full_count = 0;
    bt_usage_action *full = bt_usage_script(0.05F, &full_count);
    BT_CHECK(full != NULL);
    BT_CHECK(full_count > 40);
    size_t count = 999;
    bt_alloc_fail_arm(0);
    bt_usage_action *script = bt_usage_script(0.05F, &count);
    bt_alloc_fail_disarm();
    BT_CHECK(script == NULL);
    BT_CHECK_EQ(count, 0U);
    free(script);
    count = 999;
    bt_alloc_fail_arm(1);
    script = bt_usage_script(0.05F, &count);
    bt_alloc_fail_disarm();
    BT_CHECK(script != NULL);
    BT_CHECK(count > 0);
    BT_CHECK(count < full_count);
    BT_CHECK_EQ(script[0].kind, full[0].kind);
    free(script);
    free(full);
}

static bt_card oom_card_with_reviews(const unsigned int *qualities, size_t count) {
    bt_card card;
    memset(&card, 0, sizeof card);
    card.interval_days = 1;
    card.due_day = 0;
    card.reps = (unsigned int)count;
    card.review_count = count;
    card.review_cap = count;
    card.reviews = malloc(count * sizeof(bt_review_entry));
    if (card.reviews == NULL) {
        BT_ABORT("fixture reviews");
    }
    for (size_t i = 0; i < count; i++) {
        card.reviews[i].ts_days = 100 + (long long)i;
        card.reviews[i].quality = qualities[i];
        card.reviews[i].elapsed_days = 0;
        card.reviews[i].scheduled_days = 1;
    }
    return card;
}

static void oom_stores_with(const char *key, const bt_card *card, const bt_solve_record *solves,
                            size_t solve_count, bt_stores *out) {
    memset(out, 0, sizeof *out);
    out->settings.memo_cadence_ms = 800;
    out->settings.memo_mode = BT_MEMO_SYLLABLES;
    out->settings.drill_reps = 5;
    bt_srs_store_init(&out->srs);
    bt_srs_store_set(&out->srs, key, card);
    bt_solve_log_init(&out->solves);
    for (size_t i = 0; i < solve_count; i++) {
        bt_solve_log_push(&out->solves, solves[i]);
    }
    bt_user_algs_init(&out->user_algs);
    bt_str_list_init(&out->seeds.hidden);
    bt_str_list_init(&out->seeds.warnings);
}

BT_TEST(alloc_oom, stats_streak_fails_closed_on_both_allocation_arms) {
    const unsigned int qualities[2] = {4, 5};
    bt_card card = oom_card_with_reviews(qualities, 2);
    bt_stores stores;
    oom_stores_with("edge/UF-UB-UR", &card, NULL, 0, &stores);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_stats_streak_of(&stores), 0U);
    bt_alloc_fail_disarm();
    bt_alloc_fail_arm(1);
    BT_CHECK_EQ(bt_stats_streak_of(&stores), 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_stats_streak_of(&stores), 2U);
    bt_stores_free(&stores);
    free(card.reviews);
}

BT_TEST(alloc_oom, stats_due_events_fail_closed_on_the_grow_arm) {
    bt_case_card_entry entries[2];
    memset(entries, 0, sizeof entries);
    snprintf(entries[0].ref.event, sizeof entries[0].ref.event, "2x2");
    snprintf(entries[1].ref.event, sizeof entries[1].ref.event, "skewb");
    bt_case_cards cards;
    cards.items = entries;
    cards.count = 2;
    cards.cap = 2;
    bt_stats_due *out = (bt_stats_due *)0x1;
    size_t count = 99;
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_stats_due_events(&cards, 0, &out, &count), -1);
    BT_CHECK(out == NULL);
    BT_CHECK_EQ(count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_stats_due_events(&cards, 0, &out, &count), 0);
    BT_CHECK_EQ(count, 2U);
    BT_CHECK(out != NULL);
    free(out);
}

BT_TEST(alloc_oom, stats_snapshot_fails_closed_when_any_split_array_fails) {
    const unsigned int qualities[1] = {4};
    bt_card card = oom_card_with_reviews(qualities, 1);
    bt_solve_record solves[1] = {{1, 20000, 10000, 1}};
    bt_stores stores;
    oom_stores_with("edge/UF-UB-UR", &card, solves, 1, &stores);
    bt_case_cards cards;
    memset(&cards, 0, sizeof cards);
    for (size_t ok = 0; ok < 4; ok++) {
        bt_stats_snapshot snap;
        bt_alloc_fail_arm(ok);
        bt_stats_snapshot_make(&stores, &cards, 0, &snap);
        bt_alloc_fail_disarm();
        BT_CHECK_EQ(snap.memo_mean_ms, 0.0);
        BT_CHECK_EQ(snap.exec_mean_ms, 0.0);
        BT_CHECK_EQ(snap.memo_median_ms, 0.0);
        BT_CHECK_EQ(snap.exec_median_ms, 0.0);
        BT_CHECK_EQ(snap.streak, 0U);
        bt_stats_snapshot_free(&snap);
    }
    bt_stats_snapshot snap;
    bt_stats_snapshot_make(&stores, &cards, 0, &snap);
    BT_CHECK_EQ(snap.memo_mean_ms, 20000.0);
    BT_CHECK_EQ(snap.exec_mean_ms, 10000.0);
    BT_CHECK_EQ(snap.streak, 1U);
    bt_stats_snapshot_free(&snap);
    bt_stores_free(&stores);
    free(card.reviews);
}

BT_TEST(alloc_oom, stats_blocks_fail_closed_when_the_block_array_fails) {
    bt_stats_snapshot snap;
    memset(&snap, 0, sizeof snap);
    size_t count = 99;
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_view_stats_blocks(&snap, &count) == NULL);
    BT_CHECK_EQ(count, 0U);
    bt_alloc_fail_disarm();
    bt_stats_block *blocks = bt_view_stats_blocks(&snap, &count);
    BT_CHECK(blocks != NULL);
    BT_CHECK_EQ(count, (size_t)BT_STATS_LAYOUT_BLOCKS);
    free(blocks);
}

BT_TEST(alloc_oom, config_read_fails_closed_when_the_text_buffer_fails) {
    const char *dir = ta_temp_dir("alloc_oom", "config");
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/config.json", dir);
    ta_write_file(path, "{\"app\":{}}");
    bt_json_value *value = NULL;
    char error[128];
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_app_load_config_value(path, &value, error, sizeof error), -1);
    BT_CHECK(strstr(error, "cannot read config") != NULL);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_app_load_config_value(path, &value, error, sizeof error), 0);
    bt_json_free(value);
}

BT_TEST(alloc_oom, drill_push_split_fails_closed_on_the_grow_arm) {
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_drill_push_split(&session, 1, 1.5F), -1);
    BT_CHECK_EQ(session.split_count, 0U);
    bt_alloc_fail_disarm();
    for (int i = 0; i < 8; i++) {
        BT_CHECK_EQ(bt_drill_push_split(&session, 1, 1.0F), 0);
    }
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_drill_push_split(&session, 0, 2.0F), -1);
    BT_CHECK_EQ(session.split_count, 8U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_drill_push_split(&session, 0, 2.0F), 0);
    BT_CHECK_EQ(session.split_count, 9U);
    bt_drill_session_free(&session);
}

BT_TEST(alloc_oom, case_drill_queue_push_and_set_fail_closed_on_their_grow_arms) {
    bt_drill_case_queue queue;
    bt_drill_case_queue_init(&queue);
    bt_drill_case item;
    memset(&item, 0, sizeof item);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_drill_case_queue_push(&queue, &item), -1);
    BT_CHECK_EQ(queue.count, 0U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_drill_case_queue_push(&queue, &item), 0);
    bt_drill_case many[20];
    memset(many, 0, sizeof many);
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_drill_case_queue_set(&queue, many, 20), -1);
    BT_CHECK_EQ(queue.count, 1U);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_drill_case_queue_set(&queue, many, 20), 0);
    BT_CHECK_EQ(queue.count, 20U);
    bt_drill_case_queue_free(&queue);
}

BT_TEST(alloc_oom, case_drill_and_algs_singletons_return_null_when_the_first_allocation_fails) {
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_view_case_drill_app() == NULL);
    bt_alloc_fail_disarm();
    BT_CHECK(bt_view_case_drill_app() != NULL);
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_view_algs_app() == NULL);
    bt_alloc_fail_disarm();
    BT_CHECK(bt_view_algs_app() != NULL);
}

static bt_corpus_row oom_edge_row(const char *alg, long long rowid) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = rowid;
    row.position = _strdup("UF-UB-UR");
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup("");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

BT_TEST(alloc_oom, algs_case_rows_dyn_fails_closed_when_the_row_array_grows) {
    bt_corpus_row rows[2] = {oom_edge_row("R U R'", 1), oom_edge_row("R2 D2", 2)};
    bt_corpus_row *heap = malloc(2 * sizeof *heap);
    memcpy(heap, rows, 2 * sizeof *heap);
    bt_corpus corpus;
    bt_corpus_from_rows(heap, 2, &corpus);
    bt_case all[BT_ALGS_CASES_MAX];
    size_t all_count = bt_view_algs_all_cases(all);
    const bt_case *match = NULL;
    char position[BT_DRILL_TEXT_MAX];
    for (size_t i = 0; i < all_count; i++) {
        bt_view_case_position(&all[i], position);
        if (strcmp(position, "UF-UB-UR") == 0) {
            match = &all[i];
            break;
        }
    }
    BT_CHECK(match != NULL);
    const bt_corpus_row **out_rows = (const bt_corpus_row **)0x1;
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_view_algs_case_rows_dyn(&corpus, match, &out_rows), 0U);
    BT_CHECK(out_rows == NULL);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(bt_view_algs_case_rows_dyn(&corpus, match, &out_rows), 2U);
    BT_CHECK(out_rows != NULL);
    free(out_rows);
    bt_corpus_free(&corpus);
}

BT_TEST(alloc_oom, algs_detail_lines_fail_closed_when_the_first_line_grow_fails) {
    bt_view_algs_browser browser;
    memset(&browser, 0, sizeof browser);
    browser.all_count = 5;
    browser.filtered_count = 0;
    browser.selected = 0;
    bt_view_algs_detail_line *lines = (bt_view_algs_detail_line *)0x1;
    bt_alloc_fail_arm(0);
    BT_CHECK_EQ(bt_view_algs_detail_lines(NULL, &browser, &lines), 0U);
    BT_CHECK(lines == NULL);
    bt_alloc_fail_disarm();
    size_t count = bt_view_algs_detail_lines(NULL, &browser, &lines);
    BT_CHECK_EQ(count, 2U);
    BT_CHECK(lines != NULL);
    free(lines);
}
