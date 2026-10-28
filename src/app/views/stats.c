#include "app/views/stats.h"

#include "app/theme.h"
#include "app/views/frame.h"
#include "app/views/state.h"
#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    long long day;
    size_t seq;
    unsigned int quality;
} bt_stats_review;

typedef struct {
    const char *key;
    size_t seq;
} bt_stats_card_order;

static int review_cmp(const void *a, const void *b) {
    const bt_stats_review *left = a;
    const bt_stats_review *right = b;
    if (left->day != right->day) {
        return left->day < right->day ? -1 : 1;
    }
    if (left->seq != right->seq) {
        return left->seq < right->seq ? -1 : 1;
    }
    return 0;
}

static int card_order_cmp(const void *a, const void *b) {
    const bt_stats_card_order *left = a;
    const bt_stats_card_order *right = b;
    int cmp = strcmp(left->key, right->key);
    if (cmp != 0) {
        return cmp;
    }
    if (left->seq != right->seq) {
        return left->seq < right->seq ? -1 : 1;
    }
    return 0;
}

double bt_stats_median(long long *values, size_t count) {
    BT_TRACE_ENTER("app.stats_median");
    if (count == 0) {
        BT_TRACE_EXIT("app.stats_median");
        return 0.0;
    }
    for (size_t i = 1; i < count; i++) {
        long long value = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > value) {
            values[j] = values[j - 1];
            j--;
        }
        values[j] = value;
    }
    size_t mid = count / 2;
    double out =
        count % 2 == 1 ? (double)values[mid] : (double)(values[mid - 1] + values[mid]) / 2.0;
    BT_TRACE_EXIT("app.stats_median");
    return out;
}

double bt_stats_mean(const long long *values, size_t count) {
    BT_TRACE_ENTER("app.stats_mean");
    if (count == 0) {
        BT_TRACE_EXIT("app.stats_mean");
        return 0.0;
    }
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += (double)values[i];
    }
    BT_TRACE_EXIT("app.stats_mean");
    return sum / (double)count;
}

size_t bt_stats_streak_of(const bt_stores *stores) {
    BT_TRACE_ENTER("app.stats_streak_of");
    if (stores->srs.count == 0) {
        BT_TRACE_EXIT("app.stats_streak_of");
        return 0;
    }
    bt_stats_card_order *order = malloc(stores->srs.count * sizeof *order);
    if (order == NULL) {
        BT_TRACE_FAIL("app.stats_streak_of", "out of memory");
        BT_TRACE_EXIT("app.stats_streak_of");
        return 0;
    }
    for (size_t i = 0; i < stores->srs.count; i++) {
        order[i].key = stores->srs.items[i].key;
        order[i].seq = i;
    }
    qsort(order, stores->srs.count, sizeof *order, card_order_cmp);
    size_t review_total = 0;
    for (size_t i = 0; i < stores->srs.count; i++) {
        review_total += stores->srs.items[i].card.review_count;
    }
    bt_stats_review *reviews = review_total == 0 ? NULL : malloc(review_total * sizeof *reviews);
    if (reviews == NULL && review_total > 0) {
        free(order);
        BT_TRACE_FAIL("app.stats_streak_of", "out of memory");
        BT_TRACE_EXIT("app.stats_streak_of");
        return 0;
    }
    size_t at = 0;
    for (size_t i = 0; i < stores->srs.count; i++) {
        const bt_card *card = &stores->srs.items[order[i].seq].card;
        for (size_t r = 0; r < card->review_count; r++) {
            reviews[at].day = card->reviews[r].ts_days;
            reviews[at].seq = at;
            reviews[at].quality = card->reviews[r].quality;
            at++;
        }
    }
    qsort(reviews, at, sizeof *reviews, review_cmp);
    size_t streak = 0;
    for (size_t i = at; i > 0; i--) {
        if (reviews[i - 1].quality >= 3) {
            streak++;
        } else {
            break;
        }
    }
    free(reviews);
    free(order);
    BT_TRACE_EXIT("app.stats_streak_of");
    return streak;
}

