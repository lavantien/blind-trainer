#ifndef BT_APP_VIEWS_DRILL_CARD_H
#define BT_APP_VIEWS_DRILL_CARD_H

#include "app/theme.h"
#include "app/views/drill.h"
#include "app/views/state.h"

#include <stddef.h>

#define BT_CARD_ROWS_MAX 48
#define BT_CARD_SPANS_MAX 96
#define BT_CARD_TEXT_MAX 256

typedef enum {
    BT_ROW_FOCAL = 0,
    BT_ROW_MUTED,
    BT_ROW_HEADING,
    BT_ROW_ALG,
    BT_ROW_BODY,
    BT_ROW_SMALL,
    BT_ROW_MICRO,
} bt_row_style;

typedef struct {
    char text[BT_CARD_TEXT_MAX];
    bt_row_style style;
} bt_card_row;

typedef struct {
    char text[BT_CARD_TEXT_MAX];
    float size_rem;
    bt_theme_color color;
} bt_card_span;

typedef struct {
    bt_card_row rows[BT_CARD_ROWS_MAX];
    size_t row_count;
    int has_mechanics;
    bt_card_span spans[BT_CARD_SPANS_MAX];
    size_t span_count;
} bt_card_content;

void bt_row_style_metrics(bt_row_style style, float *size_rem, bt_theme_color *color);
int bt_view_build_sides(const bt_corpus *corpus, const bt_case *case_id, bt_drill_side *side_a,
                        bt_drill_side *side_b);
void bt_view_memo_token(const bt_syllable_book *book, const bt_case *case_id,
                        char out[BT_CARD_TEXT_MAX]);
int bt_view_mechanic_spans(const char *commutator, const bt_alg *alg, bt_card_content *out);
void bt_view_card_content(const bt_drill_session *session, const bt_corpus *corpus,
                          const bt_syllable_book *book, const bt_stores *stores, long long now_day,
                          bt_card_content *out);
void bt_view_reps_lines(const bt_drill_session *session, char out[][BT_CARD_TEXT_MAX], size_t cap,
                        size_t *count);
void bt_view_rate_lines(const bt_drill_session *session, char out[][BT_CARD_TEXT_MAX], size_t cap,
                        size_t *count);

#endif
