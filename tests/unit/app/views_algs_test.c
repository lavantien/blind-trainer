#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/state.h"

#include "common.h"

enum {
    KEY_SLASH = 47,
    KEY_D = 68,
    KEY_DOWN = 264,
};

static bt_app_ctx algs_ctx(const char *tag) {
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    bt_app_config_defaults(&ctx.config);
    ctx.state = BT_APP_ALGS;
    ctx.has_stores = 1;
    memset(&ctx.stores, 0, sizeof ctx.stores);
    bt_srs_store_init(&ctx.stores.srs);
    bt_solve_log_init(&ctx.stores.solves);
    bt_user_algs_init(&ctx.stores.user_algs);
    bt_str_list_init(&ctx.stores.seeds.hidden);
    bt_str_list_init(&ctx.stores.seeds.warnings);
    bt_fsrs_config_defaults(&ctx.fsrs);
    snprintf(ctx.paths.user_root, sizeof ctx.paths.user_root, "%s", ta_temp_dir("views_algs", tag));
    return ctx;
}

static bt_case edge_case(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, second);
    return case_id;
}

static void covered_corpus(bt_app_ctx *ctx) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup("UF-UB-UR");
    char err_token[64];
    if (!bt_alg_parse("R2 U' S R2 S' R2 U R2", &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg: %s", err_token);
    }
    row.alg_text = _strdup("R2 U' S R2 S' R2 U R2");
    row.commutator = _strdup("R2 U':[S,R2]");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("https://example.test/recon");
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, &row, sizeof *heap);
    bt_corpus_from_rows(heap, 1, &ctx->vendor_corpus);
    ctx->has_corpus = 1;
}

BT_TEST(views_algs, algs_search_routes_the_selected_case_into_drill) {
    bt_app_ctx ctx = algs_ctx("route");
    covered_corpus(&ctx);
    bt_view_algs_enter(&ctx);
    bt_view_algs_read_query(&ctx, "ab");
    bt_view_algs_refresh("ab", &bt_view_algs_app()->browser);
    bt_view_algs_app_state *app = bt_view_algs_app();
    BT_CHECK_EQ(app->browser.filtered_count, (size_t)2);
    BT_CHECK_EQ(app->browser.selected, (size_t)0);
    BT_CHECK_EQ(app->browser.filtered[0].orbit, BT_ORBIT_EDGE);

    bt_view_algs_key(&ctx, KEY_D);
    BT_CHECK(ctx.state == BT_APP_DRILL);
    bt_view_drill_enter(&ctx);
    bt_drill_app_state *drill = bt_view_drill_app();
    BT_CHECK(drill->session.has_side_a);
    BT_CHECK(drill->session.side_a.case_id.orbit == edge_case('A', 'B').orbit);
    BT_CHECK(drill->session.side_a.case_id.t1 == edge_case('A', 'B').t1);
    BT_CHECK(drill->session.side_a.case_id.t2 == edge_case('A', 'B').t2);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_algs, arrows_keep_navigating_while_the_filter_box_is_focused) {
    bt_app_ctx ctx = algs_ctx("focusedarrows");
    bt_view_algs_enter(&ctx);
    bt_view_algs_key(&ctx, KEY_SLASH);
    BT_CHECK(bt_view_algs_app()->query_focused);
    ctx.input_focus = 1;
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK_EQ(bt_view_algs_app()->browser.selected, (size_t)1);

    bt_view_algs_key(&ctx, KEY_D);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_algs, escape_releases_a_focused_text_input_before_leaving_the_view) {
    bt_app_ctx ctx = algs_ctx("escapedefocus");
    bt_view_algs_enter(&ctx);
    bt_view_algs_key(&ctx, KEY_SLASH);
    ctx.input_focus = 1;

    bt_app_key_event escape;
    memset(&escape, 0, sizeof escape);
    escape.kind = BT_APP_KEY_ESCAPE;
    escape.pressed = 1;
    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    BT_CHECK(!ctx.input_focus);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK(!bt_view_algs_app()->query_focused);

    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_MENU);
    bt_app_ctx_free(&ctx);
}
