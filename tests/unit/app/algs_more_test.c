#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/state.h"

#include "common.h"

enum {
    KEY_ENTER = 257,
    KEY_N = 78,
    KEY_P = 80,
    KEY_D = 68,
    KEY_DOWN = 264,
    KEY_UP = 265,
    KEY_RIGHT = 262,
    KEY_LEFT = 263,
};

static const char *COVERED_ALG = "R2 U' S R2 S' R2 U R2";
static const char *COVERED_POSITION = "UF-UB-UR";

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
    snprintf(ctx.paths.user_root, sizeof ctx.paths.user_root, "%s", ta_temp_dir("algs_more", tag));
    return ctx;
}

static bt_corpus_row covered_row(const char *alg, const char *commutator) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup(COVERED_POSITION);
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup(commutator);
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("https://example.test/recon");
    return row;
}

static bt_corpus_row alt_row(const char *alg) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 2;
    row.position = _strdup(COVERED_POSITION);
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup("");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("https://example.test/video");
    return row;
}

static void load_rows(bt_app_ctx *ctx, bt_corpus_row *rows, size_t count) {
    bt_corpus_row *heap = malloc(count * sizeof *heap);
    memcpy(heap, rows, count * sizeof *heap);
    bt_corpus_from_rows(heap, count, &ctx->vendor_corpus);
    ctx->has_corpus = 1;
}

static void query_algs(bt_app_ctx *ctx, const char *text) {
    bt_view_algs_enter(ctx);
    bt_view_algs_read_query(ctx, text);
    bt_view_algs_refresh(text, &bt_view_algs_app()->browser);
}

static int detail_has(bt_view_algs_detail_line *lines, size_t count, const char *text) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(lines[i].text, text) == 0) {
            return 1;
        }
    }
    return 0;
}

BT_TEST(algs_more, slash_focuses_the_filter_and_plain_keys_never_grab_it) {
    bt_app_ctx ctx = algs_ctx("moreslash");
    bt_view_algs_enter(&ctx);
    BT_CHECK(!bt_view_algs_app()->query_focused);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK(!bt_view_algs_app()->query_focused);
    bt_view_algs_key(&ctx, 47 /* slash */);
    BT_CHECK(bt_view_algs_app()->query_focused);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, typed_prefix_flows_from_the_input_into_the_filter) {
    bt_app_ctx ctx = algs_ctx("moretype");
    query_algs(&ctx, " ab ");
    bt_view_algs_app_state *app = bt_view_algs_app();
    BT_CHECK(strcmp(app->query, " ab ") == 0);
    BT_CHECK_EQ(app->browser.filtered_count, (size_t)2);
    BT_CHECK_EQ(app->browser.selected, (size_t)0);
    BT_CHECK_EQ(app->browser.filtered[0].orbit, BT_ORBIT_EDGE);
    BT_CHECK_EQ(app->browser.filtered[1].orbit, BT_ORBIT_CORNER);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, the_list_bind_renders_caret_rows_and_blanks_the_overflow) {
    bt_app_ctx ctx = algs_ctx("morecaret");
    query_algs(&ctx, "ab");
    bt_view_case_row rows[6];
    size_t count = bt_view_algs_list_rows(&bt_view_algs_app()->browser, 6, rows);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(rows[0].label, "> AB edge") == 0);
    BT_CHECK(rows[0].selected);
    BT_CHECK(strcmp(rows[1].label, "  AB corner") == 0);
    BT_CHECK(!rows[1].selected);
    bt_view_algs_key(&ctx, KEY_DOWN);
    count = bt_view_algs_list_rows(&bt_view_algs_app()->browser, 6, rows);
    BT_CHECK(strcmp(rows[0].label, "  AB edge") == 0);
    BT_CHECK(!rows[0].selected);
    BT_CHECK(strcmp(rows[1].label, "> AB corner") == 0);
    BT_CHECK(rows[1].selected);
    bt_app_ctx_free(&ctx);
}

static int colors_equal(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

static int detail_line_is(const bt_view_algs_detail_line *lines, size_t count, const char *text,
                          bt_theme_color color) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(lines[i].text, text) == 0 && colors_equal(lines[i].color, color)) {
            return 1;
        }
    }
    return 0;
}