int bt_stats_due_events(const bt_case_cards *cards, long long now_day, bt_stats_due **out,
                        size_t *count) {
    BT_TRACE_ENTER("app.stats_due_events");
    *out = NULL;
    *count = 0;
    size_t cap = 0;
    for (size_t i = 0; i < cards->count; i++) {
        if (!bt_card_is_due(&cards->items[i].card, now_day)) {
            continue;
        }
        const char *event = cards->items[i].ref.event;
        size_t slot = *count;
        for (size_t j = 0; j < *count; j++) {
            if (strcmp((*out)[j].event, event) == 0) {
                slot = j;
                break;
            }
        }
        if (slot == *count) {
            if (*count == cap) {
                size_t grown_cap = cap == 0 ? 8 : cap * 2;
                bt_stats_due *grown = realloc(*out, grown_cap * sizeof *grown);
                if (grown == NULL) {
                    free(*out);
                    *out = NULL;
                    *count = 0;
                    BT_TRACE_FAIL("app.stats_due_events", "out of memory");
                    BT_TRACE_EXIT("app.stats_due_events");
                    return -1;
                }
                *out = grown;
                cap = grown_cap;
            }
            snprintf((*out)[*count].event, BT_STATS_EVENT_MAX, "%s", event);
            (*out)[*count].due = 0;
            (*count)++;
        }
        (*out)[slot].due++;
    }
    BT_TRACE_EXIT("app.stats_due_events");
    return 0;
}

void bt_stats_snapshot_make(const bt_stores *stores, const bt_case_cards *cards, long long now_day,
                            bt_stats_snapshot *out) {
    BT_TRACE_ENTER("app.stats_snapshot");
    memset(out, 0, sizeof *out);
    out->solves = stores->solves.count;
    size_t solves = stores->solves.count;
    long long *memo = malloc((solves + 1) * sizeof *memo);
    long long *exec = malloc((solves + 1) * sizeof *exec);
    long long *memo_copy = malloc((solves + 1) * sizeof *memo_copy);
    long long *exec_copy = malloc((solves + 1) * sizeof *exec_copy);
    if (memo == NULL || exec == NULL || memo_copy == NULL || exec_copy == NULL) {
        free(memo);
        free(exec);
        free(memo_copy);
        free(exec_copy);
        BT_TRACE_FAIL("app.stats_snapshot", "out of memory");
        BT_TRACE_EXIT("app.stats_snapshot");
        return;
    }
    for (size_t i = 0; i < solves; i++) {
        memo[i] = stores->solves.items[i].memo_ms;
        exec[i] = stores->solves.items[i].exec_ms;
    }
    memcpy(memo_copy, memo, solves * sizeof *memo_copy);
    memcpy(exec_copy, exec, solves * sizeof *exec_copy);
    out->memo_mean_ms = bt_stats_mean(memo, solves);
    out->exec_mean_ms = bt_stats_mean(exec, solves);
    out->memo_median_ms = bt_stats_median(memo_copy, solves);
    out->exec_median_ms = bt_stats_median(exec_copy, solves);
    free(memo);
    free(exec);
    free(memo_copy);
    free(exec_copy);
    for (size_t i = 0; i < stores->srs.count; i++) {
        if (!bt_card_is_due(&stores->srs.items[i].card, now_day)) {
            continue;
        }
        if (strncmp(stores->srs.items[i].key, "edge/", 5) == 0) {
            out->due_edge++;
        } else if (strncmp(stores->srs.items[i].key, "corner/", 7) == 0) {
            out->due_corner++;
        }
    }
    out->streak = bt_stats_streak_of(stores);
    bt_stats_due_events(cards, now_day, &out->due_events, &out->due_event_count);
    BT_TRACE_EXIT("app.stats_snapshot");
}

void bt_stats_snapshot_free(bt_stats_snapshot *snap) {
    BT_TRACE_ENTER("app.stats_snapshot_free");
    free(snap->due_events);
    snap->due_events = NULL;
    snap->due_event_count = 0;
    BT_TRACE_EXIT("app.stats_snapshot_free");
}

static void block_set(bt_stats_block *block, const char *label, const char *value, float size_rem) {
    snprintf(block->label, BT_STATS_LABEL_MAX, "%s", label);
    snprintf(block->value, BT_STATS_VALUE_MAX, "%s", value);
    block->size_rem = size_rem;
}

