#include "app/views/drill.h"

#include "app/views/state.h"

#include "raylib.h"

#include <stdint.h>

#include "app/views/drill_card.h"

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "core/expand.h"
#include "data/commutator.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_drill_side_init(bt_drill_side *side) {
    BT_TRACE_ENTER("app.drill_side_init");
    memset(side, 0, sizeof *side);
    bt_alg_init(&side->alg);
    BT_TRACE_EXIT("app.drill_side_init");
}

void bt_drill_side_free(bt_drill_side *side) {
    BT_TRACE_ENTER("app.drill_side_free");
    bt_alg_free(&side->alg);
    BT_TRACE_EXIT("app.drill_side_free");
}

void bt_drill_side_letters(const bt_drill_side *side, char out[3]) {
    BT_TRACE_ENTER("app.drill_side_letters");
    char letters[2];
    bt_scheme_case_letters(&side->case_id, letters);
    out[0] = letters[0];
    out[1] = letters[1];
    out[2] = '\0';
    BT_TRACE_EXIT("app.drill_side_letters");
}

void bt_drill_session_init(bt_drill_session *session) {
    BT_TRACE_ENTER("app.drill_session_init");
    memset(session, 0, sizeof *session);
    bt_drill_side_init(&session->side_a);
    bt_drill_side_init(&session->side_b);
    session->reps_side_a = 1;
    session->prepared = bt_cube_solved();
    BT_TRACE_EXIT("app.drill_session_init");
}

void bt_drill_session_free(bt_drill_session *session) {
    BT_TRACE_ENTER("app.drill_session_free");
    bt_drill_side_free(&session->side_a);
    bt_drill_side_free(&session->side_b);
    free(session->splits);
    session->splits = NULL;
    session->split_count = 0;
    session->split_cap = 0;
    BT_TRACE_EXIT("app.drill_session_free");
}

int bt_drill_push_split(bt_drill_session *session, int is_a, float secs) {
    BT_TRACE_ENTER("app.drill_push_split");
    if (session->split_count == session->split_cap) {
        size_t cap = session->split_cap == 0 ? 8 : session->split_cap * 2;
        bt_drill_split *grown = realloc(session->splits, cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.drill_push_split", "out of memory at %zu", session->split_count);
            BT_TRACE_EXIT("app.drill_push_split");
            return -1;
        }
        session->splits = grown;
        session->split_cap = cap;
    }
    session->splits[session->split_count].is_a = is_a;
    session->splits[session->split_count].secs = secs;
    session->split_count += 1;
    BT_TRACE_EXIT("app.drill_push_split");
    return 0;
}

const bt_drill_side *bt_drill_session_sides(const bt_drill_session *session,
                                            const bt_drill_side **side_b) {
    BT_TRACE_ENTER("app.drill_session_sides");
    if (!session->has_side_a || !session->has_side_b) {
        if (side_b != NULL) {
            *side_b = NULL;
        }
        BT_TRACE_EXIT("app.drill_session_sides");
        return NULL;
    }
    if (side_b != NULL) {
        *side_b = &session->side_b;
    }
    BT_TRACE_EXIT("app.drill_session_sides");
    return &session->side_a;
}

const bt_drill_side *bt_drill_session_current_side(const bt_drill_session *session) {
    BT_TRACE_ENTER("app.drill_session_current_side");
    const bt_drill_side *side = NULL;
    if (session->reps_side_a && session->has_side_a) {
        side = &session->side_a;
    } else if (!session->reps_side_a && session->has_side_b) {
        side = &session->side_b;
    }
    BT_TRACE_EXIT("app.drill_session_current_side");
    return side;
}

bt_sheet_class bt_view_sheet_of(bt_orbit orbit) {
    BT_TRACE_ENTER("app.drill_sheet_of");
    bt_sheet_class sheet = orbit == BT_ORBIT_EDGE ? BT_SHEET_EDGE : BT_SHEET_CORNER;
    BT_TRACE_EXIT("app.drill_sheet_of");
    return sheet;
}

