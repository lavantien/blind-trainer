#include "app/views/stats.h"
#include "app/views/state.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_card card_with_reviews(const unsigned int *qualities, size_t count) {
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

static void stores_with(const char *(*cards)[2], const bt_card *card_values, size_t card_count,
                        const bt_solve_record *solves, size_t solve_count, bt_stores *out) {
    memset(out, 0, sizeof *out);
    out->settings.memo_cadence_ms = 800;
    out->settings.memo_mode = BT_MEMO_SYLLABLES;
    out->settings.drill_reps = 5;
    bt_srs_store_init(&out->srs);
    for (size_t i = 0; i < card_count; i++) {
        bt_srs_store_set(&out->srs, cards[i][0], &card_values[i]);
    }
    bt_solve_log_init(&out->solves);
    for (size_t i = 0; i < solve_count; i++) {
        bt_solve_log_push(&out->solves, solves[i]);
    }
    bt_user_algs_init(&out->user_algs);
    bt_str_list_init(&out->seeds.hidden);
    bt_str_list_init(&out->seeds.warnings);
}

static void case_fixture_cards(const char *(*refs)[4], const bt_card *card_values, size_t count,
                               bt_case_cards *out) {
    bt_db db;
    bt_db_error err;
    if (bt_db_open_memory(&db, &err) != 0) {
        char text[BT_PATH_MAX];
        bt_db_error_string(&err, text, sizeof text);
        BT_ABORT("fixture db: %s", text);
    }
    for (size_t i = 0; i < count; i++) {
        bt_case_ref ref;
        bt_case_ref_of(&ref, refs[i][0], refs[i][1], refs[i][2], refs[i][3]);
        if (bt_repo_cards_save_card(&db, &ref, &card_values[i], &err) != 0) {
            char text[BT_PATH_MAX];
            bt_db_error_string(&err, text, sizeof text);
            BT_ABORT("fixture card: %s", text);
        }
    }
    bt_fsrs_config fsrs;
    bt_fsrs_config_defaults(&fsrs);
    bt_srs_load_error load_err;
    if (bt_repo_cards_load(&db, &fsrs, out, &load_err) != 0) {
        char text[BT_PATH_MAX];
        bt_srs_load_error_string(&load_err, text, sizeof text);
        BT_ABORT("fixture load: %s", text);
    }
    bt_db_close(&db);
}

BT_TEST(view_stats, snapshot_aggregates_splits_and_due_counts) {
    const unsigned int edge_q[2] = {4, 5};
    const unsigned int corner_q[1] = {2};
    const unsigned int due_q[1] = {4};
    const char *keys[2][2] = {{"edge/UF-UB-UR", NULL}, {"corner/UFR-UBL-UBR", NULL}};
    bt_card values[2] = {card_with_reviews(edge_q, 2), card_with_reviews(corner_q, 1)};
    bt_solve_record solves[2] = {{1, 20000, 10000, 1}, {2, 30000, 20000, 0}};
    bt_stores stores;
    stores_with(keys, values, 2, solves, 2, &stores);
    bt_card future = card_with_reviews(due_q, 1);
    future.due_day = 900;
    const char *refs[3][4] = {{"2x2", "cll", "cll", "cll.1"},
                              {"2x2", "cll", "cll", "cll.2"},
                              {"skewb", "sarah", "sarah", "s.1"}};
    bt_card card_values[3] = {card_with_reviews(due_q, 1), future, card_with_reviews(corner_q, 1)};
    bt_case_cards cards;
    case_fixture_cards(refs, card_values, 3, &cards);

    bt_stats_snapshot snap;
    bt_stats_snapshot_make(&stores, &cards, 0, &snap);
    BT_CHECK(snap.solves == 2);
    BT_CHECK(snap.memo_mean_ms == 25000.0);
    BT_CHECK(snap.memo_median_ms == 25000.0);
    BT_CHECK(snap.exec_mean_ms == 15000.0);
    BT_CHECK(snap.exec_median_ms == 15000.0);
    BT_CHECK(snap.due_edge == 1);
    BT_CHECK(snap.due_corner == 1);
    BT_CHECK(snap.streak == 2);
    BT_CHECK(snap.due_event_count == 2);
    BT_CHECK(snap.due_events != NULL && strcmp(snap.due_events[0].event, "2x2") == 0 &&
             snap.due_events[0].due == 1);
    BT_CHECK(snap.due_events != NULL && strcmp(snap.due_events[1].event, "skewb") == 0 &&
             snap.due_events[1].due == 1);
    bt_stats_snapshot_free(&snap);
    bt_case_cards_free(&cards);
    bt_stores_free(&stores);
}

