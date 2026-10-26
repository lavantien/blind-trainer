#ifndef BT_DATA_SRS_H
#define BT_DATA_SRS_H

#include "data/fsrs.h"

#include <stddef.h>

typedef struct {
    long long ts_days;
    unsigned int quality;
    long long elapsed_days;
    long long scheduled_days;
} bt_review_entry;

typedef struct {
    int has_memory;
    bt_fsrs_memory memory;
    long long interval_days;
    long long due_day;
    unsigned int reps;
    unsigned int lapses;
    bt_review_entry *reviews;
    size_t review_count;
    size_t review_cap;
} bt_card;

void bt_card_init(bt_card *card, long long now_days);
void bt_card_free(bt_card *card);
int bt_card_clone(const bt_card *src, bt_card *dst);
int bt_card_eq(const bt_card *a, const bt_card *b);
void bt_card_review(bt_card *card, bt_grade rating, long long now_days,
                    const bt_fsrs_config *cfg);
int bt_card_last_review_day(const bt_card *card, long long *out_day);
int bt_card_is_due(const bt_card *card, long long now_days);
int bt_grade_from_quality(unsigned int quality, bt_grade *out);
int bt_replay_sm2(const bt_review_entry *entries, size_t count,
                  const bt_fsrs_config *cfg, int *has_out, bt_fsrs_memory *out);

#endif