unsigned char bt_view_buffer_of(bt_orbit orbit) {
    BT_TRACE_ENTER("app.drill_buffer_of");
    unsigned char sticker =
        orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER : BT_SCHEME_CORNER_BUFFER_STICKER;
    BT_TRACE_EXIT("app.drill_buffer_of");
    return sticker;
}

void bt_view_case_position(const bt_case *case_id, char out[BT_DRILL_TEXT_MAX]) {
    BT_TRACE_ENTER("app.drill_case_position");
    snprintf(out, BT_DRILL_TEXT_MAX, "%s-%s-%s",
             bt_scheme_name_of(bt_view_buffer_of(case_id->orbit)), bt_scheme_name_of(case_id->t1),
             bt_scheme_name_of(case_id->t2));
    BT_TRACE_EXIT("app.drill_case_position");
}

bt_case bt_view_flip_case(const bt_case *case_id) {
    BT_TRACE_ENTER("app.drill_flip_case");
    bt_case flipped;
    flipped.orbit = case_id->orbit;
    flipped.t1 = case_id->t2;
    flipped.t2 = case_id->t1;
    BT_TRACE_EXIT("app.drill_flip_case");
    return flipped;
}

void bt_view_card_key(bt_sheet_class sheet, const char *position, char out[BT_DRILL_TEXT_MAX]) {
    BT_TRACE_ENTER("app.drill_card_key");
    snprintf(out, BT_DRILL_TEXT_MAX, "%s/%s", bt_sheet_class_file(sheet), position);
    BT_TRACE_EXIT("app.drill_card_key");
}

unsigned int bt_view_even_reps(unsigned int reps) {
    BT_TRACE_ENTER("app.drill_even_reps");
    unsigned int sum = reps + (reps % 2);
    if (sum < reps) {
        sum = 0xFFFFFFFFU;
    }
    if (sum < 2) {
        sum = 2;
    }
    unsigned int even = sum & ~1U;
    BT_TRACE_EXIT("app.drill_even_reps");
    return even;
}

int bt_view_has_commutator(const char *text) {
    BT_TRACE_ENTER("app.drill_has_commutator");
    int has = text[0] != '\0' && strcmp(text, "Not found.") != 0;
    BT_TRACE_EXIT("app.drill_has_commutator");
    return has;
}

static void alg_into_buffer(const bt_alg *alg, char out[BT_DRILL_TEXT_MAX]) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_alg_format(alg, &sb);
    snprintf(out, BT_DRILL_TEXT_MAX, "%s", sb.data != NULL ? sb.data : "");
    bt_strbuf_free(&sb);
}

int bt_view_expand_flat_line(const char *alg_text, char out[BT_DRILL_TEXT_MAX]) {
    BT_TRACE_ENTER("app.drill_expand_flat_line");
    bt_alg expanded;
    memset(&expanded, 0, sizeof expanded);
    if (!bt_expand(alg_text, &expanded)) {
        BT_TRACE_EXIT("app.drill_expand_flat_line");
        return 0;
    }
    char text[BT_DRILL_TEXT_MAX];
    alg_into_buffer(&expanded, text);
    bt_alg_free(&expanded);
    int changed = strcmp(text, alg_text) != 0;
    if (changed) {
        snprintf(out, BT_DRILL_TEXT_MAX, "expand %s", text);
    }
    BT_TRACE_EXIT("app.drill_expand_flat_line");
    return changed;
}

