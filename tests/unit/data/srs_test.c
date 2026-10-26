#include "common/bt_harness.h"
#include "data/fsrs.h"
#include "data/srs.h"

BT_TEST(srs, new_card_starts_empty_at_the_due_day) {
    bt_card card;
    bt_card_init(&card, 10);
    BT_CHECK(card.has_memory == 0);
    BT_CHECK(card.interval_days == 0);
    BT_CHECK(card.due_day == 10);
    BT_CHECK(card.reps == 0);
    BT_CHECK(card.lapses == 0);
    BT_CHECK(card.review_count == 0);
    long long day = 0;
    BT_CHECK(bt_card_last_review_day(&card, &day) == 0);
    bt_card_free(&card);
}

BT_TEST(srs, review_good_records_the_review_and_memory) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 10);
    bt_card_review(&card, BT_GRADE_GOOD, 42, &cfg);
    long long day = 0;
    BT_CHECK(bt_card_last_review_day(&card, &day) == 1);
    BT_CHECK(day == 42);
    BT_CHECK(card.has_memory == 1);
    BT_CHECK(bt_fsrs_memory_is_valid(card.memory) == 1);
    BT_CHECK(card.review_count == 1);
    BT_CHECK(card.reviews[0].quality == 4);
    BT_CHECK(card.reps == 1);
    BT_CHECK(card.lapses == 0);
    bt_card_free(&card);
}

BT_TEST(srs, review_again_increments_lapses) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_AGAIN, 0, &cfg);
    BT_CHECK(card.lapses == 1);
    BT_CHECK(card.reps == 1);
    bt_card_free(&card);
}

BT_TEST(srs, is_due_at_or_after_the_due_day) {
    bt_card card;
    bt_card_init(&card, 10);
    BT_CHECK(bt_card_is_due(&card, 9) == 0);
    BT_CHECK(bt_card_is_due(&card, 10) == 1);
    BT_CHECK(bt_card_is_due(&card, 11) == 1);
    bt_card_free(&card);
}

BT_TEST(srs, grade_from_quality_maps_sm2_codes) {
    bt_grade grade;
    BT_CHECK(bt_grade_from_quality(2, &grade) == 0);
    BT_CHECK(grade == BT_GRADE_AGAIN);
    BT_CHECK(bt_grade_from_quality(3, &grade) == 0);
    BT_CHECK(grade == BT_GRADE_HARD);
    BT_CHECK(bt_grade_from_quality(4, &grade) == 0);
    BT_CHECK(grade == BT_GRADE_GOOD);
    BT_CHECK(bt_grade_from_quality(5, &grade) == 0);
    BT_CHECK(grade == BT_GRADE_EASY);
    BT_CHECK(bt_grade_from_quality(7, &grade) != 0);
    BT_CHECK(bt_grade_from_quality(0, &grade) != 0);
}

BT_TEST(srs, replay_of_no_reviews_yields_no_memory) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_fsrs_memory out;
    int has = 1;
    BT_CHECK(bt_replay_sm2(NULL, 0, &cfg, &has, &out) == 0);
    BT_CHECK(has == 0);
}

BT_TEST(srs, review_log_quality_is_the_grade_code_plus_one) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_HARD, 0, &cfg);
    BT_CHECK(card.reviews[0].quality == 3);
    bt_card_free(&card);
}

BT_TEST(srs, cards_clone_and_compare_deep) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&card, BT_GRADE_EASY, 3, &cfg);
    bt_card copy;
    BT_CHECK(bt_card_clone(&card, &copy) == 0);
    BT_CHECK(bt_card_eq(&card, &copy) == 1);
    copy.due_day += 1;
    BT_CHECK(bt_card_eq(&card, &copy) == 0);
    bt_card_free(&card);
    bt_card_free(&copy);
}
