#include "app/views/solve.h"

#include "app/views/drill.h"
#include "app/views/drill_view.h"
#include "app/views/frame.h"
#include "app/views/memo.h"
#include "app/views/net_view.h"
#include "app/window.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_solve_outcome_init(bt_solve_outcome *outcome) {
    BT_TRACE_ENTER("app.solve_outcome_init");
    memset(outcome, 0, sizeof *outcome);
    BT_TRACE_EXIT("app.solve_outcome_init");
}

void bt_solve_outcome_free(bt_solve_outcome *outcome) {
    BT_TRACE_ENTER("app.solve_outcome_free");
    for (size_t i = 0; i < outcome->broken_count; i++) {
        free(outcome->broken[i]);
    }
    outcome->broken_count = 0;
    BT_TRACE_EXIT("app.solve_outcome_free");
}

int bt_solve_outcome_push_broken(bt_solve_outcome *outcome, const char *text) {
    if (outcome->broken_count >= BT_SOLVE_BROKEN_MAX) {
        BT_TRACE_FAIL("app.solve_outcome_push_broken", "broken list full at %s", text);
        return -1;
    }
    char *copy = _strdup(text);
    if (copy == NULL) {
        BT_TRACE_FAIL("app.solve_outcome_push_broken", "out of memory");
        return -1;
    }
    outcome->broken[outcome->broken_count++] = copy;
    return 0;
}

void bt_view_solve_session_init(bt_solve_session *session) {
    BT_TRACE_ENTER("app.solve_session_init");
    memset(session, 0, sizeof *session);
    session->scramble = bt_cube_solved();
    BT_TRACE_EXIT("app.solve_session_init");
}

void bt_view_solve_session_free(bt_solve_session *session) {
    BT_TRACE_ENTER("app.solve_session_free");
    if (session->has_finished) {
        bt_solve_outcome_free(&session->finished.outcome);
    }
    session->has_finished = 0;
    BT_TRACE_EXIT("app.solve_session_free");
}

void bt_view_solve_pair_position(uint8_t buffer, const bt_case *case_id,
                                 char out[BT_SOLVE_TEXT_MAX]) {
    BT_TRACE_ENTER("app.solve_pair_position");
    snprintf(out, BT_SOLVE_TEXT_MAX, "%s-%s-%s", bt_scheme_name_of(buffer),
             bt_scheme_name_of(case_id->t1), bt_scheme_name_of(case_id->t2));
    BT_TRACE_EXIT("app.solve_pair_position");
}

static const bt_corpus_row *pick_row(const bt_corpus *corpus, bt_sheet_class sheet,
                                     const char *position) {
    const bt_corpus_row *first = NULL;
    for (size_t i = 0; i < corpus->count; i++) {
        const bt_corpus_row *row = &corpus->rows[i];
        if (row->sheet != sheet || strcmp(row->position, position) != 0) {
            continue;
        }
        if (row->solver == BT_SOLVER_USER) {
            return row;
        }
        if (first == NULL) {
            first = row;
        }
    }
    return first;
}

static void apply_special(const bt_corpus *corpus, bt_sheet_class sheet, const char *position,
                          bt_solve_outcome *broken, bt_alg *moves) {
    const bt_corpus_row *row = pick_row(corpus, sheet, position);
    if (row == NULL) {
        char text[BT_SOLVE_TEXT_MAX];
        snprintf(text, sizeof text, "%s uncovered", position);
        bt_solve_outcome_push_broken(broken, text);
        return;
    }
    for (size_t i = 0; i < bt_alg_len(&row->alg); i++) {
        bt_alg_push(moves, row->alg.items[i]);
    }
}

