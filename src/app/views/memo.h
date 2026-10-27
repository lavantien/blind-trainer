#ifndef BT_APP_VIEWS_MEMO_H
#define BT_APP_VIEWS_MEMO_H

#include "app/logic.h"
#include "app/persist.h"
#include "app/states.h"
#include "app/views/state.h"
#include "core/cube.h"
#include "data/fsrs.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scramble.h"

#include <stddef.h>
#include <stdint.h>

#define BT_MEMO_TOKENS_MAX (BT_MEMO_MAX_PAIRS * 2)
#define BT_MEMO_TEXT_MAX 256
#define BT_MEMO_RECENT_MAX 4096
#define BT_MEMO_STATUS_MAX 2048
#define BT_MEMO_RESIDUE_MAX 16
#define BT_MEMO_RECENT_TOKENS 4

typedef struct {
    bt_orbit orbit;
    char letters[2];
    char *syllable;
    const char *image;
} bt_memo_token;

typedef struct {
    uint64_t seed;
    bt_cube scramble;
    int has_memo;
    bt_memo memo;
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t token_count;
    size_t shown;
    float clock;
} bt_memo_session;

typedef struct {
    char focal[BT_MEMO_TEXT_MAX];
    char recent[BT_MEMO_RECENT_MAX];
    char status[BT_MEMO_STATUS_MAX];
} bt_memo_rows;

void bt_memo_session_init(bt_memo_session *session);
void bt_memo_session_free(bt_memo_session *session);
float bt_memo_cadence_secs(const bt_memo_session *session, const bt_stores *stores);
void bt_memo_reset_stream(bt_memo_session *session);
void bt_memo_build_tokens(const bt_memo *memo, const bt_syllable_book *book, bt_memo_token *out,
                          size_t *count);
void bt_memo_token_text(const bt_memo_token *token, bt_memo_mode mode, char out[BT_MEMO_TEXT_MAX]);
void bt_memo_rebuild(bt_memo_session *session, uint64_t seed, const bt_syllable_book *book);
void bt_memo_rate_pairs(bt_stores *stores, const bt_fsrs_config *fsrs,
                        const bt_memo_session *session, bt_grade rating, long long now_day);
int bt_memo_residue_lines(const bt_memo *memo, char out[][BT_MEMO_TEXT_MAX], size_t cap,
                          size_t *count);
void bt_memo_view_rows(bt_memo_phase phase, const bt_memo_session *session, const bt_stores *stores,
                       bt_memo_rows *out);

void bt_view_memo_draw(const bt_memo_rows *rows, const void *font_regular, const void *font_bold,
                       float window_w, float window_h);

#endif