int bt_view_expand_alg_line(const char *commutator, const bt_alg *alg, const char *alg_text,
                            const char *displayed, char out[BT_DRILL_TEXT_MAX]) {
    BT_TRACE_ENTER("app.drill_expand_alg_line");
    bt_alg expanded;
    memset(&expanded, 0, sizeof expanded);
    if (bt_view_has_commutator(commutator)) {
        bt_expanded_list units;
        bt_expanded_list_init(&units);
        if (!bt_decompose_verified(commutator, alg, &units)) {
            bt_expanded_list_free(&units);
            BT_TRACE_EXIT("app.drill_expand_alg_line");
            return 0;
        }
        bt_alg moves;
        bt_alg_init(&moves);
        for (size_t i = 0; i < units.count; i++) {
            bt_alg unit_moves = bt_expanded_expansion(&units.items[i]);
            for (size_t m = 0; m < bt_alg_len(&unit_moves); m++) {
                if (!bt_alg_push(&moves, unit_moves.items[m])) {
                    bt_alg_free(&unit_moves);
                    bt_alg_free(&moves);
                    bt_expanded_list_free(&units);
                    BT_TRACE_FAIL("app.drill_expand_alg_line", "out of memory");
                    BT_TRACE_EXIT("app.drill_expand_alg_line");
                    return 0;
                }
            }
            bt_alg_free(&unit_moves);
        }
        bt_expand_cancel(&moves, &expanded);
        bt_alg_free(&moves);
        bt_expanded_list_free(&units);
    } else if (!bt_expand(alg_text, &expanded)) {
        BT_TRACE_EXIT("app.drill_expand_alg_line");
        return 0;
    }
    char text[BT_DRILL_TEXT_MAX];
    alg_into_buffer(&expanded, text);
    bt_alg_free(&expanded);
    int changed = strcmp(text, displayed) != 0;
    if (changed) {
        snprintf(out, BT_DRILL_TEXT_MAX, "expand %s", text);
    }
    BT_TRACE_EXIT("app.drill_expand_alg_line");
    return changed;
}

static void reset_counters(bt_drill_session *session) {
    session->reps_done = 0;
    session->reps_side_a = 1;
    session->reps_clock = 0.0F;
    session->last_split_at = 0.0F;
    session->split_count = 0;
    session->rate_step = 0;
}

static void clear_sides(bt_drill_session *session) {
    session->has_side_a = 0;
    session->has_side_b = 0;
}

void bt_view_install_pair(bt_drill_session *session, const bt_corpus *corpus,
                          const bt_stores *stores, bt_case case_id) {
    BT_TRACE_ENTER("app.drill_install_pair");
    session->has_pin = 1;
    session->pin = case_id;
    bt_drill_side a;
    bt_drill_side b;
    bt_drill_side_init(&a);
    bt_drill_side_init(&b);
    if (bt_view_build_sides(corpus, &case_id, &a, &b)) {
        bt_cube prepared = bt_cube_solved();
        bt_cube_apply_alg(&prepared, &b.alg);
        session->prepared = prepared;
        bt_drill_side_free(&session->side_a);
        bt_drill_side_free(&session->side_b);
        session->side_a = a;
        session->side_b = b;
        session->has_side_a = 1;
        session->has_side_b = 1;
        session->reps_target = bt_view_even_reps(stores->settings.drill_reps);
    } else {
        bt_drill_side_free(&a);
        bt_drill_side_free(&b);
        session->prepared = bt_cube_solved();
        clear_sides(session);
        session->reps_target = 0;
    }
    reset_counters(session);
    BT_TRACE_EXIT("app.drill_install_pair");
}

int bt_view_pair_is_due(const bt_stores *stores, long long now_day, const bt_case *case_id) {
    BT_TRACE_ENTER("app.drill_pair_is_due");
    bt_sheet_class sheet = bt_view_sheet_of(case_id->orbit);
    char positions[2][BT_DRILL_TEXT_MAX];
    bt_view_case_position(case_id, positions[0]);
    bt_case flipped = bt_view_flip_case(case_id);
    bt_view_case_position(&flipped, positions[1]);
    int all_carded = 1;
    int any_due = 0;
    for (int i = 0; i < 2; i++) {
        char key[BT_DRILL_TEXT_MAX];
        bt_view_card_key(sheet, positions[i], key);
        const bt_card *card = bt_srs_store_get(&stores->srs, key);
        if (card == NULL) {
            all_carded = 0;
        } else {
            any_due = any_due || bt_card_is_due(card, now_day);
        }
    }
    BT_TRACE_EXIT("app.drill_pair_is_due");
    return !all_carded || any_due;
}

