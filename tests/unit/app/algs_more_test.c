#include "app/boot.h"
#include "app/views/algs/mod.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/state.h"
#include "data/db.h"
#include "data/paths.h"

#include "common.h"

enum {
    KEY_SLASH = 47,
    KEY_O = 79,
    KEY_X = 88,
    KEY_ENTER = 257,
    KEY_TAB = 258,
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
    bt_view_algs_key(&ctx, KEY_SLASH);
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

BT_TEST(algs_more, the_detail_shows_coverage_mechanics_meta_and_links) {
    bt_app_ctx ctx = algs_ctx("moredetail");
    bt_corpus_row row = covered_row(COVERED_ALG, "R2 U':[S,R2]");
    load_rows(&ctx, &row, 1);
    query_algs(&ctx, "ab");
    bt_view_algs_detail_line *lines = NULL;
    size_t count =
        bt_view_algs_detail_lines(&ctx.vendor_corpus, &bt_view_algs_app()->browser, &lines);
    char meta[BT_ALGS_TEXT_MAX];
    bt_view_algs_meta_line(&ctx.vendor_corpus.rows[0], meta);
    BT_CHECK(detail_has(lines, count, "2 of 818 cases match"));
    BT_CHECK(detail_has(lines, count, "AB"));
    BT_CHECK(detail_has(lines, count, "edge UF-UB-UR covered yes"));
    BT_CHECK(detail_has(lines, count, meta));
    BT_CHECK(detail_has(lines, count, "recon  video https://example.test/recon"));
    size_t mechanic = count;
    for (size_t i = 0; i < count; i++) {
        if (lines[i].span_count > 0) {
            mechanic = i;
        }
    }
    BT_CHECK(mechanic < count);
    BT_CHECK_EQ(lines[mechanic].span_count, (size_t)6);
    BT_CHECK(strcmp(lines[mechanic].spans[0].text, "R2 U': ") == 0);
    BT_CHECK(colors_equal(lines[mechanic].spans[0].color, bt_theme_setup()));
    BT_CHECK(strcmp(lines[mechanic].spans[1].text, "[") == 0);
    BT_CHECK(colors_equal(lines[mechanic].spans[1].color, bt_theme_text_muted()));
    BT_CHECK(strcmp(lines[mechanic].spans[2].text, "S") == 0);
    BT_CHECK(colors_equal(lines[mechanic].spans[2].color, bt_theme_teal()));
    BT_CHECK(strcmp(lines[mechanic].spans[4].text, "R2") == 0);
    BT_CHECK(colors_equal(lines[mechanic].spans[4].color, bt_theme_interchange()));
    BT_CHECK(strcmp(lines[mechanic].spans[5].text, "]") == 0);
    BT_CHECK(colors_equal(lines[mechanic].spans[5].color, bt_theme_text_muted()));
    free(lines);
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
    bt_view_algs_detail_line *lines = NULL;
    size_t count =
        bt_view_algs_detail_lines(&ctx.vendor_corpus, &bt_view_algs_app()->browser, &lines);
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
    free(lines);
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

    bt_view_algs_key(&ctx, KEY_SLASH);
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
    bt_view_algs_detail_line *lines = NULL;
    size_t detail_count = bt_view_algs_detail_lines(&ctx.vendor_corpus, &app->browser, &lines);
    BT_CHECK(detail_has(lines, detail_count, "0 of 818 cases match"));
    BT_CHECK(detail_has(lines, detail_count, "no case matches the query"));
    free(lines);
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

static bt_repo_browser *install_cll_repo(bt_app_ctx *ctx) {
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
    bt_view_algs_enter(ctx);
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_repo_browser_free(repo);
    repo->arms = bt_repo_arms(packs, 1, &hidden, rows, 1, &repo->arm_count);
    bt_str_list_free(&hidden);
    return repo;
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
    bt_repo_browser *repo = install_cll_repo(&ctx);

    repo_walk_to_case(&ctx);
    BT_CHECK(repo->repo_focused);
    BT_CHECK(strcmp(bt_repo_browser_node(repo)->label, "cll.1") == 0);

    int open_key[1] = {KEY_O};
    bt_view_algs_frame(&ctx, open_key, 1);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    BT_CHECK(strcmp(draft->site.case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(draft->text, "R U R'") == 0);

    int cancel_key[1] = {KEY_TAB};
    bt_view_algs_frame(&ctx, cancel_key, 1);
    BT_CHECK(!draft->has_case);

    bt_view_algs_frame(&ctx, open_key, 1);
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "F R F'");
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(bt_overrides_contains(bt_view_algs_overrides(), &key));
    BT_CHECK(!draft->has_case);

    int delete_key[1] = {KEY_X};
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

BT_TEST(algs_more, reentering_the_algs_view_resets_the_query_focus_for_the_d_gate) {
    bt_app_ctx ctx = algs_ctx("reenter");
    install_cll_repo(&ctx);
    int slash_key[1] = {KEY_SLASH};
    bt_view_algs_frame(&ctx, slash_key, 1);
    BT_CHECK(bt_view_algs_app()->query_focused);
    bt_app_request_state(&ctx, BT_APP_MENU);
    bt_app_transition_frame(&ctx);
    bt_app_request_state(&ctx, BT_APP_ALGS);
    bt_app_transition_frame(&ctx);
    BT_CHECK(!bt_view_algs_app()->query_focused);
    install_cll_repo(&ctx);
    repo_walk_to_case(&ctx);
    int d_key[1] = {KEY_D};
    bt_view_algs_frame(&ctx, d_key, 1);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, case_rows_report_every_vendor_row) {
    bt_app_ctx ctx = algs_ctx("manyrows");
    enum { ROWS = 70 };
    bt_corpus_row rows[ROWS];
    for (int i = 0; i < ROWS; i++) {
        rows[i] = covered_row(COVERED_ALG, "");
    }
    bt_corpus_row *heap = malloc(ROWS * sizeof *heap);
    memcpy(heap, rows, ROWS * sizeof *heap);
    bt_corpus_from_rows(heap, ROWS, &ctx.vendor_corpus);
    ctx.has_corpus = 1;
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'A');
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, 'B');
    const bt_corpus_row **found = NULL;
    size_t count = bt_view_algs_case_rows_dyn(&ctx.vendor_corpus, &case_id, &found);
    BT_CHECK_EQ(count, (size_t)ROWS);
    free(found);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, detail_line_styles_map_to_font_and_color) {
    float size = 0.0F;
    bt_theme_color color;
    bt_view_algs_style_metrics(BT_ALGS_STYLE_FOCAL, &size, &color);
    BT_CHECK(size == bt_theme_table()->font_title);
    bt_view_algs_style_metrics(BT_ALGS_STYLE_MUTED, &size, &color);
    BT_CHECK(size == bt_theme_table()->font_body);
    bt_view_algs_style_metrics(BT_ALGS_STYLE_ALG, &size, &color);
    BT_CHECK(size == bt_theme_table()->font_alg);
    bt_view_algs_style_metrics(BT_ALGS_STYLE_SMALL, &size, &color);
    BT_CHECK(size == bt_theme_table()->font_small);
    bt_view_algs_style_metrics(BT_ALGS_STYLE_MICRO, &size, &color);
    BT_CHECK(size == bt_theme_table()->font_micro);
}

BT_TEST(algs_more, unit_spans_render_the_b_first_pair_flipped) {
    bt_expanded unit;
    memset(&unit, 0, sizeof unit);
    char err_token[64];
    BT_CHECK(bt_alg_parse("R", &unit.a, err_token, sizeof err_token));
    BT_CHECK(bt_alg_parse("U", &unit.b, err_token, sizeof err_token));
    unit.b_first = true;
    bt_view_algs_span spans[BT_ALGS_SPANS_MAX];
    size_t count = bt_view_algs_unit_spans(&unit, spans);
    BT_CHECK(count >= 4);
    int at_u = -1;
    int at_r = -1;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(spans[i].text, "U") == 0) {
            at_u = (int)i;
        }
        if (strcmp(spans[i].text, "R") == 0) {
            at_r = (int)i;
        }
    }
    BT_CHECK(at_u >= 0);
    BT_CHECK(at_r >= 0);
    BT_CHECK(at_u < at_r);
}