BT_TEST(algs_more, the_detail_shows_coverage_mechanics_meta_and_links) {
    bt_app_ctx ctx = algs_ctx("moredetail");
    bt_corpus_row row = covered_row(COVERED_ALG, "R2 U':[S,R2]");
    load_rows(&ctx, &row, 1);
    query_algs(&ctx, "ab");
    bt_view_algs_detail_line lines[BT_ALGS_ROWS_MAX * 2];
    size_t count =
        bt_view_algs_detail_lines(&ctx.vendor_corpus, &bt_view_algs_app()->browser, lines);
    char meta[BT_ALGS_TEXT_MAX];
    bt_view_algs_meta_line(&ctx.vendor_corpus.rows[0], meta);
    BT_CHECK(detail_has(lines, count, "2 of 818 cases match"));
    BT_CHECK(detail_has(lines, count, "AB"));
    BT_CHECK(detail_has(lines, count, "edge UF-UB-UR covered yes"));
    BT_CHECK(detail_has(lines, count, meta));
    BT_CHECK(detail_has(lines, count, "recon  video https://example.test/recon"));
    BT_CHECK(detail_line_is(lines, count, "R2 U': ", bt_theme_setup()));
    BT_CHECK(detail_line_is(lines, count, "[", bt_theme_text_muted()));
    BT_CHECK(detail_line_is(lines, count, "S", bt_theme_teal()));
    BT_CHECK(detail_line_is(lines, count, ",", bt_theme_text_muted()));
    BT_CHECK(detail_line_is(lines, count, "R2", bt_theme_interchange()));
    BT_CHECK(detail_line_is(lines, count, "]", bt_theme_text_muted()));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, extra_vendor_rows_render_as_alternatives_with_their_own_meta) {
    bt_app_ctx ctx = algs_ctx("morealt");
    bt_corpus_row rows[2] = {
        covered_row(COVERED_ALG, "R2 U':[S,R2]"),
        alt_row("R2 U' R2 S R2 S' U R2"),
    };
    load_rows(&ctx, rows, 2);
    query_algs(&ctx, "ab");
    bt_view_algs_detail_line lines[BT_ALGS_ROWS_MAX * 2];
    size_t count =
        bt_view_algs_detail_lines(&ctx.vendor_corpus, &bt_view_algs_app()->browser, lines);
    BT_CHECK(detail_has(lines, count, "R2 U' R2 S R2 S' U R2"));
    char meta[BT_ALGS_TEXT_MAX];
    bt_view_algs_meta_line(&ctx.vendor_corpus.rows[1], meta);
    BT_CHECK(detail_has(lines, count, meta));
    BT_CHECK(strstr(meta, "problem(s)]") != NULL);
    int recon_lines = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(lines[i].text, "recon", 5) == 0) {
            recon_lines++;
        }
    }
    BT_CHECK(recon_lines >= 2);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, movement_keys_step_wrap_and_respect_the_typing_guard) {
    bt_app_ctx ctx = algs_ctx("moremove");
    query_algs(&ctx, "");
    bt_view_algs_app_state *app = bt_view_algs_app();
    size_t total = app->browser.all_count;

    bt_view_algs_key(&ctx, KEY_UP);
    BT_CHECK_EQ(app->browser.selected, total - 1);
    bt_view_algs_key(&ctx, KEY_N);
    BT_CHECK_EQ(app->browser.selected, (size_t)0);
    bt_view_algs_key(&ctx, KEY_P);
    BT_CHECK_EQ(app->browser.selected, total - 1);

    bt_view_algs_key(&ctx, 47 /* slash */);
    ctx.input_focus = 1;
    size_t before = app->browser.selected;
    bt_view_algs_key(&ctx, KEY_N);
    bt_view_algs_key(&ctx, KEY_P);
    BT_CHECK_EQ(app->browser.selected, before);
    bt_view_algs_key(&ctx, KEY_ENTER);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, movement_wraps_inside_a_small_match_set) {
    bt_app_ctx ctx = algs_ctx("morewrap");
    query_algs(&ctx, "ab");
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK_EQ(bt_view_algs_app()->browser.selected, (size_t)1);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK_EQ(bt_view_algs_app()->browser.selected, (size_t)0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, enter_sends_the_selected_case_to_the_drill) {
    bt_app_ctx ctx = algs_ctx("moreenter");
    bt_corpus_row row = covered_row(COVERED_ALG, "R2 U':[S,R2]");
    load_rows(&ctx, &row, 1);
    query_algs(&ctx, "ab");
    bt_view_algs_key(&ctx, KEY_ENTER);
    BT_CHECK(ctx.state == BT_APP_DRILL);
    bt_view_drill_enter(&ctx);
    bt_drill_app_state *drill = bt_view_drill_app();
    BT_CHECK(drill->session.has_side_a);
    char letters[3];
    bt_drill_side_letters(&drill->session.side_a, letters);
    BT_CHECK(strcmp(letters, "AB") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, a_shrunk_match_set_clamps_the_selection_and_blocks_the_handoff) {
    bt_app_ctx ctx = algs_ctx("moreshrink");
    query_algs(&ctx, "ab");
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK_EQ(bt_view_algs_app()->browser.selected, (size_t)1);
    bt_view_algs_read_query(&ctx, "abc");
    bt_view_algs_refresh("abc", &bt_view_algs_app()->browser);
    bt_view_algs_app_state *app = bt_view_algs_app();
    BT_CHECK_EQ(app->browser.filtered_count, (size_t)0);
    BT_CHECK_EQ(app->browser.selected, (size_t)0);

    bt_view_algs_key(&ctx, KEY_DOWN);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK_EQ(app->browser.selected, (size_t)0);
    bt_view_algs_key(&ctx, KEY_ENTER);
    bt_view_algs_key(&ctx, KEY_D);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    bt_view_case_row rows[6];
    size_t count = bt_view_algs_list_rows(&app->browser, 6, rows);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(rows[0].label, "no case matches the query") == 0);
    bt_view_algs_detail_line lines[BT_ALGS_ROWS_MAX * 2];
    size_t detail_count = bt_view_algs_detail_lines(&ctx.vendor_corpus, &app->browser, lines);
    BT_CHECK(detail_has(lines, detail_count, "0 of 818 cases match"));
    BT_CHECK(detail_has(lines, detail_count, "no case matches the query"));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, meta_line_badges_verification_and_problem_counts) {
    bt_corpus_row verified = covered_row(COVERED_ALG, "");
    char meta[BT_ALGS_TEXT_MAX];
    bt_view_algs_meta_line(&verified, meta);
    char wanted[BT_ALGS_TEXT_MAX];
    snprintf(wanted, sizeof wanted, "cherry qtm %u htm %u [verified]", bt_alg_qtm(&verified.alg),
             bt_alg_htm(&verified.alg));
    BT_CHECK(strcmp(meta, wanted) == 0);

    bt_corpus_row broken;
    memset(&broken, 0, sizeof broken);
    broken.solver = BT_SOLVER_EGGINS;
    broken.sheet = BT_SHEET_EDGE;
    broken.rowid = 1;
    broken.position = _strdup(COVERED_POSITION);
    bt_alg_init(&broken.alg);
    broken.alg_text = _strdup("");
    broken.commutator = _strdup("");
    broken.recon_link = _strdup("");
    broken.youtube_link = _strdup("");
    bt_view_algs_meta_line(&broken, meta);
    BT_CHECK(strcmp(meta, "eggins qtm 0 htm 0 [6 problem(s)]") == 0);
}

static void repo_walk_to_case(bt_app_ctx *ctx) {
    int keys[5] = {KEY_RIGHT, KEY_DOWN, KEY_RIGHT, KEY_RIGHT, KEY_RIGHT};
    bt_view_algs_frame(ctx, keys, 5);
}

BT_TEST(algs_more, the_production_frame_browses_packs_and_edits_overrides) {
    bt_app_ctx ctx = algs_ctx("frame");
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup("UF-UB-UR");
    char err_token[64];
    BT_CHECK(bt_alg_parse(COVERED_ALG, &row.alg, err_token, sizeof err_token));
    row.alg_text = _strdup(COVERED_ALG);
    row.commutator = _strdup("");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    load_rows(&ctx, &row, 1);

    bt_seed_row rows[1];
    memset(&rows[0], 0, sizeof rows[0]);
    snprintf(rows[0].event, sizeof rows[0].event, "%s", "2x2");
    snprintf(rows[0].method, sizeof rows[0].method, "%s", "cll");
    snprintf(rows[0].stage, sizeof rows[0].stage, "%s", "cll");
    snprintf(rows[0].case_key, sizeof rows[0].case_key, "%s", "cll.1");
    rows[0].alg = _strdup("R U R'");
    rows[0].front_json = _strdup("{}");
    rows[0].source_url = _strdup("https://example.test");
    rows[0].source_at = _strdup("2026-01-01");
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_pack_entry packs[1];
    memset(&packs[0], 0, sizeof packs[0]);
    snprintf(packs[0].file, sizeof packs[0].file, "%s", "cll.tsv");
    snprintf(packs[0].event, sizeof packs[0].event, "%s", "2x2");
    snprintf(packs[0].method, sizeof packs[0].method, "%s", "cll");
    snprintf(packs[0].stage, sizeof packs[0].stage, "%s", "cll");
    packs[0].expected_rows = 1;
    bt_view_algs_enter(&ctx);
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_repo_browser_free(repo);
    repo->arms = bt_repo_arms(packs, 1, &hidden, rows, 1, &repo->arm_count);
    bt_str_list_free(&hidden);

    repo_walk_to_case(&ctx);
    BT_CHECK(repo->repo_focused);
    BT_CHECK(strcmp(bt_repo_browser_node(repo)->label, "cll.1") == 0);

    int open_key[1] = {79 /* o */};
    bt_view_algs_frame(&ctx, open_key, 1);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    BT_CHECK(strcmp(draft->site.case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(draft->text, "R U R'") == 0);

    int cancel_key[1] = {258 /* tab */};
    bt_view_algs_frame(&ctx, cancel_key, 1);
    BT_CHECK(!draft->has_case);

    bt_view_algs_frame(&ctx, open_key, 1);
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "F R F'");
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(bt_overrides_contains(bt_view_algs_overrides(), &key));
    BT_CHECK(!draft->has_case);

    int delete_key[1] = {88 /* x */};
    bt_view_algs_frame(&ctx, delete_key, 1);
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));

    int enter_key[1] = {KEY_ENTER};
    bt_view_algs_frame(&ctx, enter_key, 1);
    BT_CHECK(ctx.state == BT_APP_ALGS);

    int d_key[1] = {KEY_D};
    bt_view_algs_frame(&ctx, d_key, 1);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    bt_app_ctx_free(&ctx);
}