void bt_view_solve_execute_residue(const bt_corpus *corpus, const bt_residue *residue,
                                   bt_solve_outcome *broken, bt_alg *moves) {
    BT_TRACE_ENTER("app.solve_execute_residue");
    bt_residue_class class = bt_scheme_residue_class(residue);
    char position[BT_SOLVE_TEXT_MAX];
    if (class.kind == BT_RESIDUE_NONE) {
        BT_TRACE_EXIT("app.solve_execute_residue");
        return;
    }
    if (class.kind == BT_RESIDUE_FLIP_PAIR) {
        snprintf(position, sizeof position, "%s & %s", bt_scheme_name_of(class.a),
                 bt_scheme_name_of(class.b));
        apply_special(corpus, BT_SHEET_FLIPS, position, broken, moves);
        BT_TRACE_EXIT("app.solve_execute_residue");
        return;
    }
    if (class.kind == BT_RESIDUE_TWIST_PAIR) {
        char canonical[BT_SOLVE_TEXT_MAX];
        char swapped[BT_SOLVE_TEXT_MAX];
        snprintf(canonical, sizeof canonical, "%s & %s", bt_scheme_name_of(class.a),
                 bt_scheme_name_of(class.b));
        snprintf(swapped, sizeof swapped, "%s & %s", bt_scheme_name_of(class.b),
                 bt_scheme_name_of(class.a));
        if (pick_row(corpus, BT_SHEET_TWISTS, canonical) == NULL &&
            pick_row(corpus, BT_SHEET_TWISTS, swapped) != NULL) {
            apply_special(corpus, BT_SHEET_TWISTS, swapped, broken, moves);
        } else {
            apply_special(corpus, BT_SHEET_TWISTS, canonical, broken, moves);
        }
        BT_TRACE_EXIT("app.solve_execute_residue");
        return;
    }
    if (class.kind == BT_RESIDUE_PARITY || class.kind == BT_RESIDUE_LTCT) {
        const bt_parity *parity = &residue->parity;
        if (class.kind == BT_RESIDUE_PARITY) {
            snprintf(position, sizeof position, "%s-%s, %s-%s", bt_scheme_name_of(parity->edge[0]),
                     bt_scheme_name_of(parity->edge[1]), bt_scheme_name_of(parity->corner[0]),
                     bt_scheme_name_of(parity->corner[1]));
            apply_special(corpus, BT_SHEET_PARITY, position, broken, moves);
            BT_TRACE_EXIT("app.solve_execute_residue");
            return;
        }
        int has_bracket = 0;
        uint8_t bracket = 0;
        for (size_t i = 0; i < residue->twist_count; i++) {
            uint8_t corner = residue->twists[i].corner;
            if (!bt_scheme_same_piece(corner, parity->corner[0]) &&
                !bt_scheme_same_piece(corner, parity->corner[1])) {
                has_bracket = 1;
                bracket = corner;
                break;
            }
        }
        if (!has_bracket) {
            bt_solve_outcome_push_broken(broken, "ltct without a bracketed corner");
            BT_TRACE_EXIT("app.solve_execute_residue");
            return;
        }
        snprintf(position, sizeof position, "%s-%s, %s-%s[%s]", bt_scheme_name_of(parity->edge[0]),
                 bt_scheme_name_of(parity->edge[1]), bt_scheme_name_of(parity->corner[0]),
                 bt_scheme_name_of(parity->corner[1]), bt_scheme_name_of(bracket));
        apply_special(corpus, BT_SHEET_LTCT, position, broken, moves);
        BT_TRACE_EXIT("app.solve_execute_residue");
        return;
    }
    if (class.kind == BT_RESIDUE_TWIST_TRIPLE) {
        char text[BT_SOLVE_TEXT_MAX];
        snprintf(text, sizeof text, "residue unsupported: TwistTriple(%u, %u, %u)", class.a,
                 class.b, class.c);
        bt_solve_outcome_push_broken(broken, text);
        BT_TRACE_EXIT("app.solve_execute_residue");
        return;
    }
    bt_solve_outcome_push_broken(broken, "residue unsupported: Compound");
    BT_TRACE_EXIT("app.solve_execute_residue");
}