bt_stats_block *bt_view_stats_blocks(const bt_stats_snapshot *snap, size_t *count) {
    BT_TRACE_ENTER("app.view_stats_blocks");
    const bt_theme *theme = bt_theme_table();
    size_t total = BT_STATS_LAYOUT_BLOCKS + snap->due_event_count;
    bt_stats_block *blocks = total == 0 ? NULL : malloc(total * sizeof *blocks);
    if (blocks == NULL) {
        *count = 0;
        BT_TRACE_FAIL("app.view_stats_blocks", "out of memory");
        BT_TRACE_EXIT("app.view_stats_blocks");
        return NULL;
    }
    char number[BT_STATS_VALUE_MAX];
    char seconds[BT_STATS_VALUE_MAX];
    snprintf(number, sizeof number, "%llu", (unsigned long long)snap->solves);
    block_set(&blocks[0], "solves", number, theme->font_title);
    snprintf(number, sizeof number, "%llu", (unsigned long long)snap->streak);
    block_set(&blocks[1], "verified drill streak", number, theme->font_title);
    snprintf(seconds, sizeof seconds, "%.1fs", snap->memo_mean_ms / 1000.0);
    block_set(&blocks[2], "memo mean", seconds, theme->font_title);
    snprintf(seconds, sizeof seconds, "%.1fs", snap->memo_median_ms / 1000.0);
    block_set(&blocks[3], "memo median", seconds, theme->font_heading);
    snprintf(seconds, sizeof seconds, "%.1fs", snap->exec_mean_ms / 1000.0);
    block_set(&blocks[4], "exec mean", seconds, theme->font_title);
    snprintf(seconds, sizeof seconds, "%.1fs", snap->exec_median_ms / 1000.0);
    block_set(&blocks[5], "exec median", seconds, theme->font_heading);
    snprintf(number, sizeof number, "%llu", (unsigned long long)snap->due_edge);
    block_set(&blocks[6], "due edge", number, theme->font_title);
    snprintf(number, sizeof number, "%llu", (unsigned long long)snap->due_corner);
    block_set(&blocks[7], "due corner", number, theme->font_title);
    for (size_t i = 0; i < snap->due_event_count; i++) {
        char label[BT_STATS_LABEL_MAX];
        snprintf(label, sizeof label, "due %s", snap->due_events[i].event);
        snprintf(number, sizeof number, "%llu", (unsigned long long)snap->due_events[i].due);
        block_set(&blocks[BT_STATS_LAYOUT_BLOCKS + i], label, number, theme->font_title);
    }
    *count = total;
    BT_TRACE_EXIT("app.view_stats_blocks");
    return blocks;
}

void bt_view_stats_blocks_free(bt_stats_block *blocks) {
    BT_TRACE_ENTER("app.view_stats_blocks_free");
    free(blocks);
    BT_TRACE_EXIT("app.view_stats_blocks_free");
}

static bt_stats_app_state g_stats_app;

void bt_view_stats_app_init(bt_stats_app_state *app) {
    BT_TRACE_ENTER("app.view_stats_app_init");
    memset(app, 0, sizeof *app);
    app->initialized = 1;
    BT_TRACE_EXIT("app.view_stats_app_init");
}

void bt_view_stats_app_free(bt_stats_app_state *app) {
    BT_TRACE_ENTER("app.view_stats_app_free");
    bt_stats_snapshot_free(&app->snap);
    app->initialized = 0;
    BT_TRACE_EXIT("app.view_stats_app_free");
}

bt_stats_app_state *bt_view_stats_app(void) {
    if (!g_stats_app.initialized) {
        bt_view_stats_app_init(&g_stats_app);
    }
    return &g_stats_app;
}

void bt_view_stats_enter_state(bt_app_ctx *ctx, bt_stats_app_state *app) {
    BT_TRACE_ENTER("app.view_stats_enter_state");
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx->paths.user_root, db_path);
    bt_db db;
    bt_db_error db_err;
    if (bt_db_open_at(db_path, &db, &db_err) == 0) {
        bt_srs_load_error load_err;
        if (bt_repo_cards_load(&db, &ctx->fsrs, &cards, &load_err) != 0) {
            bt_case_cards_free(&cards);
            bt_case_cards_init(&cards);
        }
        bt_db_close(&db);
    }
    bt_stats_snapshot_free(&app->snap);
    bt_stats_snapshot_make(&ctx->stores, &cards, bt_views_now_day(), &app->snap);
    bt_case_cards_free(&cards);
    BT_TRACE_EXIT("app.view_stats_enter_state");
}