BT_TEST(algs_more, detail_lines_report_a_commutator_that_does_not_verify) {
    bt_app_ctx ctx = algs_ctx("badcomm");
    bt_corpus_row row = covered_row(COVERED_ALG, "not a commutator");
    load_rows(&ctx, &row, 1);
    query_algs(&ctx, "ab");
    bt_view_algs_detail_line *lines = NULL;
    size_t count =
        bt_view_algs_detail_lines(&ctx.vendor_corpus, &bt_view_algs_app()->browser, &lines);
    BT_CHECK(count > 0);
    char joined[BT_ALGS_TEXT_MAX * 8];
    joined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        strncat(joined, lines[i].text, sizeof joined - strlen(joined) - 1);
        strncat(joined, "\n", sizeof joined - strlen(joined) - 1);
    }
    BT_CHECK(strstr(joined, "commutator not a commutator") != NULL);
    free(lines);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, update_types_frame_chars_into_the_override_draft) {
    bt_app_ctx ctx = algs_ctx("draftchars");
    bt_view_algs_enter(&ctx);
    bt_view_algs_app_state *app = bt_view_algs_app();
    app->query_focused = 0;
    ctx.input_focus = 0;
    bt_view_override_draft *draft = bt_view_algs_draft();
    draft->has_case = 1;
    draft->text[0] = '\0';
    bt_ui_input input;
    bt_ui_input_clear(&input);
    input.chars[0] = 'x';
    input.chars[1] = 'y';
    input.char_count = 2;
    bt_app_frame_input_set(&input);
    bt_view_algs_update(&ctx);
    bt_app_frame_input_set(NULL);
    BT_CHECK(strcmp(draft->text, "xy") == 0);
    bt_app_ctx_free(&ctx);
}