void bt_view_solve_executed_moves(const bt_corpus *corpus, const bt_memo *memo,
                                  bt_solve_outcome *broken, bt_alg *moves) {
    BT_TRACE_ENTER("app.solve_executed_moves");
    for (size_t orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? memo->edge_pairs : memo->corner_pairs;
        const uint8_t *buffers = orbit == 0 ? memo->edge_buffers : memo->corner_buffers;
        size_t count = orbit == 0 ? memo->edge_pair_count : memo->corner_pair_count;
        for (size_t i = 0; i < count; i++) {
            char position[BT_SOLVE_TEXT_MAX];
            bt_view_solve_pair_position(buffers[i], &pairs[i], position);
            const bt_corpus_row *row = pick_row(corpus, bt_view_sheet_of(pairs[i].orbit), position);
            if (row == NULL) {
                char text[BT_SOLVE_TEXT_MAX];
                snprintf(text, sizeof text, "%s uncovered", position);
                bt_solve_outcome_push_broken(broken, text);
                continue;
            }
            for (size_t at = 0; at < bt_alg_len(&row->alg); at++) {
                bt_alg_push(moves, row->alg.items[at]);
            }
        }
    }
    bt_view_solve_execute_residue(corpus, &memo->residue, broken, moves);
    BT_TRACE_EXIT("app.solve_executed_moves");
}

void bt_view_solve_simulate(const bt_corpus *corpus, const bt_cube *scramble, const bt_memo *memo,
                            bt_solve_outcome *out) {
    BT_TRACE_ENTER("app.solve_simulate");
    bt_solve_outcome broken;
    bt_solve_outcome_init(&broken);
    bt_alg moves;
    bt_alg_init(&moves);
    bt_view_solve_executed_moves(corpus, memo, &broken, &moves);
    bt_cube cube = *scramble;
    for (size_t i = 0; i < bt_alg_len(&moves); i++) {
        bt_cube_apply(&cube, moves.items[i]);
    }
    bt_alg_free(&moves);
    out->success = broken.broken_count == 0 && bt_cube_is_solved(&cube);
    for (size_t i = 0; i < broken.broken_count; i++) {
        bt_solve_outcome_push_broken(out, broken.broken[i]);
    }
    bt_solve_outcome_free(&broken);
    BT_TRACE_EXIT("app.solve_simulate");
}

void bt_view_solve_rebuild(bt_solve_session *session, uint64_t seed, const bt_syllable_book *book,
                           const bt_cube *override_scramble) {
    BT_TRACE_ENTER("app.solve_rebuild");
    if (session->has_finished) {
        bt_solve_outcome_free(&session->finished.outcome);
    }
    if (override_scramble != NULL) {
        session->scramble = *override_scramble;
    } else {
        bt_scheme_random_state(seed, &session->scramble);
    }
    bt_scheme_memo(&session->scramble, &session->memo);
    session->has_memo = 1;
    bt_memo_build_tokens(&session->memo, book, session->tokens, &session->token_count);
    session->seed = seed;
    session->shown = 0;
    session->clock = 0.0F;
    session->memo_secs = 0.0;
    session->exec_secs = 0.0;
    session->started_ms = 0;
    session->has_finished = 0;
    BT_TRACE_EXIT("app.solve_rebuild");
}

const char *bt_view_solve_phase_word(bt_solve_phase phase) {
    BT_TRACE_ENTER("app.solve_phase_word");
    const char *word = "";
    if (phase == BT_SOLVE_READY) {
        word = "ready";
    } else if (phase == BT_SOLVE_MEMO) {
        word = "memo";
    } else if (phase == BT_SOLVE_EXEC) {
        word = "exec";
    } else {
        word = "review";
    }
    BT_TRACE_EXIT("app.solve_phase_word");
    return word;
}