int bt_view_pick_pair(const bt_corpus *corpus, const bt_stores *stores, long long now_day,
                      bt_rng *rng, bt_case *out) {
    BT_TRACE_ENTER("app.drill_pick_pair");
    bt_case tier_due[BT_PAIRS_MAX];
    size_t due_count = 0;
    bt_case tier_seen[BT_PAIRS_MAX];
    size_t seen_count = 0;
    for (int orbit = 0; orbit <= 1; orbit++) {
        bt_case cases[BT_PAIRS_MAX];
        size_t count = bt_scheme_enumerate_cases((bt_orbit)orbit, cases);
        for (size_t i = 0; i < count; i++) {
            char position[BT_DRILL_TEXT_MAX];
            bt_view_case_position(&cases[i], position);
            if (bt_corpus_rows_for_position(corpus, bt_view_sheet_of((bt_orbit)orbit), position,
                                            NULL, 0) == 0) {
                continue;
            }
            if (bt_view_pair_is_due(stores, now_day, &cases[i])) {
                tier_due[due_count++] = cases[i];
            } else {
                tier_seen[seen_count++] = cases[i];
            }
        }
    }
    bt_case *bucket = tier_due;
    size_t bucket_count = due_count;
    if (due_count == 0) {
        bucket = tier_seen;
        bucket_count = seen_count;
    }
    if (bucket_count == 0) {
        BT_TRACE_EXIT("app.drill_pick_pair");
        return 0;
    }
    *out = bucket[bt_rng_below(rng, bucket_count)];
    BT_TRACE_EXIT("app.drill_pick_pair");
    return 1;
}

void bt_view_write_review(bt_stores *stores, const bt_fsrs_config *fsrs, bt_sheet_class sheet,
                          const char *position, bt_grade rating, long long now_day) {
    BT_TRACE_ENTER("app.drill_write_review");
    char key[BT_DRILL_TEXT_MAX];
    bt_view_card_key(sheet, position, key);
    bt_card card;
    const bt_card *found = bt_srs_store_get(&stores->srs, key);
    if (found != NULL) {
        bt_card_clone(found, &card);
    } else {
        bt_card_init(&card, now_day);
    }
    bt_card_review(&card, rating, now_day, fsrs);
    bt_srs_store_set(&stores->srs, key, &card);
    bt_card_free(&card);
    BT_TRACE_EXIT("app.drill_write_review");
}

int bt_view_rating_of(int key, bt_grade *out) {
    BT_TRACE_ENTER("app.drill_rating_of");
    int mapped = 1;
    switch (key) {
    case 49:
        *out = BT_GRADE_AGAIN;
        break;
    case 50:
        *out = BT_GRADE_HARD;
        break;
    case 51:
        *out = BT_GRADE_GOOD;
        break;
    case 52:
        *out = BT_GRADE_EASY;
        break;
    default:
        mapped = 0;
        break;
    }
    BT_TRACE_EXIT("app.drill_rating_of");
    return mapped;
}

void bt_view_drill_net_set(bt_app_net_state *net, bt_cube cube, const bt_drill_side *side) {
    BT_TRACE_ENTER("app.drill_net_set");
    net->cube = cube;
    net->has_buffer = 1;
    net->buffer_sticker = bt_view_buffer_of(side->case_id.orbit);
    net->has_target[0] = 1;
    net->has_target[1] = 1;
    net->targets[0] = side->case_id.t1;
    net->targets[1] = side->case_id.t2;
    BT_TRACE_EXIT("app.drill_net_set");
}

void bt_view_drill_net_clear(bt_app_net_state *net) {
    BT_TRACE_ENTER("app.drill_net_clear");
    net->cube = bt_cube_solved();
    net->has_buffer = 0;
    net->has_target[0] = 0;
    net->has_target[1] = 0;
    net->targets[0] = 0;
    net->targets[1] = 0;
    BT_TRACE_EXIT("app.drill_net_clear");
}