static void blocked_user_root(const char *tag, char out[BT_PATH_MAX]) {
    const char *dir = ta_temp_dir("algs_more", tag);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s\\blocker", dir);
    ta_write_file(blocker, "x");
    snprintf(out, BT_PATH_MAX, "%s\\blocker\\under", dir);
}

BT_TEST(algs_more, override_open_without_a_case_site_returns_early) {
    bt_app_ctx ctx = algs_ctx("openearly");
    install_cll_repo(&ctx);
    bt_view_override_cancel();
    bt_view_override_open(&ctx);
    BT_CHECK(!bt_view_algs_draft()->has_case);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_open_prefers_the_stored_override_text) {
    bt_app_ctx ctx = algs_ctx("openstored");
    install_cll_repo(&ctx);
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    bt_overrides_put(bt_view_algs_overrides(), &key, "L U L'");
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    BT_CHECK(strcmp(bt_view_algs_draft()->text, "L U L'") == 0);
    bt_app_ctx_free(&ctx);
}

static void install_caseless_alg_repo(bt_app_ctx *ctx) {
    bt_view_algs_enter(ctx);
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_repo_browser_free(repo);
    bt_repo_node *arms = malloc(2 * sizeof *arms);
    bt_repo_node *method = malloc(sizeof *method);
    bt_repo_node *stage = malloc(sizeof *stage);
    bt_repo_node *leaf = malloc(sizeof *leaf);
    if (arms == NULL || method == NULL || stage == NULL || leaf == NULL) {
        BT_ABORT("fixture nodes out of memory");
    }
    memset(arms, 0, 2 * sizeof *arms);
    snprintf(arms[0].label, sizeof arms[0].label, "%s", "letters");
    arms[0].kind = BT_REPO_LETTERS;
    snprintf(arms[1].label, sizeof arms[1].label, "%s", "2x2");
    arms[1].kind = BT_REPO_EVENT;
    memset(method, 0, sizeof *method);
    snprintf(method->label, sizeof method->label, "%s", "cll");
    method->kind = BT_REPO_METHOD;
    memset(stage, 0, sizeof *stage);
    snprintf(stage->label, sizeof stage->label, "%s", "cll");
    stage->kind = BT_REPO_STAGE;
    memset(leaf, 0, sizeof *leaf);
    snprintf(leaf->label, sizeof leaf->label, "%s", "cll.1");
    leaf->kind = BT_REPO_CASE;
    leaf->has_case = 1;
    stage->children = leaf;
    stage->children_count = 1;
    method->children = stage;
    method->children_count = 1;
    arms[1].children = method;
    arms[1].children_count = 1;
    repo->arms = arms;
    repo->arm_count = 2;
    repo->arm = 0;
    repo->depth = 0;
    repo->repo_focused = 0;
}