void bt_view_solve_phase_label(bt_solve_phase phase, const bt_solve_session *session,
                               char out[BT_SOLVE_TEXT_MAX]) {
    BT_TRACE_ENTER("app.solve_phase_label");
    snprintf(out, BT_SOLVE_TEXT_MAX, "%s, seed %llu", bt_view_solve_phase_word(phase),
             (unsigned long long)session->seed);
    BT_TRACE_EXIT("app.solve_phase_label");
}

const char *bt_view_solve_hint_text(bt_solve_phase phase) {
    BT_TRACE_ENTER("app.solve_hint_text");
    const char *text = "";
    if (phase == BT_SOLVE_READY) {
        text = "space to start memo";
    } else if (phase == BT_SOLVE_MEMO) {
        text = "space to finish memo";
    } else if (phase == BT_SOLVE_EXEC) {
        text = "space to stop";
    } else {
        text = "space next solve, v 3d replay";
    }
    BT_TRACE_EXIT("app.solve_hint_text");
    return text;
}

void bt_view_solve_focal_text(bt_solve_phase phase, const bt_solve_session *session,
                              char out[BT_SOLVE_TEXT_MAX]) {
    BT_TRACE_ENTER("app.solve_focal_text");
    out[0] = '\0';
    if (phase == BT_SOLVE_MEMO) {
        snprintf(out, BT_SOLVE_TEXT_MAX, "%zu/%zu", session->shown, session->token_count);
    }
    BT_TRACE_EXIT("app.solve_focal_text");
}

void bt_view_solve_timer_text(bt_solve_phase phase, const bt_solve_session *session,
                              char out[BT_SOLVE_TEXT_MAX]) {
    BT_TRACE_ENTER("app.solve_timer_text");
    out[0] = '\0';
    if (phase == BT_SOLVE_READY) {
        snprintf(out, BT_SOLVE_TEXT_MAX, "%zu pairs", session->token_count);
    } else if (phase == BT_SOLVE_MEMO) {
        snprintf(out, BT_SOLVE_TEXT_MAX, "memo %.1fs", session->memo_secs);
    } else if (phase == BT_SOLVE_EXEC) {
        snprintf(out, BT_SOLVE_TEXT_MAX, "exec %.1fs", session->exec_secs);
    }
    BT_TRACE_EXIT("app.solve_timer_text");
}

size_t bt_view_solve_review_lines(const bt_solve_session *session, char out[][BT_SOLVE_TEXT_MAX],
                                  size_t cap) {
    BT_TRACE_ENTER("app.solve_review_lines");
    size_t count = 0;
    if (!session->has_finished) {
        BT_TRACE_EXIT("app.solve_review_lines");
        return 0;
    }
    const bt_solve_finished *finished = &session->finished;
    if (count < cap) {
        snprintf(out[count++], BT_SOLVE_TEXT_MAX, "%s",
                 finished->outcome.success ? "solved" : "failed");
    }
    if (count < cap) {
        snprintf(out[count++], BT_SOLVE_TEXT_MAX, "memo %.1fs",
                 (double)finished->record.memo_ms / 1000.0);
    }
    if (count < cap) {
        snprintf(out[count++], BT_SOLVE_TEXT_MAX, "exec %.1fs",
                 (double)finished->record.exec_ms / 1000.0);
    }
    if (count < cap) {
        snprintf(out[count++], BT_SOLVE_TEXT_MAX, "total %.1fs",
                 (double)(finished->record.memo_ms + finished->record.exec_ms) / 1000.0);
    }
    for (size_t i = 0; i < finished->outcome.broken_count && count < cap; i++) {
        snprintf(out[count++], BT_SOLVE_TEXT_MAX, "%s", finished->outcome.broken[i]);
    }
    BT_TRACE_EXIT("app.solve_review_lines");
    return count;
}