void bt_view_drill_net_sync(bt_app_net_state *net, const bt_drill_session *session) {
    BT_TRACE_ENTER("app.drill_net_sync");
    const bt_drill_side *side_b = NULL;
    const bt_drill_side *side_a = bt_drill_session_sides(session, &side_b);
    if (side_a != NULL) {
        bt_view_drill_net_set(net, bt_cube_solved(), side_a);
    } else {
        bt_view_drill_net_clear(net);
    }
    BT_TRACE_EXIT("app.drill_net_sync");
}

static bt_drill_app_state drill_singleton;

static void clear_pair(bt_drill_session *session) {
    session->has_pin = 0;
    clear_sides(session);
    session->prepared = bt_cube_solved();
    session->reps_target = 0;
    reset_counters(session);
}

void bt_view_drill_app_init(bt_drill_app_state *app) {
    BT_TRACE_ENTER("app.drill_app_init");
    memset(app, 0, sizeof *app);
    bt_drill_session_init(&app->session);
    app->seed = bt_views_time_seed();
    app->last_phase = (bt_drill_phase)-1;
    app->initialized = 1;
    BT_TRACE_EXIT("app.drill_app_init");
}

void bt_view_drill_app_free(bt_drill_app_state *app) {
    BT_TRACE_ENTER("app.drill_app_free");
    bt_drill_session_free(&app->session);
    app->initialized = 0;
    BT_TRACE_EXIT("app.drill_app_free");
}

bt_drill_app_state *bt_view_drill_app(void) {
    BT_TRACE_ENTER("app.drill_app");
    if (!drill_singleton.initialized) {
        bt_view_drill_app_init(&drill_singleton);
    }
    BT_TRACE_EXIT("app.drill_app");
    return &drill_singleton;
}

void bt_view_drill_set_pending(bt_case case_id) {
    BT_TRACE_ENTER("app.drill_set_pending");
    bt_drill_app_state *app = bt_view_drill_app();
    app->has_pending = 1;
    app->pending = case_id;
    BT_TRACE_EXIT("app.drill_set_pending");
}

static void pick_and_install(bt_app_ctx *ctx, bt_drill_app_state *app, int use_pending) {
    bt_case picked;
    int has = 0;
    if (use_pending && app->has_pending) {
        picked = app->pending;
        app->has_pending = 0;
        has = 1;
    } else if (ctx->has_corpus && ctx->has_stores) {
        bt_rng rng;
        bt_rng_seed(&rng, app->seed);
        has =
            bt_view_pick_pair(&ctx->vendor_corpus, &ctx->stores, bt_views_now_day(), &rng, &picked);
    }
    if (has) {
        bt_view_install_pair(&app->session, &ctx->vendor_corpus, &ctx->stores, picked);
    } else {
        clear_pair(&app->session);
    }
}

void bt_view_drill_enter_state(bt_app_ctx *ctx, bt_drill_app_state *app) {
    BT_TRACE_ENTER("app.drill_enter_state");
    pick_and_install(ctx, app, 1);
    ctx->drill_phase = BT_DRILL_CARD;
    bt_view_drill_net_sync(&ctx->net, &app->session);
    app->last_phase = BT_DRILL_CARD;
    BT_TRACE_EXIT("app.drill_enter_state");
}

void bt_view_drill_enter_phase(bt_app_ctx *ctx, bt_drill_app_state *app, bt_drill_phase phase) {
    BT_TRACE_ENTER("app.drill_enter_phase");
    if (phase == BT_DRILL_CARD) {
        bt_view_drill_net_sync(&ctx->net, &app->session);
    } else if (phase == BT_DRILL_REPS) {
        app->session.reps_clock = 0.0F;
        app->session.last_split_at = 0.0F;
        const bt_drill_side *side = bt_drill_session_current_side(&app->session);
        if (side != NULL) {
            bt_view_drill_net_set(&ctx->net, app->session.prepared, side);
        }
    }
    app->last_phase = phase;
    BT_TRACE_EXIT("app.drill_enter_phase");
}

