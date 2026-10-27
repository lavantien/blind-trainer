#ifndef BT_APP_VIEWS_SOLVE_H
#define BT_APP_VIEWS_SOLVE_H

#include "app/logic.h"
#include "app/persist.h"
#include "app/states.h"
#include "app/views/memo.h"
#include "app/views/state.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/corpus.h"
#include "data/fsrs.h"
#include "scheme/memo.h"
#include "scheme/scramble.h"

#include <stddef.h>
#include <stdint.h>

#define BT_SOLVE_TEXT_MAX 256
#define BT_SOLVE_BROKEN_MAX 16
#define BT_SOLVE_LINES_MAX 8

typedef struct {
    int success;
    size_t broken_count;
    char *broken[BT_SOLVE_BROKEN_MAX];
} bt_solve_outcome;

typedef struct {
    bt_solve_record record;
    int has_outcome;
    bt_solve_outcome outcome;
} bt_solve_finished;

typedef struct {
    uint64_t seed;
    bt_cube scramble;
    int has_memo;
    bt_memo memo;
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t token_count;
    size_t shown;
    float clock;
    double memo_secs;
    double exec_secs;
    long long started_ms;
    int has_finished;
    bt_solve_finished finished;
} bt_solve_session;

typedef struct {
    char result[BT_SOLVE_TEXT_MAX];
    char splits[BT_SOLVE_TEXT_MAX];
    char broken[BT_SOLVE_TEXT_MAX * 2];
} bt_solve_view_parts;

void bt_solve_outcome_init(bt_solve_outcome *outcome);
void bt_solve_outcome_free(bt_solve_outcome *outcome);
int bt_solve_outcome_push_broken(bt_solve_outcome *outcome, const char *text);

void bt_view_solve_session_init(bt_solve_session *session);
void bt_view_solve_session_free(bt_solve_session *session);

void bt_view_solve_pair_position(uint8_t buffer, const bt_case *case_id,
                                 char out[BT_SOLVE_TEXT_MAX]);
void bt_view_solve_execute_residue(const bt_corpus *corpus, const bt_residue *residue,
                                   bt_solve_outcome *broken, bt_alg *moves);
void bt_view_solve_executed_moves(const bt_corpus *corpus, const bt_memo *memo,
                                  bt_solve_outcome *broken, bt_alg *moves);
void bt_view_solve_simulate(const bt_corpus *corpus, const bt_cube *scramble, const bt_memo *memo,
                            bt_solve_outcome *out);
void bt_view_solve_rebuild(bt_solve_session *session, uint64_t seed, const bt_syllable_book *book,
                           const bt_cube *override_scramble);

const char *bt_view_solve_phase_word(bt_solve_phase phase);
void bt_view_solve_phase_label(bt_solve_phase phase, const bt_solve_session *session,
                               char out[BT_SOLVE_TEXT_MAX]);
const char *bt_view_solve_hint_text(bt_solve_phase phase);
void bt_view_solve_focal_text(bt_solve_phase phase, const bt_solve_session *session,
                              char out[BT_SOLVE_TEXT_MAX]);
void bt_view_solve_timer_text(bt_solve_phase phase, const bt_solve_session *session,
                              char out[BT_SOLVE_TEXT_MAX]);
size_t bt_view_solve_review_lines(const bt_solve_session *session, char out[][BT_SOLVE_TEXT_MAX],
                                  size_t cap);
void bt_view_solve_review_parts(const bt_solve_session *session, bt_solve_view_parts *out);

typedef struct {
    int initialized;
    uint64_t seed;
    int has_scramble_override;
    bt_cube scramble_override;
    bt_solve_session session;
} bt_view_solve_app_state;

void bt_view_solve_app_init(bt_view_solve_app_state *app);
void bt_view_solve_app_free(bt_view_solve_app_state *app);
bt_view_solve_app_state *bt_view_solve_app(void);
void bt_view_solve_set_scramble_override(bt_cube scramble);
void bt_view_solve_set_seed(uint64_t seed);
void bt_view_solve_enter_state(bt_app_ctx *ctx, bt_view_solve_app_state *app);
void bt_view_solve_enter_phase(bt_app_ctx *ctx, bt_view_solve_app_state *app, bt_solve_phase phase);
void bt_view_solve_tick(bt_app_ctx *ctx, bt_view_solve_app_state *app, float dt);
void bt_view_solve_key(bt_app_ctx *ctx, bt_view_solve_app_state *app, int key);
void bt_view_solve_enter(bt_app_ctx *ctx);
void bt_view_solve_update(bt_app_ctx *ctx);

void bt_view_solve_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                        float window_w, float window_h);
void bt_view_solve_row_draw(bt_app_ctx *ctx);

#endif