void bt_view_solve_review_parts(const bt_solve_session *session, bt_solve_view_parts *out) {
    BT_TRACE_ENTER("app.solve_review_parts");
    size_t broken_count = 0;
    if (session->has_finished && session->finished.has_outcome) {
        broken_count = session->finished.outcome.broken_count;
    }
    size_t rows = 4 + broken_count;
    char (*lines)[BT_SOLVE_TEXT_MAX] = malloc(rows * sizeof *lines);
    if (lines == NULL) {
        BT_TRACE_FAIL("app.solve_review_parts", "out of memory");
        memset(out, 0, sizeof *out);
        BT_TRACE_EXIT("app.solve_review_parts");
        return;
    }
    size_t count = bt_view_solve_review_lines(session, lines, rows);
    memset(out, 0, sizeof *out);
    if (count > 0) {
        snprintf(out->result, sizeof out->result, "%s", lines[0]);
    }
    size_t split_end = count < 4 ? count : 4;
    for (size_t i = 1; i < split_end; i++) {
        if (out->splits[0] != '\0') {
            strncat(out->splits, "\n", sizeof out->splits - strlen(out->splits) - 1);
        }
        strncat(out->splits, lines[i], sizeof out->splits - strlen(out->splits) - 1);
    }
    for (size_t i = split_end; i < count; i++) {
        if (out->broken[0] != '\0') {
            strncat(out->broken, "\n", sizeof out->broken - strlen(out->broken) - 1);
        }
        strncat(out->broken, lines[i], sizeof out->broken - strlen(out->broken) - 1);
    }
    free(lines);
    BT_TRACE_EXIT("app.solve_review_parts");
}

static bt_view_solve_app_state *shared_solve_app;

void bt_view_solve_app_init(bt_view_solve_app_state *app) {
    BT_TRACE_ENTER("app.solve_app_init");
    memset(app, 0, sizeof *app);
    app->scramble_override = bt_cube_solved();
    bt_view_solve_session_init(&app->session);
    app->initialized = 1;
    BT_TRACE_EXIT("app.solve_app_init");
}

void bt_view_solve_app_free(bt_view_solve_app_state *app) {
    BT_TRACE_ENTER("app.solve_app_free");
    bt_view_solve_session_free(&app->session);
    app->initialized = 0;
    BT_TRACE_EXIT("app.solve_app_free");
}

bt_view_solve_app_state *bt_view_solve_app(void) {
    BT_TRACE_ENTER("app.solve_app");
    if (shared_solve_app == NULL) {
        shared_solve_app = malloc(sizeof *shared_solve_app);
        if (shared_solve_app == NULL) {
            BT_TRACE_FAIL("app.solve_app", "out of memory");
            BT_TRACE_EXIT("app.solve_app");
            return NULL;
        }
        bt_view_solve_app_init(shared_solve_app);
    }
    BT_TRACE_EXIT("app.solve_app");
    return shared_solve_app;
}

void bt_view_solve_set_seed(uint64_t seed) {
    BT_TRACE_ENTER("app.solve_set_seed");
    bt_view_solve_app_state *app = bt_view_solve_app();
    if (app != NULL) {
        app->seed = seed;
    }
    BT_TRACE_EXIT("app.solve_set_seed");
}

void bt_view_solve_set_scramble_override(bt_cube scramble) {
    BT_TRACE_ENTER("app.solve_set_scramble_override");
    bt_view_solve_app_state *app = bt_view_solve_app();
    if (app != NULL) {
        app->has_scramble_override = 1;
        app->scramble_override = scramble;
    }
    BT_TRACE_EXIT("app.solve_set_scramble_override");
}

void bt_view_solve_enter_state(bt_app_ctx *ctx, bt_view_solve_app_state *app) {
    BT_TRACE_ENTER("app.solve_enter_state");
    const bt_cube *override = app->has_scramble_override ? &app->scramble_override : NULL;
    bt_view_solve_rebuild(&app->session, app->seed, bt_views_book(ctx), override);
    app->has_scramble_override = 0;
    ctx->net.cube = app->session.scramble;
    ctx->net.has_buffer = 0;
    ctx->net.buffer_sticker = 0;
    ctx->net.has_target[0] = 0;
    ctx->net.has_target[1] = 0;
    ctx->net.targets[0] = 0;
    ctx->net.targets[1] = 0;
    ctx->solve_phase = BT_SOLVE_READY;
    BT_TRACE_EXIT("app.solve_enter_state");
}