void bt_view_stats_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_stats_enter");
    bt_view_stats_enter_state(ctx, bt_view_stats_app());
    BT_TRACE_EXIT("app.view_stats_enter");
}

static const float BT_STATS_TEXT_SPACING = 1.0F;

static float stats_center_offset(float total, float used) {
    return (total - used) / 2.0F;
}

void bt_view_stats_draw(const bt_stats_snapshot *snap, const void *font_regular,
                        const void *font_bold, float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_stats_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "stats", "esc menu");
    size_t count = 0;
    bt_stats_block *blocks = bt_view_stats_blocks(snap, &count);
    if (blocks == NULL) {
        BT_TRACE_FAIL("app.view_stats_draw", "no blocks");
        BT_TRACE_EXIT("app.view_stats_draw");
        return;
    }
    size_t shown = count < BT_STATS_LAYOUT_BLOCKS ? count : BT_STATS_LAYOUT_BLOCKS;
    Font regular = *(const Font *)font_regular;
    float row_gap = theme->spacing_rem * 2.0F * theme->root_rem;
    float col_gap = theme->spacing_rem * 6.0F * theme->root_rem;
    float inner_gap = theme->spacing_rem * 0.5F * theme->root_rem;
    float label_h = theme->font_micro * theme->root_rem;
    float row_h = 0.0F;
    float block_w[2] = {0.0F, 0.0F};
    for (size_t i = 0; i < shown; i++) {
        float size = blocks[i].size_rem * theme->root_rem;
        Vector2 value_box = MeasureTextEx(regular, blocks[i].value, size, BT_STATS_TEXT_SPACING);
        Vector2 label_box = MeasureTextEx(regular, blocks[i].label, label_h, BT_STATS_TEXT_SPACING);
        float width = value_box.x > label_box.x ? value_box.x : label_box.x;
        size_t col = i % 2;
        if (width > block_w[col]) {
            block_w[col] = width;
        }
        float height = size + inner_gap + label_h;
        if (height > row_h) {
            row_h = height;
        }
    }
    size_t row_count = (shown + 1) / 2;
    float total_h = row_h * (float)row_count + row_gap * (float)(row_count - 1);
    float y = metrics.content_y + stats_center_offset(metrics.content_h, total_h);
    for (size_t row = 0; row < row_count; row++) {
        float row_w = block_w[0] + block_w[1] + col_gap;
        float x = metrics.content_x + stats_center_offset(metrics.content_w, row_w);
        for (size_t col = 0; col < 2; col++) {
            size_t index = row * 2 + col;
            if (index >= shown) {
                break;
            }
            float size = blocks[index].size_rem * theme->root_rem;
            Vector2 value_box =
                MeasureTextEx(regular, blocks[index].value, size, BT_STATS_TEXT_SPACING);
            Vector2 label_box =
                MeasureTextEx(regular, blocks[index].label, label_h, BT_STATS_TEXT_SPACING);
            bt_app_draw_text(font_regular, blocks[index].value,
                             x + stats_center_offset(block_w[col], value_box.x), y, size,
                             bt_theme_text_primary());
            bt_app_draw_text(font_regular, blocks[index].label,
                             x + stats_center_offset(block_w[col], label_box.x),
                             y + size + inner_gap, label_h, bt_theme_text_muted());
            x += block_w[col] + col_gap;
        }
        y += row_h + row_gap;
    }
    bt_view_stats_blocks_free(blocks);
    BT_TRACE_EXIT("app.view_stats_draw");
}

void bt_view_stats_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_stats_row_draw");
    (void)ctx;
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_stats_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_stats_row_draw");
        return;
    }
    bt_view_stats_draw(&bt_view_stats_app()->snap, book->regular, book->bold,
                       (float)GetScreenWidth(), (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_stats_row_draw");
}
