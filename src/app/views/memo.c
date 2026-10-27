#include "app/views/memo.h"

#include "app/views/frame.h"
#include "app/window.h"

#include "raylib.h"

#include "app/views/drill.h"

#include "common/bt_trace.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_memo_session_init(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_session_init");
    memset(session, 0, sizeof *session);
    session->scramble = bt_cube_solved();
    BT_TRACE_EXIT("app.memo_session_init");
}

void bt_memo_session_free(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_session_free");
    for (size_t i = 0; i < session->token_count; i++) {
        free(session->tokens[i].syllable);
        session->tokens[i].syllable = NULL;
    }
    session->token_count = 0;
    BT_TRACE_EXIT("app.memo_session_free");
}

float bt_memo_cadence_secs(const bt_memo_session *session, const bt_stores *stores) {
    BT_TRACE_ENTER("app.memo_cadence_secs");
    (void)session;
    float secs = (float)stores->settings.memo_cadence_ms / 1000.0F;
    BT_TRACE_EXIT("app.memo_cadence_secs");
    return secs;
}

void bt_memo_reset_stream(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_reset_stream");
    session->shown = 0;
    session->clock = 0.0F;
    BT_TRACE_EXIT("app.memo_reset_stream");
}

void bt_memo_build_tokens(const bt_memo *memo, const bt_syllable_book *book, bt_memo_token *out,
                          size_t *count) {
    BT_TRACE_ENTER("app.memo_build_tokens");
    *count = 0;
    for (int orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? memo->edge_pairs : memo->corner_pairs;
        size_t pair_count = orbit == 0 ? memo->edge_pair_count : memo->corner_pair_count;
        for (size_t i = 0; i < pair_count; i++) {
            bt_memo_token *token = &out[(*count)++];
            token->orbit = pairs[i].orbit;
            bt_scheme_case_letters(&pairs[i], token->letters);
            token->syllable = NULL;
            const char *syllable = bt_syllable_book_for(
                book, token->orbit == BT_ORBIT_EDGE ? "edge" : "corner", token->letters);
            if (syllable != NULL) {
                token->syllable = _strdup(syllable);
                if (token->syllable == NULL) {
                    BT_TRACE_FAIL("app.memo_build_tokens", "out of memory");
                }
            }
            token->image = bt_views_image_for(token->letters);
        }
    }
    BT_TRACE_EXIT("app.memo_build_tokens");
}

void bt_memo_token_text(const bt_memo_token *token, bt_memo_mode mode, char out[BT_MEMO_TEXT_MAX]) {
    BT_TRACE_ENTER("app.memo_token_text");
    char pair[3] = {token->letters[0], token->letters[1], '\0'};
    const char *text = pair;
    int use_syllable = mode == BT_MEMO_SYLLABLES;
    int use_image = mode == BT_MEMO_IMAGES;
    if (mode == BT_MEMO_PER_ORBIT) {
        use_syllable = token->orbit == BT_ORBIT_EDGE;
        use_image = token->orbit == BT_ORBIT_CORNER;
    }
    if (use_syllable && token->syllable != NULL) {
        text = token->syllable;
    }
    if (use_image && token->image != NULL) {
        text = token->image;
    }
    snprintf(out, BT_MEMO_TEXT_MAX, "%s", text);
    BT_TRACE_EXIT("app.memo_token_text");
}

void bt_memo_rebuild(bt_memo_session *session, uint64_t seed, const bt_syllable_book *book) {
    BT_TRACE_ENTER("app.memo_rebuild");
    bt_memo_session_free(session);
    bt_cube scramble;
    bt_scheme_random_state(seed, &scramble);
    bt_memo memo;
    bt_scheme_memo(&scramble, &memo);
    bt_memo_build_tokens(&memo, book, session->tokens, &session->token_count);
    session->memo = memo;
    session->has_memo = 1;
    session->scramble = scramble;
    session->seed = seed;
    bt_memo_reset_stream(session);
    BT_TRACE_EXIT("app.memo_rebuild");
}