void bt_view_solve_enter_phase(bt_app_ctx *ctx, bt_view_solve_app_state *app,
                               bt_solve_phase phase) {
    BT_TRACE_ENTER("app.solve_enter_phase");
    (void)ctx;
    (void)app;
    (void)phase;
    BT_TRACE_EXIT("app.solve_enter_phase");
}

void bt_view_solve_tick(bt_app_ctx *ctx, bt_view_solve_app_state *app, float dt) {
    BT_TRACE_ENTER("app.solve_tick");
    (void)ctx;
    if (ctx->solve_phase == BT_SOLVE_MEMO) {
        app->session.memo_secs += (double)dt;
        float cadence = (float)ctx->stores.settings.memo_cadence_ms / 1000.0F;
        if (cadence <= 0.0F) {
            app->session.shown = app->session.token_count;
            BT_TRACE_EXIT("app.solve_tick");
            return;
        }
        app->session.clock += dt;
        while (app->session.clock >= cadence && app->session.shown < app->session.token_count) {
            app->session.clock -= cadence;
            app->session.shown += 1;
        }
    } else if (ctx->solve_phase == BT_SOLVE_EXEC) {
        app->session.exec_secs += (double)dt;
    }
    BT_TRACE_EXIT("app.solve_tick");
}

static void finish_solve(bt_app_ctx *ctx, bt_view_solve_app_state *app) {
    bt_solve_record record;
    record.started_ms = app->session.started_ms;
    record.memo_ms = (long long)(app->session.memo_secs * 1000.0 + 0.5);
    record.exec_ms = (long long)(app->session.exec_secs * 1000.0 + 0.5);
    record.success = 0;
    bt_solve_finished finished;
    memset(&finished, 0, sizeof finished);
    finished.record = record;
    finished.has_outcome = 1;
    bt_solve_outcome_init(&finished.outcome);
    if (app->session.has_memo) {
        bt_view_solve_simulate(&ctx->vendor_corpus, &app->session.scramble, &app->session.memo,
                               &finished.outcome);
        finished.record.success = finished.outcome.success;
    } else {
        finished.outcome.success = 0;
        bt_solve_outcome_push_broken(&finished.outcome, "no memo");
    }
    bt_solve_log_push(&ctx->stores.solves, finished.record);
    bt_app_save_stores(ctx);
    bt_view_solve_session_free(&app->session);
    app->session.has_finished = 1;
    app->session.finished = finished;
    ctx->solve_phase = BT_SOLVE_REVIEW;
}

void bt_view_solve_key(bt_app_ctx *ctx, bt_view_solve_app_state *app, int key) {
    BT_TRACE_ENTER("app.solve_key");
    if (key == KEY_V && ctx->solve_phase == BT_SOLVE_REVIEW) {
        bt_app_request_state(ctx, BT_APP_REPLAY);
        BT_TRACE_EXIT("app.solve_key");
        return;
    }
    if (key != KEY_SPACE) {
        BT_TRACE_EXIT("app.solve_key");
        return;
    }
    if (ctx->solve_phase == BT_SOLVE_READY) {
        app->session.started_ms = bt_views_now_unix_ms();
        app->session.memo_secs = 0.0;
        app->session.shown = 0;
        app->session.clock = 0.0F;
        ctx->solve_phase = BT_SOLVE_MEMO;
        BT_TRACE_EXIT("app.solve_key");
        return;
    }
    if (ctx->solve_phase == BT_SOLVE_MEMO) {
        app->session.exec_secs = 0.0;
        ctx->solve_phase = BT_SOLVE_EXEC;
        BT_TRACE_EXIT("app.solve_key");
        return;
    }
    if (ctx->solve_phase == BT_SOLVE_EXEC) {
        finish_solve(ctx, app);
        BT_TRACE_EXIT("app.solve_key");
        return;
    }
    if (ctx->solve_phase == BT_SOLVE_REVIEW) {
        app->seed = app->seed + 1;
        bt_view_solve_rebuild(&app->session, app->seed, bt_views_book(ctx), NULL);
        ctx->net.cube = app->session.scramble;
        ctx->solve_phase = BT_SOLVE_READY;
    }
    BT_TRACE_EXIT("app.solve_key");
}