BT_TEST(algs_more, override_open_falls_back_to_an_empty_seed_when_the_row_alg_was_lost) {
    bt_app_ctx ctx = algs_ctx("openseedlost");
    install_caseless_alg_repo(&ctx);
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_repo_deeper(repo);
    bt_repo_move(repo, 1);
    bt_repo_deeper(repo);
    bt_repo_deeper(repo);
    bt_repo_deeper(repo);
    const bt_repo_node *node = bt_repo_browser_node(repo);
    BT_CHECK(node != NULL);
    BT_CHECK_EQ(node->kind, BT_REPO_CASE);
    BT_CHECK(node->case_alg == NULL);
    bt_view_override_open(&ctx);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    BT_CHECK(draft->text[0] == '\0');
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_submit_without_an_open_draft_returns_early) {
    bt_app_ctx ctx = algs_ctx("submitearly");
    install_cll_repo(&ctx);
    bt_view_override_cancel();
    bt_view_override_submit(&ctx, "F R F'");
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_submit_trims_outer_space_and_a_blank_submit_cancels) {
    bt_app_ctx ctx = algs_ctx("subtrim");
    install_cll_repo(&ctx);
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "  F R F'  ");
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    const char *stored = bt_overrides_get(bt_view_algs_overrides(), &key);
    BT_CHECK(stored != NULL);
    BT_CHECK(strcmp(stored, "F R F'") == 0);
    bt_view_override_open(&ctx);
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "   ");
    BT_CHECK(!draft->has_case);
    stored = bt_overrides_get(bt_view_algs_overrides(), &key);
    BT_CHECK(stored != NULL);
    BT_CHECK(strcmp(stored, "F R F'") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_submit_rejects_text_that_fails_the_geometry_gate) {
    bt_app_ctx ctx = algs_ctx("submitbadalg");
    install_cll_repo(&ctx);
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    bt_view_override_draft *draft = bt_view_algs_draft();
    bt_view_override_submit(&ctx, "QQ");
    BT_CHECK(draft->has_case);
    BT_CHECK(draft->has_error);
    BT_CHECK(draft->error[0] != '\0');
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_submit_reports_a_db_open_failure) {
    bt_app_ctx ctx = algs_ctx("submitdbfail");
    char gone[BT_PATH_MAX];
    blocked_user_root("submitdbfail-user", gone);
    snprintf(ctx.paths.user_root, sizeof ctx.paths.user_root, "%s", gone);
    install_cll_repo(&ctx);
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "F R F'");
    BT_CHECK(draft->has_case);
    BT_CHECK(draft->has_error);
    BT_CHECK(draft->error[0] != '\0');
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_delete_without_a_site_or_a_stored_override_returns_early) {
    bt_app_ctx ctx = algs_ctx("delearly");
    install_cll_repo(&ctx);
    bt_view_override_delete(&ctx);
    repo_walk_to_case(&ctx);
    bt_view_override_delete(&ctx);
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_delete_reports_a_db_open_failure) {
    bt_app_ctx ctx = algs_ctx("deldbfail");
    install_cll_repo(&ctx);
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    bt_view_override_submit(&ctx, "F R F'");
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(bt_overrides_contains(bt_view_algs_overrides(), &key));
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx.paths.user_root, db_path);
    BT_CHECK_EQ(remove(db_path), 0);
    BT_CHECK_EQ(_mkdir(db_path), 0);
    bt_view_override_delete(&ctx);
    BT_CHECK(bt_overrides_contains(bt_view_algs_overrides(), &key));
    BT_CHECK(bt_view_algs_draft()->has_error);
    BT_CHECK_EQ(_rmdir(db_path), 0);
    bt_app_ctx_free(&ctx);
}

static void drop_user_alg_table(const char *user_root) {
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(user_root, db_path);
    bt_db db;
    bt_db_error err;
    BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
    BT_CHECK_EQ(bt_db_exec(&db, "DROP TABLE user_alg", "drop user_alg", &err), 0);
    bt_db_close(&db);
}

BT_TEST(algs_more, override_submit_reports_an_upsert_failure_on_a_broken_schema) {
    bt_app_ctx ctx = algs_ctx("submitdrop");
    install_cll_repo(&ctx);
    drop_user_alg_table(ctx.paths.user_root);
    repo_walk_to_case(&ctx);
    bt_view_override_open(&ctx);
    bt_view_override_draft *draft = bt_view_algs_draft();
    BT_CHECK(draft->has_case);
    bt_view_override_submit(&ctx, "F R F'");
    BT_CHECK(draft->has_case);
    BT_CHECK(draft->has_error);
    BT_CHECK(draft->error[0] != '\0');
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    BT_CHECK(!bt_overrides_contains(bt_view_algs_overrides(), &key));
    bt_app_ctx_free(&ctx);
}

BT_TEST(algs_more, override_delete_reports_a_remove_failure_on_a_broken_schema) {
    bt_app_ctx ctx = algs_ctx("deldrop");
    install_cll_repo(&ctx);
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    bt_overrides_put(bt_view_algs_overrides(), &key, "L U L'");
    drop_user_alg_table(ctx.paths.user_root);
    repo_walk_to_case(&ctx);
    bt_view_override_delete(&ctx);
    BT_CHECK(bt_overrides_contains(bt_view_algs_overrides(), &key));
    BT_CHECK(bt_view_algs_draft()->has_error);
    bt_app_ctx_free(&ctx);
}
