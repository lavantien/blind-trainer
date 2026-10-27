#include "app/views/drill.h"
#include "app/views/drill_view.h"
#include "app/views/memo.h"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    KEY_SPACE = 32,
    KEY_ONE = 49,
    KEY_THREE = 51,
    KEY_R = 82,
};

static bt_corpus_row heap_row(const char *position, const char *alg) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup(position);
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup("");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static void fill_flow_ctx(bt_app_ctx *ctx, const char *tag) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_DRILL;
    ctx->has_corpus = 1;
    bt_corpus_row rows[1] = {heap_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2")};
    bt_corpus_row *heap = malloc(sizeof rows);
    memcpy(heap, rows, sizeof rows);
    bt_corpus_from_rows(heap, 1, &ctx->vendor_corpus);
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    ctx->stores.settings.memo_mode = BT_MEMO_PER_ORBIT;
    ctx->stores.settings.drill_reps = 4;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    bt_fsrs_config_defaults(&ctx->fsrs);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s",
             ta_temp_dir("view_flow", tag));
}

static bt_case edge_ab(void) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'A');
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'B');
    return case_id;
}

BT_TEST(view_flow, drill_runs_card_reps_rate_and_repicks) {
    bt_app_ctx ctx;
    fill_flow_ctx(&ctx, "drill");
    bt_drill_app_state *app = bt_view_drill_app();
    bt_view_drill_app_init(app);
    app->seed = 20261012;
    bt_view_drill_enter_state(&ctx, app);
    BT_CHECK(ctx.drill_phase == BT_DRILL_CARD);
    BT_CHECK(bt_drill_session_sides(&app->session, NULL) != NULL);
    BT_CHECK(ctx.net.has_target[0] && ctx.net.targets[0] == edge_ab().t1);
    BT_CHECK(bt_cube_is_solved(&ctx.net.cube));

    bt_view_drill_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.drill_phase == BT_DRILL_REPS);
    BT_CHECK(app->session.reps_clock == 0.0F);
    BT_CHECK(!bt_cube_is_solved(&ctx.net.cube));

    bt_view_drill_tick(&ctx, app, 0.5F);
    bt_view_drill_tick(&ctx, app, 0.5F);
    BT_CHECK(app->session.reps_clock >= 0.99F && app->session.reps_clock <= 1.01F);

    for (unsigned rep = 1; rep <= 4; rep++) {
        bt_view_drill_key(&ctx, app, KEY_SPACE);
        BT_CHECK(app->session.reps_done == rep);
        if (rep < 4) {
            BT_CHECK(ctx.drill_phase == BT_DRILL_REPS);
        }
    }
    BT_CHECK(ctx.drill_phase == BT_DRILL_RATE);
    BT_CHECK(app->session.split_count == 4);
    BT_CHECK(bt_cube_eq(&ctx.net.cube, &app->session.prepared));

    bt_view_drill_key(&ctx, app, KEY_ONE);
    BT_CHECK(ctx.drill_phase == BT_DRILL_RATE);
    BT_CHECK(app->session.rate_step == 1);
    char key_a[BT_DRILL_TEXT_MAX];
    bt_view_card_key(BT_SHEET_EDGE, "UF-UB-UR", key_a);
    BT_CHECK(bt_srs_store_get(&ctx.stores.srs, key_a) != NULL);

    bt_view_drill_key(&ctx, app, KEY_THREE);
    BT_CHECK(ctx.drill_phase == BT_DRILL_CARD);
    BT_CHECK(app->session.reps_done == 0);
    BT_CHECK(app->session.rate_step == 0);
    BT_CHECK(app->session.has_pin);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_flow, memo_streams_rates_and_restreams) {
    bt_app_ctx ctx;
    fill_flow_ctx(&ctx, "memo");
    bt_memo_app_state *app = bt_view_memo_app();
    bt_view_memo_app_init(app);
    app->seed = 12345;
    bt_view_memo_enter_state(&ctx, app);
    BT_CHECK(ctx.memo_phase == BT_MEMO_INTRO);
    BT_CHECK(app->session.token_count > 0);

    bt_view_memo_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.memo_phase == BT_MEMO_STREAM);
    BT_CHECK(app->session.seed == 12346);
    BT_CHECK(app->session.shown == 0);

    float cadence = bt_memo_cadence_secs(&app->session, &ctx.stores);
    bt_view_memo_tick(&ctx, app, cadence * 1.5F);
    BT_CHECK(app->session.shown == 1);
    bt_view_memo_tick(&ctx, app, cadence * 32.0F);
    BT_CHECK(app->session.shown == app->session.token_count);
    BT_CHECK(ctx.memo_phase == BT_MEMO_RECALL);

    bt_view_memo_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.memo_phase == BT_MEMO_REVIEW);

    bt_view_memo_key(&ctx, app, KEY_THREE);
    BT_CHECK(ctx.memo_phase == BT_MEMO_INTRO);
    BT_CHECK(bt_srs_store_len(&ctx.stores.srs) == app->session.token_count);

    bt_view_memo_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.memo_phase == BT_MEMO_STREAM);
    bt_view_memo_tick(&ctx, app, cadence * 32.0F);
    bt_view_memo_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.memo_phase == BT_MEMO_REVIEW);
    bt_view_memo_key(&ctx, app, KEY_R);
    BT_CHECK(ctx.memo_phase == BT_MEMO_STREAM);
    BT_CHECK(app->session.shown == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(view_flow, drill_pending_case_is_consumed_once) {
    bt_app_ctx ctx;
    fill_flow_ctx(&ctx, "pending");
    bt_drill_app_state *app = bt_view_drill_app();
    bt_view_drill_app_init(app);
    app->seed = 20261012;
    bt_case pending;
    memset(&pending, 0, sizeof pending);
    pending.orbit = BT_ORBIT_CORNER;
    pending.t1 = bt_scheme_sticker_of(BT_ORBIT_CORNER, 'A');
    pending.t2 = bt_scheme_sticker_of(BT_ORBIT_CORNER, 'B');
    bt_view_drill_set_pending(pending);
    bt_view_drill_enter_state(&ctx, app);
    BT_CHECK(app->session.has_pin && app->session.pin.orbit == BT_ORBIT_CORNER);
    BT_CHECK(app->session.has_side_a == 0);
    BT_CHECK(!app->has_pending);
    bt_app_ctx_free(&ctx);
}
