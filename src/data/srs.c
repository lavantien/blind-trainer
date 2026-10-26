#include "data/srs.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

void bt_card_init(bt_card *card, long long now_days) {
    BT_TRACE_ENTER("srs.card_init");
    memset(card, 0, sizeof *card);
    card->due_day = now_days;
    BT_TRACE_EXIT("srs.card_init");
}

void bt_card_free(bt_card *card) {
    BT_TRACE_ENTER("srs.card_free");
    free(card->reviews);
    card->reviews = NULL;
    card->review_count = 0;
    card->review_cap = 0;
    BT_TRACE_EXIT("srs.card_free");
}

int bt_card_clone(const bt_card *src, bt_card *dst) {
    BT_TRACE_ENTER("srs.card_clone");
    *dst = *src;
    dst->reviews = NULL;
    if (src->review_count > 0) {
        dst->reviews =
            malloc(src->review_count * sizeof *dst->reviews);
        if (dst->reviews == NULL) {
            BT_TRACE_FAIL("srs.card_clone", "out of memory for %zu reviews", src->review_count);
            return -1;
        }
        memcpy(dst->reviews, src->reviews, src->review_count * sizeof *dst->reviews);
    }
    dst->review_cap = src->review_count;
    BT_TRACE_EXIT("srs.card_clone");
    return 0;
}

int bt_card_eq(const bt_card *a, const bt_card *b) {
    if (a->has_memory != b->has_memory || a->interval_days != b->interval_days ||
        a->due_day != b->due_day || a->reps != b->reps || a->lapses != b->lapses ||
        a->review_count != b->review_count) {
        return 0;
    }
    if (a->has_memory &&
        (a->memory.stability != b->memory.stability ||
         a->memory.difficulty != b->memory.difficulty)) {
        return 0;
    }
    for (size_t i = 0; i < a->review_count; i++) {
        const bt_review_entry *x = &a->reviews[i];
        const bt_review_entry *y = &b->reviews[i];
        if (x->ts_days != y->ts_days || x->quality != y->quality ||
            x->elapsed_days != y->elapsed_days || x->scheduled_days != y->scheduled_days) {
            return 0;
        }
    }
    return 1;
}

int bt_card_last_review_day(const bt_card *card, long long *out_day) {
    if (card->review_count == 0) {
        return 0;
    }
    *out_day = card->reviews[card->review_count - 1].ts_days;
    return 1;
}

int bt_card_is_due(const bt_card *card, long long now_days) {
    return now_days >= card->due_day;
}

int bt_grade_from_quality(unsigned int quality, bt_grade *out) {
    switch (quality) {
    case 2:
        *out = BT_GRADE_AGAIN;
        return 0;
    case 3:
        *out = BT_GRADE_HARD;
        return 0;
    case 4:
        *out = BT_GRADE_GOOD;
        return 0;
    case 5:
        *out = BT_GRADE_EASY;
        return 0;
    default:
        BT_TRACE_FAIL("srs.grade_from_quality", "quality %u has no mapping", quality);
        return -1;
    }
}

int bt_replay_sm2(const bt_review_entry *entries, size_t count,
                  const bt_fsrs_config *cfg, int *has_out, bt_fsrs_memory *out) {
    BT_TRACE_ENTER("srs.replay_sm2");
    *has_out = 0;
    if (count == 0) {
        BT_TRACE_EXIT("srs.replay_sm2");
        return 0;
    }
    bt_grade grade;
    if (bt_grade_from_quality(entries[0].quality, &grade) != 0) {
        BT_TRACE_FAIL("srs.replay_sm2", "quality %u has no grade", entries[0].quality);
        return -1;
    }
    bt_fsrs_memory memory = bt_fsrs_memory_initial(grade, cfg);
    for (size_t i = 1; i < count; i++) {
        if (bt_grade_from_quality(entries[i].quality, &grade) != 0) {
            BT_TRACE_FAIL("srs.replay_sm2", "quality %u has no grade", entries[i].quality);
            return -1;
        }
        memory = bt_fsrs_memory_review(memory, grade, (double)entries[i].elapsed_days, cfg);
    }
    *has_out = 1;
    *out = memory;
    BT_TRACE_EXIT("srs.replay_sm2");
    return 0;
}

static int push_review(bt_card *card, bt_review_entry entry) {
    if (card->review_count == card->review_cap) {
        size_t cap = card->review_cap == 0 ? 4 : card->review_cap * 2;
        bt_review_entry *grown = realloc(card->reviews, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        card->reviews = grown;
        card->review_cap = cap;
    }
    card->reviews[card->review_count++] = entry;
    return 0;
}

void bt_card_review(bt_card *card, bt_grade rating, long long now_days,
                    const bt_fsrs_config *cfg) {
    BT_TRACE_ENTER("srs.card_review");
    long long elapsed_days = 0;
    if (card->review_count > 0) {
        long long last = card->reviews[card->review_count - 1].ts_days;
        elapsed_days = now_days - last > 0 ? now_days - last : 0;
    }
    bt_fsrs_memory memory;
    if (card->review_count == 0) {
        memory = bt_fsrs_memory_initial(rating, cfg);
    } else if (card->has_memory) {
        memory = bt_fsrs_memory_review(card->memory, rating, (double)elapsed_days, cfg);
    } else {
        bt_fsrs_memory replayed;
        int has = 0;
        if (bt_replay_sm2(card->reviews, card->review_count, cfg, &has, &replayed) == 0 &&
            has) {
            memory = bt_fsrs_memory_review(replayed, rating, (double)elapsed_days, cfg);
        } else {
            memory = bt_fsrs_memory_initial(rating, cfg);
        }
    }
    card->has_memory = 1;
    card->memory = memory;
    card->reps += 1;
    if (rating == BT_GRADE_AGAIN) {
        card->lapses += 1;
    }
    card->interval_days = bt_fsrs_next_interval_days(memory.stability, cfg);
    card->due_day = now_days + card->interval_days;
    bt_review_entry entry = {
        now_days,
        (unsigned int)rating + 1,
        elapsed_days,
        card->interval_days,
    };
    push_review(card, entry);
    BT_TRACE_EXIT("srs.card_review");
}