void bt_memo_rate_pairs(bt_stores *stores, const bt_fsrs_config *fsrs,
                        const bt_memo_session *session, bt_grade rating, long long now_day) {
    BT_TRACE_ENTER("app.memo_rate_pairs");
    if (!session->has_memo) {
        BT_TRACE_EXIT("app.memo_rate_pairs");
        return;
    }
    for (int orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? session->memo.edge_pairs : session->memo.corner_pairs;
        size_t pair_count =
            orbit == 0 ? session->memo.edge_pair_count : session->memo.corner_pair_count;
        for (size_t i = 0; i < pair_count; i++) {
            char position[BT_MEMO_TEXT_MAX];
            bt_view_case_position(&pairs[i], position);
            bt_view_write_review(stores, fsrs, bt_view_sheet_of(pairs[i].orbit), position, rating,
                                 now_day);
        }
    }
    BT_TRACE_EXIT("app.memo_rate_pairs");
}

int bt_memo_residue_lines(const bt_memo *memo, char out[][BT_MEMO_TEXT_MAX], size_t cap,
                          size_t *count) {
    BT_TRACE_ENTER("app.memo_residue_lines");
    *count = 0;
    if (memo->residue.has_parity) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "parity %s-%s, %s-%s",
                 bt_scheme_name_of(memo->residue.parity.edge[0]),
                 bt_scheme_name_of(memo->residue.parity.edge[1]),
                 bt_scheme_name_of(memo->residue.parity.corner[0]),
                 bt_scheme_name_of(memo->residue.parity.corner[1]));
    }
    for (size_t i = 0; i + 1 < memo->residue.flip_count; i += 2) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "flip %s & %s",
                 bt_scheme_name_of(memo->residue.flips[i]),
                 bt_scheme_name_of(memo->residue.flips[i + 1]));
    }
    for (size_t i = 0; i < memo->residue.twist_count; i++) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "twist %s x%u",
                 bt_scheme_name_of(memo->residue.twists[i].corner),
                 (unsigned)memo->residue.twists[i].quarter);
    }
    if (*count == 0) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "%s", "residue none");
    }
    BT_TRACE_EXIT("app.memo_residue_lines");
    return 1;
}

static void append_joined(char *out, size_t cap, const bt_memo_token *tokens, size_t count,
                          bt_memo_mode mode) {
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        char text[BT_MEMO_TEXT_MAX];
        bt_memo_token_text(&tokens[i], mode, text);
        if (i > 0) {
            strncat(out, " ", cap - strlen(out) - 1);
        }
        strncat(out, text, cap - strlen(out) - 1);
    }
}

static void trailing_before_current(char *out, size_t cap, const bt_memo_session *session,
                                    bt_memo_mode mode) {
    size_t end = session->shown > 0 ? session->shown - 1 : 0;
    size_t start = end > BT_MEMO_RECENT_TOKENS ? end - BT_MEMO_RECENT_TOKENS : 0;
    append_joined(out, cap, session->tokens + start, end - start, mode);
}

static void review_status(char *out, size_t cap, const bt_memo_session *session) {
    out[0] = '\0';
    if (session->has_memo) {
        char lines[BT_MEMO_RESIDUE_MAX][BT_MEMO_TEXT_MAX];
        size_t count = 0;
        bt_memo_residue_lines(&session->memo, lines, BT_MEMO_RESIDUE_MAX, &count);
        for (size_t i = 0; i < count; i++) {
            if (out[0] != '\0') {
                strncat(out, "; ", cap - strlen(out) - 1);
            }
            strncat(out, lines[i], cap - strlen(out) - 1);
        }
    }
    if (out[0] != '\0') {
        strncat(out, "; ", cap - strlen(out) - 1);
    }
    strncat(out, "r replay  space next  1..4 rate pairs", cap - strlen(out) - 1);
}

