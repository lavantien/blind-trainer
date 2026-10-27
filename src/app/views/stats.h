#ifndef BT_APP_VIEWS_STATS_H
#define BT_APP_VIEWS_STATS_H

#include "app/logic.h"
#include "app/persist.h"
#include "app/states.h"
#include "data/repo_cards.h"

#include <stddef.h>

#define BT_STATS_EVENT_MAX BT_CASE_FIELD_MAX
#define BT_STATS_LABEL_MAX 64
#define BT_STATS_VALUE_MAX 32
#define BT_STATS_LAYOUT_BLOCKS 8

typedef struct {
    char event[BT_STATS_EVENT_MAX];
    size_t due;
} bt_stats_due;

typedef struct {
    size_t solves;
    double memo_mean_ms;
    double memo_median_ms;
    double exec_mean_ms;
    double exec_median_ms;
    size_t due_edge;
    size_t due_corner;
    size_t streak;
    bt_stats_due *due_events;
    size_t due_event_count;
} bt_stats_snapshot;

typedef struct {
    char label[BT_STATS_LABEL_MAX];
    char value[BT_STATS_VALUE_MAX];
    float size_rem;
} bt_stats_block;

double bt_stats_median(long long *values, size_t count);
double bt_stats_mean(const long long *values, size_t count);
size_t bt_stats_streak_of(const bt_stores *stores);
int bt_stats_due_events(const bt_case_cards *cards, long long now_day, bt_stats_due **out,
                        size_t *count);
void bt_stats_snapshot_make(const bt_stores *stores, const bt_case_cards *cards, long long now_day,
                            bt_stats_snapshot *out);
void bt_stats_snapshot_free(bt_stats_snapshot *snap);

bt_stats_block *bt_view_stats_blocks(const bt_stats_snapshot *snap, size_t *count);
void bt_view_stats_blocks_free(bt_stats_block *blocks);

typedef struct {
    int initialized;
    bt_stats_snapshot snap;
} bt_stats_app_state;

void bt_view_stats_app_init(bt_stats_app_state *app);
void bt_view_stats_app_free(bt_stats_app_state *app);
bt_stats_app_state *bt_view_stats_app(void);
void bt_view_stats_enter_state(bt_app_ctx *ctx, bt_stats_app_state *app);
void bt_view_stats_enter(bt_app_ctx *ctx);
void bt_view_stats_draw(const bt_stats_snapshot *snap, const void *font_regular,
                        const void *font_bold, float window_w, float window_h);
void bt_view_stats_row_draw(bt_app_ctx *ctx);

#endif