void bt_view_drill_tick(bt_app_ctx *ctx, bt_drill_app_state *app, float dt) {
    BT_TRACE_ENTER("app.drill_tick");
    if (ctx->drill_phase == BT_DRILL_REPS) {
        app->session.reps_clock += dt;
    }
    BT_TRACE_EXIT("app.drill_tick");
}

void bt_view_drill_key(bt_app_ctx *ctx, bt_drill_app_state *app, int key) {
    BT_TRACE_ENTER("app.drill_key");
    if (ctx->drill_phase == BT_DRILL_CARD) {
        if (key == KEY_SPACE && bt_drill_session_sides(&app->session, NULL) != NULL) {
            ctx->drill_phase = BT_DRILL_REPS;
            bt_view_drill_enter_phase(ctx, app, BT_DRILL_REPS);
        }
        BT_TRACE_EXIT("app.drill_key");
        return;
    }
    if (ctx->drill_phase == BT_DRILL_REPS) {
        if (key != KEY_SPACE) {
            BT_TRACE_EXIT("app.drill_key");
            return;
        }
        const bt_drill_side *side = bt_drill_session_current_side(&app->session);
        if (side == NULL) {
            BT_TRACE_EXIT("app.drill_key");
            return;
        }
        float split = app->session.reps_clock - app->session.last_split_at;
        if (split < 0.0F) {
            split = 0.0F;
        }
        app->session.last_split_at = app->session.reps_clock;
        if (bt_drill_push_split(&app->session, app->session.reps_side_a, split) != 0) {
            BT_TRACE_EXIT("app.drill_key");
            return;
        }
        bt_cube_apply_alg(&ctx->net.cube, &side->alg);
        app->session.reps_side_a = app->session.reps_side_a == 0;
        app->session.reps_done += 1;
        const bt_drill_side *next = bt_drill_session_current_side(&app->session);
        if (next != NULL) {
            bt_view_drill_net_set(&ctx->net, ctx->net.cube, next);
        }
        if (app->session.reps_done >= app->session.reps_target) {
            ctx->drill_phase = BT_DRILL_RATE;
            bt_view_drill_enter_phase(ctx, app, BT_DRILL_RATE);
        }
        BT_TRACE_EXIT("app.drill_key");
        return;
    }
    if (ctx->drill_phase == BT_DRILL_RATE) {
        bt_grade rating;
        if (!bt_view_rating_of(key, &rating)) {
            BT_TRACE_EXIT("app.drill_key");
            return;
        }
        const bt_drill_side *side_b = NULL;
        const bt_drill_side *a = bt_drill_session_sides(&app->session, &side_b);
        if (a == NULL) {
            BT_TRACE_EXIT("app.drill_key");
            return;
        }
        const char *position = app->session.rate_step == 0 ? a->position : side_b->position;
        bt_view_write_review(&ctx->stores, &ctx->fsrs, bt_view_sheet_of(a->case_id.orbit), position,
                             rating, bt_views_now_day());
        bt_app_save_stores(ctx);
        app->session.rate_step += 1;
        if (app->session.rate_step > 1) {
            pick_and_install(ctx, app, 0);
            app->seed += 1;
            ctx->drill_phase = BT_DRILL_CARD;
            bt_view_drill_enter_phase(ctx, app, BT_DRILL_CARD);
        }
        BT_TRACE_EXIT("app.drill_key");
        return;
    }
    BT_TRACE_EXIT("app.drill_key");
}

void bt_view_drill_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.drill_enter");
    bt_view_drill_enter_state(ctx, bt_view_drill_app());
    BT_TRACE_EXIT("app.drill_enter");
}

void bt_view_drill_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.drill_update");
    bt_drill_app_state *app = bt_view_drill_app();
    if (app->last_phase != ctx->drill_phase) {
        bt_view_drill_enter_phase(ctx, app, ctx->drill_phase);
    }
    bt_view_drill_tick(ctx, app, GetFrameTime());
    int keys[5] = {KEY_SPACE, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(keys[i])) {
            bt_view_drill_key(ctx, app, keys[i]);
        }
    }
    BT_TRACE_EXIT("app.drill_update");
}