void bt_view_solve_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.solve_enter");
    bt_view_solve_enter_state(ctx, bt_view_solve_app());
    BT_TRACE_EXIT("app.solve_enter");
}

void bt_view_solve_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.solve_update");
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_tick(ctx, app, GetFrameTime());
    if (IsKeyPressed(KEY_SPACE)) {
        bt_view_solve_key(ctx, app, KEY_SPACE);
    }
    if (IsKeyPressed(KEY_V)) {
        bt_view_solve_key(ctx, app, KEY_V);
    }
    BT_TRACE_EXIT("app.solve_update");
}

void bt_view_solve_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                        float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_solve_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(font_bold, font_regular, window_w, window_h, "solve",
                       "space through ready, memo, exec, review, esc menu");
    bt_view_solve_app_state *app = bt_view_solve_app();
    char phase[BT_SOLVE_TEXT_MAX];
    char focal[BT_SOLVE_TEXT_MAX];
    char timer[BT_SOLVE_TEXT_MAX];
    bt_view_solve_phase_label(ctx->solve_phase, &app->session, phase);
    bt_view_solve_focal_text(ctx->solve_phase, &app->session, focal);
    bt_view_solve_timer_text(ctx->solve_phase, &app->session, timer);
    bt_solve_view_parts parts;
    bt_view_solve_review_parts(&app->session, &parts);
    float gap = theme->spacing_rem * theme->root_rem;
    bt_views_net *pulse = &bt_app_views_shared()->net;
    bt_view_drill_net_draw(font_bold, pulse, metrics.content_x, metrics.content_y,
                           theme->drill_net_width_pct * metrics.content_w, metrics.content_h);
    float x = metrics.content_x + theme->drill_net_width_pct * metrics.content_w + gap * 2.0F;
    float y = metrics.content_y;
    bt_app_draw_text(font_regular, phase, x, y, theme->font_small, bt_theme_text_muted());
    y += theme->font_focal * theme->root_rem + gap;
    bt_app_draw_text(font_bold, focal, x, y, theme->font_focal, bt_theme_text_primary());
    y += theme->font_focal * theme->root_rem + gap;
    bt_app_draw_text(font_bold, timer, x, y, theme->font_heading, bt_theme_text_primary());
    y += theme->font_title * theme->root_rem + gap;
    bt_app_draw_text(font_bold, parts.result, x, y, theme->font_title,
                     parts.result[0] != '\0' && strcmp(parts.result, "solved") == 0
                         ? bt_theme_ok()
                         : bt_theme_danger());
    y += theme->font_heading * theme->root_rem + gap;
    bt_app_draw_text(font_regular, parts.splits, x, y, theme->font_heading,
                     bt_theme_text_primary());
    y += theme->font_small * theme->root_rem + gap;
    bt_app_draw_text(font_regular, parts.broken, x, y, theme->font_small, bt_theme_danger());
    y += theme->font_small * theme->root_rem + gap;
    bt_app_draw_text(font_regular, bt_view_solve_hint_text(ctx->solve_phase), x, y,
                     theme->font_small, bt_theme_text_muted());
    BT_TRACE_EXIT("app.view_solve_draw");
}

void bt_view_solve_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_solve_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_solve_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_solve_row_draw");
        return;
    }
    bt_view_solve_draw(ctx, book->regular, book->bold, (float)GetScreenWidth(),
                       (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_solve_row_draw");
}