BT_TEST(view_stats, due_events_count_per_event_in_card_order) {
    const unsigned int due_q[1] = {4};
    bt_card future = card_with_reviews(due_q, 1);
    future.due_day = 900;
    const char *refs[3][4] = {{"skewb", "sarah", "sarah", "s.1"},
                              {"2x2", "cll", "cll", "cll.1"},
                              {"2x2", "cll", "cll", "cll.2"}};
    bt_card card_values[3] = {card_with_reviews(due_q, 1), card_with_reviews(due_q, 1), future};
    bt_case_cards cards;
    case_fixture_cards(refs, card_values, 3, &cards);
    bt_stats_due *events = NULL;
    size_t count = 0;
    BT_CHECK(bt_stats_due_events(&cards, 0, &events, &count) == 0);
    BT_CHECK(count == 2);
    BT_CHECK(strcmp(events[0].event, "2x2") == 0 && events[0].due == 1);
    BT_CHECK(strcmp(events[1].event, "skewb") == 0 && events[1].due == 1);
    free(events);
    events = NULL;
    count = 0;
    bt_case_cards empty;
    bt_case_cards_init(&empty);
    BT_CHECK(bt_stats_due_events(&empty, 5, &events, &count) == 0);
    BT_CHECK(count == 0 && events == NULL);
    BT_CHECK(bt_stats_due_events(&cards, 900, &events, &count) == 0);
    BT_CHECK(count == 2);
    BT_CHECK(strcmp(events[0].event, "2x2") == 0 && events[0].due == 2);
    BT_CHECK(strcmp(events[1].event, "skewb") == 0 && events[1].due == 1);
    free(events);
    bt_case_cards_free(&cards);
}

BT_TEST(view_stats, streak_counts_trailing_successful_reviews_across_cards) {
    const unsigned int five[1] = {5};
    const unsigned int fours[2] = {4, 4};
    const char *keys[2][2] = {{"edge/A", NULL}, {"corner/B", NULL}};
    bt_card values[2] = {card_with_reviews(five, 1), card_with_reviews(fours, 2)};
    bt_stores stores;
    stores_with(keys, values, 2, NULL, 0, &stores);
    BT_CHECK(bt_stats_streak_of(&stores) == 3);
    bt_stores_free(&stores);
    const unsigned int broken_q[3] = {4, 2, 5};
    const char *broken_keys[1][2] = {{"edge/A", NULL}};
    bt_card broken_values[1] = {card_with_reviews(broken_q, 3)};
    bt_stores broken;
    stores_with(broken_keys, broken_values, 1, NULL, 0, &broken);
    BT_CHECK(bt_stats_streak_of(&broken) == 1);
    bt_stores_free(&broken);
}

BT_TEST(view_stats, median_handles_even_counts) {
    long long even[4] = {3, 1, 2, 4};
    BT_CHECK(bt_stats_median(even, 4) == 2.5);
    long long odd[3] = {5, 1, 3};
    BT_CHECK(bt_stats_median(odd, 3) == 3.0);
    BT_CHECK(bt_stats_median(NULL, 0) == 0.0);
}

BT_TEST(view_stats, blocks_pair_values_with_labels_in_a_fixed_order) {
    bt_stats_due due[1] = {{"2x2", 3}};
    bt_stats_snapshot snap;
    memset(&snap, 0, sizeof snap);
    snap.solves = 2;
    snap.memo_mean_ms = 25000.0;
    snap.memo_median_ms = 25000.0;
    snap.exec_mean_ms = 15000.0;
    snap.exec_median_ms = 15000.0;
    snap.due_edge = 1;
    snap.due_corner = 0;
    snap.streak = 2;
    snap.due_events = due;
    snap.due_event_count = 1;
    size_t count = 0;
    bt_stats_block *blocks = bt_view_stats_blocks(&snap, &count);
    BT_CHECK(blocks != NULL);
    BT_CHECK(count == 9);
    const char *labels[9] = {"solves",         "verified drill streak", "memo mean",
                             "memo median",    "exec mean",             "exec median",
                             "due edge",       "due corner",            "due 2x2"};
    const char *values[9] = {"2", "2", "25.0s", "25.0s", "15.0s", "15.0s", "1", "0", "3"};
    for (size_t i = 0; i < 9; i++) {
        BT_CHECK(strcmp(blocks[i].label, labels[i]) == 0);
        BT_CHECK(strcmp(blocks[i].value, values[i]) == 0);
    }
    const bt_theme *theme = bt_theme_table();
    BT_CHECK(blocks[0].size_rem == theme->font_title);
    BT_CHECK(blocks[3].size_rem == theme->font_heading);
    bt_view_stats_blocks_free(blocks);
}