void bt_memo_view_rows(bt_memo_phase phase, const bt_memo_session *session, const bt_stores *stores,
                       bt_memo_rows *out) {
    BT_TRACE_ENTER("app.memo_view_rows");
    bt_memo_mode mode = stores->settings.memo_mode;
    unsigned cadence = stores->settings.memo_cadence_ms;
    if (phase == BT_MEMO_INTRO) {
        snprintf(out->focal, sizeof out->focal, "%s", "space to stream");
        out->recent[0] = '\0';
        snprintf(out->status, sizeof out->status, "seed %llu  %zu pairs  mode %s  cadence %u ms",
                 (unsigned long long)session->seed, session->token_count, bt_memo_mode_label(mode),
                 cadence);
    } else if (phase == BT_MEMO_STREAM) {
        if (session->shown > 0) {
            bt_memo_token_text(&session->tokens[session->shown - 1], mode, out->focal);
        } else {
            snprintf(out->focal, sizeof out->focal, "%s", "...");
        }
        trailing_before_current(out->recent, sizeof out->recent, session, mode);
        snprintf(out->status, sizeof out->status, "stream %zu/%zu  cadence %u ms", session->shown,
                 session->token_count, cadence);
    } else if (phase == BT_MEMO_RECALL) {
        snprintf(out->focal, sizeof out->focal, "%s", "recall");
        out->recent[0] = '\0';
        snprintf(out->status, sizeof out->status, "%zu/%zu streamed  space to reveal",
                 session->shown, session->token_count);
    } else {
        snprintf(out->focal, sizeof out->focal, "%s", "rate 1..4");
        append_joined(out->recent, sizeof out->recent, session->tokens, session->token_count, mode);
        review_status(out->status, sizeof out->status, session);
    }
    BT_TRACE_EXIT("app.memo_view_rows");
}

void bt_view_memo_draw(const bt_memo_rows *rows, const void *font_regular, const void *font_bold,
                       float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_memo_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "memo training", "");
    float gap = theme->spacing_rem * theme->root_rem;
    const char *lines[4] = {"esc menu", rows->focal, rows->recent, rows->status};
    float sizes[4] = {
        theme->font_small * theme->root_rem,
        theme->font_focal * theme->root_rem,
        theme->font_small * theme->root_rem,
        theme->font_body * theme->root_rem,
    };
    const void *fonts[4] = {font_regular, font_bold, font_regular, font_regular};
    bt_theme_color colors[4] = {
        bt_theme_text_muted(),
        bt_theme_text_primary(),
        bt_theme_text_muted(),
        bt_theme_text_primary(),
    };
    float total = 0.0F;
    for (int i = 0; i < 4; i++) {
        total += sizes[i];
    }
    total += gap * 3.0F;
    float y = metrics.content_y + (metrics.content_h - total) / 2.0F;
    for (int i = 0; i < 4; i++) {
        Font font = *(const Font *)fonts[i];
        Vector2 box = MeasureTextEx(font, lines[i], sizes[i], 1.0F);
        bt_app_draw_text(fonts[i], lines[i], metrics.content_x + (metrics.content_w - box.x) / 2.0F,
                         y, sizes[i], colors[i]);
        y += sizes[i] + gap;
    }
    BT_TRACE_EXIT("app.view_memo_draw");
}

static bt_memo_app_state memo_singleton;

void bt_view_memo_app_init(bt_memo_app_state *app) {
    BT_TRACE_ENTER("app.memo_app_init");
    memset(app, 0, sizeof *app);
    bt_memo_session_init(&app->session);
    app->seed = bt_views_time_seed();
    app->initialized = 1;
    BT_TRACE_EXIT("app.memo_app_init");
}

void bt_view_memo_app_free(bt_memo_app_state *app) {
    BT_TRACE_ENTER("app.memo_app_free");
    bt_memo_session_free(&app->session);
    app->initialized = 0;
    BT_TRACE_EXIT("app.memo_app_free");
}

bt_memo_app_state *bt_view_memo_app(void) {
    BT_TRACE_ENTER("app.memo_app");
    if (!memo_singleton.initialized) {
        bt_view_memo_app_init(&memo_singleton);
    }
    BT_TRACE_EXIT("app.memo_app");
    return &memo_singleton;
}

