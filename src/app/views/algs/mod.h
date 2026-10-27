#ifndef BT_APP_VIEWS_ALGS_MOD_H
#define BT_APP_VIEWS_ALGS_MOD_H

#include "app/config.h"
#include "app/logic.h"
#include "app/states.h"
#include "app/theme.h"
#include "app/views/state.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/commutator.h"
#include "data/corpus.h"
#include "data/verify.h"
#include "scheme/pairs.h"

#include <stddef.h>

#define BT_ALGS_QUERY_MAX 32
#define BT_ALGS_LABEL_MAX 16
#define BT_ALGS_ROWS_MAX 64
#define BT_ALGS_TEXT_MAX 256
#define BT_ALGS_SPANS_MAX 32
#define BT_ALGS_CASES_MAX 1024

typedef struct {
    size_t first;
    size_t count;
} bt_view_list_span;

typedef struct {
    char label[BT_ALGS_TEXT_MAX];
    int selected;
} bt_view_case_row;

typedef enum {
    BT_ALGS_STYLE_FOCAL,
    BT_ALGS_STYLE_MUTED,
    BT_ALGS_STYLE_HEADING,
    BT_ALGS_STYLE_ALG,
    BT_ALGS_STYLE_BODY,
    BT_ALGS_STYLE_SMALL,
    BT_ALGS_STYLE_MICRO,
} bt_algs_style;

typedef struct {
    bt_case all[BT_ALGS_CASES_MAX];
    size_t all_count;
    bt_case filtered[BT_ALGS_CASES_MAX];
    size_t filtered_count;
    size_t selected;
} bt_view_algs_browser;

typedef struct {
    int initialized;
    char query[BT_ALGS_QUERY_MAX];
    bt_view_algs_browser browser;
    int query_focused;
} bt_view_algs_app_state;

bt_view_list_span bt_view_list_window(size_t total, size_t selected, size_t capacity);

size_t bt_view_algs_all_cases(bt_case out[BT_ALGS_CASES_MAX]);
void bt_view_algs_letters_of(const bt_case *case_id, char out[3]);
const char *bt_view_algs_orbit_word(bt_orbit orbit);
size_t bt_view_algs_filter_cases(const bt_case *all, size_t count, const char *query,
                                 bt_case out[BT_ALGS_CASES_MAX]);
size_t bt_view_algs_case_rows(const bt_corpus *corpus, const bt_case *case_id,
                              const bt_corpus_row *out[BT_ALGS_ROWS_MAX]);
size_t bt_view_algs_case_rows_dyn(const bt_corpus *corpus, const bt_case *case_id,
                                  const bt_corpus_row ***out);
int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id);
size_t bt_view_algs_list_capacity(const bt_app_config *config);
size_t bt_view_algs_list_rows(const bt_view_algs_browser *browser, size_t capacity,
                              bt_view_case_row out[BT_ALGS_ROWS_MAX]);
void bt_view_algs_meta_line(const bt_corpus_row *row, char out[BT_ALGS_TEXT_MAX]);
size_t bt_view_algs_pair_expand_rows(const bt_corpus_row *row, char out[BT_ALGS_TEXT_MAX]);
typedef struct {
    char text[BT_ALGS_TEXT_MAX];
    bt_theme_color color;
} bt_view_algs_span;
size_t bt_view_algs_mechanic_spans(const bt_expanded_list *units,
                                   bt_view_algs_span out[BT_ALGS_SPANS_MAX]);

void bt_view_algs_app_init(bt_view_algs_app_state *app);
bt_view_algs_app_state *bt_view_algs_app(void);
void bt_view_algs_highlight(bt_app_net_state *net, const bt_case *case_id);
void bt_view_algs_refresh(const char *query, bt_view_algs_browser *browser);
void bt_view_algs_enter(bt_app_ctx *ctx);
void bt_view_algs_update(bt_app_ctx *ctx);
void bt_view_algs_read_query(bt_app_ctx *ctx, const char *text);
void bt_view_algs_key(bt_app_ctx *ctx, int key);
void bt_view_case_drill_open_request(bt_app_ctx *ctx);

void bt_view_override_open(bt_app_ctx *ctx);
void bt_view_override_submit(bt_app_ctx *ctx, const char *text);
void bt_view_override_cancel(void);
void bt_view_override_delete(bt_app_ctx *ctx);
int bt_view_algs_frame(bt_app_ctx *ctx, const int *keys, int count);

void bt_view_algs_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                       float window_w, float window_h);
void bt_view_algs_row_draw(bt_app_ctx *ctx);

typedef struct {
    bt_algs_style style;
    char text[BT_ALGS_TEXT_MAX];
    bt_theme_color color;
} bt_view_algs_detail_line;

size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line **out);
void bt_view_algs_style_metrics(bt_algs_style style, float *size, bt_theme_color *color);

#endif
