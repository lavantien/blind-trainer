#include "app/boot.h"
#include "app/open_url.h"
#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/state.h"

#include "common.h"

enum {
    KEY_SLASH = 47,
    KEY_D = 68,
    KEY_R = 82,
    KEY_DOWN = 264,
    KEY_BACKSPACE = 259,
};

enum {
    BT_VIEWS_ALGS_URL_MAX = 256,
};

static int g_recon_calls;
static int g_recon_result;
static char g_recon_last[BT_VIEWS_ALGS_URL_MAX];

static int stub_launch(const char *url) {
    g_recon_calls++;
    snprintf(g_recon_last, sizeof g_recon_last, "%s", url);
    return g_recon_result;
}

static void stub_reset(int result) {
    g_recon_calls = 0;
    g_recon_last[0] = '\0';
    g_recon_result = result;
}

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

static void covered_corpus_recon(bt_app_ctx *ctx, const char *recon) {
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
    row.recon_link = _strdup(recon);
    row.youtube_link = _strdup("https://example.test/recon");
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, &row, sizeof *heap);
    bt_corpus_from_rows(heap, 1, &ctx->vendor_corpus);
    ctx->has_corpus = 1;
}

static void covered_corpus(bt_app_ctx *ctx) {
    covered_corpus_recon(ctx, "");
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

BT_TEST(views_algs, typed_chars_filter_the_list_through_the_production_frame) {
    bt_app_ctx ctx = algs_ctx("querywire");
    covered_corpus(&ctx);
    bt_view_algs_read_query(&ctx, "");
    bt_view_algs_enter(&ctx);
    bt_view_algs_app_state *app = bt_view_algs_app();
    size_t all = app->browser.all_count;
    BT_CHECK_EQ(app->browser.filtered_count, all);

    bt_view_algs_detail_line *lines = NULL;
    size_t line_count = bt_view_algs_detail_lines(&ctx.vendor_corpus, &app->browser, &lines);
    BT_CHECK(line_count > 0);
    char before[BT_ALGS_TEXT_MAX];
    snprintf(before, sizeof before, "%s", lines[0].text);
    free(lines);

    int slash[1] = {KEY_SLASH};
    bt_view_algs_frame(&ctx, slash, 1);
    BT_CHECK(app->query_focused);

    int typed[1] = {'b'};
    bt_view_algs_frame(&ctx, typed, 1);
    bt_view_algs_refresh(app->query, &app->browser);
    BT_CHECK(strcmp(app->query, "b") == 0);
    BT_CHECK(app->browser.filtered_count > 0);
    BT_CHECK(app->browser.filtered_count < all);

    lines = NULL;
    line_count = bt_view_algs_detail_lines(&ctx.vendor_corpus, &app->browser, &lines);
    BT_CHECK(line_count > 0);
    BT_CHECK(strcmp(lines[0].text, before) != 0);
    free(lines);

    int back[1] = {KEY_BACKSPACE};
    bt_view_algs_frame(&ctx, back, 1);
    bt_view_algs_refresh(app->query, &app->browser);
    BT_CHECK_EQ(app->query[0], '\0');
    BT_CHECK_EQ(app->browser.filtered_count, all);

    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.chars[0] = 'b';
    input.char_count = 1;
    bt_app_frame_input_set(&input);
    bt_view_algs_update(&ctx);
    bt_app_frame_input_set(NULL);
    BT_CHECK(strcmp(app->query, "b") == 0);
    BT_CHECK(app->browser.filtered_count > 0);
    BT_CHECK(app->browser.filtered_count < all);

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

BT_TEST(views_algs, r_key_opens_the_selected_case_recon_through_the_seam) {
    bt_app_ctx ctx = algs_ctx("reconkey");
    covered_corpus_recon(&ctx, "https://example.test/recon");
    bt_view_algs_enter(&ctx);
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    int r[1] = {KEY_R};
    bt_view_algs_frame(&ctx, r, 1);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 1);
    BT_CHECK(strcmp(g_recon_last, "https://example.test/recon") == 0);
    bt_app_ctx_free(&ctx);

    bt_app_ctx blank = algs_ctx("reconblank");
    covered_corpus(&blank);
    bt_view_algs_enter(&blank);
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    bt_view_algs_frame(&blank, r, 1);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 0);
    bt_app_ctx_free(&blank);

    bt_app_ctx typing = algs_ctx("recontyping");
    covered_corpus_recon(&typing, "https://example.test/recon");
    bt_view_algs_read_query(&typing, "");
    bt_view_algs_enter(&typing);
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    int slash[1] = {KEY_SLASH};
    bt_view_algs_frame(&typing, slash, 1);
    bt_view_algs_frame(&typing, r, 1);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 0);
    BT_CHECK_EQ(bt_view_algs_app()->query, "R");
    bt_app_ctx_free(&typing);
}

BT_TEST(views_algs, recon_click_hits_the_recorded_link_rect_through_the_seam) {
    bt_app_ctx ctx = algs_ctx("reconclick");
    covered_corpus_recon(&ctx, "https://example.test/recon");
    bt_view_algs_enter(&ctx);
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_view_algs_link_record(app, 500.0F, 300.0F, 240.0F, 44.0F, "https://example.test/recon");
    BT_CHECK(app->link_ready);

    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.click = 1;
    input.x = 500.0F;
    input.y = 310.0F;
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    bt_app_frame_input_set(&input);
    bt_view_algs_update(&ctx);
    bt_app_frame_input_set(NULL);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 1);
    BT_CHECK(strcmp(g_recon_last, "https://example.test/recon") == 0);

    input.x = 100.0F;
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    bt_app_frame_input_set(&input);
    bt_view_algs_update(&ctx);
    bt_app_frame_input_set(NULL);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 0);

    app->link_ready = 0;
    input.x = 500.0F;
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    bt_app_frame_input_set(&input);
    bt_view_algs_update(&ctx);
    bt_app_frame_input_set(NULL);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(g_recon_calls, 0);
    bt_app_ctx_free(&ctx);
}