void bt_view_memo_enter_state(bt_app_ctx *ctx, bt_memo_app_state *app) {
    BT_TRACE_ENTER("app.memo_enter_state");
    bt_memo_rebuild(&app->session, app->seed, bt_views_book(ctx));
    ctx->memo_phase = BT_MEMO_INTRO;
    BT_TRACE_EXIT("app.memo_enter_state");
}

void bt_view_memo_tick(bt_app_ctx *ctx, bt_memo_app_state *app, float dt) {
    BT_TRACE_ENTER("app.memo_tick");
    if (ctx->memo_phase != BT_MEMO_STREAM) {
        BT_TRACE_EXIT("app.memo_tick");
        return;
    }
    float cadence = bt_memo_cadence_secs(&app->session, &ctx->stores);
    if (cadence <= 0.0F) {
        app->session.shown = app->session.token_count;
    } else {
        app->session.clock += dt;
        while (app->session.clock >= cadence && app->session.shown < app->session.token_count) {
            app->session.clock -= cadence;
            app->session.shown += 1;
        }
    }
    if (app->session.shown >= app->session.token_count && app->session.token_count > 0) {
        ctx->memo_phase = BT_MEMO_RECALL;
    }
    BT_TRACE_EXIT("app.memo_tick");
}

void bt_view_memo_key(bt_app_ctx *ctx, bt_memo_app_state *app, int key) {
    BT_TRACE_ENTER("app.memo_key");
    if (ctx->memo_phase == BT_MEMO_INTRO) {
        if (key == KEY_SPACE) {
            app->seed += 1;
            bt_memo_rebuild(&app->session, app->seed, bt_views_book(ctx));
            ctx->memo_phase = BT_MEMO_STREAM;
        }
        BT_TRACE_EXIT("app.memo_key");
        return;
    }
    if (ctx->memo_phase == BT_MEMO_RECALL) {
        if (key == KEY_SPACE) {
            ctx->memo_phase = BT_MEMO_REVIEW;
        }
        BT_TRACE_EXIT("app.memo_key");
        return;
    }
    if (ctx->memo_phase == BT_MEMO_REVIEW) {
        if (key == KEY_R) {
            ctx->memo_phase = BT_MEMO_STREAM;
            bt_memo_reset_stream(&app->session);
            BT_TRACE_EXIT("app.memo_key");
            return;
        }
        if (key == KEY_SPACE) {
            ctx->memo_phase = BT_MEMO_INTRO;
            BT_TRACE_EXIT("app.memo_key");
            return;
        }
        bt_grade rating;
        if (bt_view_rating_of(key, &rating)) {
            bt_memo_rate_pairs(&ctx->stores, &ctx->fsrs, &app->session, rating, bt_views_now_day());
            bt_app_save_stores(ctx);
            ctx->memo_phase = BT_MEMO_INTRO;
        }
    }
    BT_TRACE_EXIT("app.memo_key");
}

void bt_view_memo_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.memo_enter");
    bt_view_memo_enter_state(ctx, bt_view_memo_app());
    BT_TRACE_EXIT("app.memo_enter");
}

void bt_view_memo_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.memo_update");
    bt_memo_app_state *app = bt_view_memo_app();
    bt_view_memo_tick(ctx, app, GetFrameTime());
    int keys[6] = {KEY_SPACE, KEY_R, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};
    for (int i = 0; i < 6; i++) {
        if (IsKeyPressed(keys[i])) {
            bt_view_memo_key(ctx, app, keys[i]);
        }
    }
    BT_TRACE_EXIT("app.memo_update");
}

void bt_view_memo_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_memo_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_memo_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_memo_row_draw");
        return;
    }
    if (!ctx->has_stores) {
        BT_TRACE_FAIL("app.view_memo_row_draw", "no stores");
        BT_TRACE_EXIT("app.view_memo_row_draw");
        return;
    }
    bt_memo_rows rows;
    bt_memo_view_rows(ctx->memo_phase, &bt_view_memo_app()->session, &ctx->stores, &rows);
    bt_view_memo_draw(&rows, book->regular, book->bold, (float)GetScreenWidth(),
                      (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_memo_row_draw");
}
