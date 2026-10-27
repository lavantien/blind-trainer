#ifndef BT_APP_VIEWS_CASE_DRILL_H
#define BT_APP_VIEWS_CASE_DRILL_H

#include "app/logic.h"
#include "app/persist.h"
#include "app/states.h"
#include "app/views/algs/browse.h"
#include "app/views/algs/detail.h"
#include "app/views/algs/mod.h"
#include "data/repo_cards.h"
#include "data/repo_user.h"
#include "puzzles/geometry.h"

#include <stddef.h>

#define BT_CASE_DRILL_QUEUE_MAX 256
#define BT_CASE_DRILL_TEXT_MAX 256

typedef struct {
    bt_case_ref case_ref;
    int has_size;
    long long size;
    char seed_alg[BT_CASE_DRILL_TEXT_MAX];
    char alg[BT_CASE_DRILL_TEXT_MAX];
} bt_drill_case;

typedef struct {
    bt_drill_case *items;
    size_t count;
    size_t cap;
} bt_drill_case_queue;

void bt_drill_case_queue_init(bt_drill_case_queue *queue);
void bt_drill_case_queue_free(bt_drill_case_queue *queue);
int bt_drill_case_queue_push(bt_drill_case_queue *queue, const bt_drill_case *case_drill);
int bt_drill_case_queue_set(bt_drill_case_queue *queue, const bt_drill_case *cases, size_t count);

typedef struct {
    bt_case_cards cards;
    int has_load_error;
    char load_error[BT_CASE_DRILL_TEXT_MAX];
} bt_case_card_store;

typedef struct {
    bt_drill_case_queue queue;
    int has_current;
    bt_drill_case current;
    size_t done;
    size_t total;
    int has_grid;
    bt_pz_grid grid;
    int has_grid_error;
    char grid_error[BT_CASE_DRILL_TEXT_MAX];
    int has_save_error;
    char save_error[BT_CASE_DRILL_TEXT_MAX];
} bt_case_drill_session;

size_t bt_case_drill_face_color_index(const char *face);
size_t bt_case_drill_face_permutation(const char *event, size_t out[BT_PZ_MAX_FACES]);
size_t bt_case_drill_color_remap(const char *event, size_t out[16]);
int bt_case_drill_themed_grid(const bt_pz_grid *grid, const char *event, bt_pz_grid *out);
int bt_case_drill_facelets(const char *event, int has_size, long long size, const char *alg,
                           bt_pz_grid *out, char *error, size_t error_len);
size_t bt_case_drill_collect_cases(const bt_repo_browser *browser, const bt_overrides *store,
                                   bt_drill_case_queue *out);
size_t bt_case_drill_due_session(const bt_drill_case_queue *cases, const bt_case_cards *cards,
                                 long long now_day, bt_drill_case_queue *out);
int bt_case_drill_request(const bt_repo_browser *browser, const bt_overrides *store,
                          const bt_case_card_store *cards, long long now_day,
                          bt_drill_case_queue *out);
void bt_case_drill_install(bt_case_drill_session *session, const bt_drill_case_queue *queue);
void bt_case_drill_advance(bt_case_drill_session *session);
void bt_case_drill_case_site_line(const bt_drill_case *case_drill,
                                  char out[BT_CASE_DRILL_TEXT_MAX]);

typedef struct {
    int initialized;
    bt_drill_case_queue pending;
    bt_case_card_store cards;
    bt_case_drill_session session;
} bt_view_case_drill_app_state;

void bt_view_case_drill_app_init(bt_view_case_drill_app_state *app);
bt_view_case_drill_app_state *bt_view_case_drill_app(void);
void bt_view_case_drill_set_pending(const bt_drill_case_queue *queue);
void bt_view_case_drill_cards_enter(bt_app_ctx *ctx);
void bt_view_case_drill_enter_state(bt_app_ctx *ctx, bt_view_case_drill_app_state *app);
void bt_view_case_drill_key(bt_app_ctx *ctx, bt_view_case_drill_app_state *app, int key);
void bt_view_case_drill_frame(bt_app_ctx *ctx, bt_view_case_drill_app_state *app, const int *keys,
                              int count);
void bt_view_case_drill_enter(bt_app_ctx *ctx);
void bt_view_case_drill_update(bt_app_ctx *ctx);

#endif
