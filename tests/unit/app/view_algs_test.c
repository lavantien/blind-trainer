#include "app/views/algs/mod.h"
#include "app/views/state.h"

#include "common.h"

static bt_corpus_row edge_row(const char *position, const char *alg, const char *commutator) {
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
    row.commutator = _strdup(commutator);
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static int span_is(const bt_view_algs_span *spans, size_t count, size_t at, const char *text,
                   bt_theme_color color) {
    if (at >= count) {
        return 0;
    }
    if (strcmp(spans[at].text, text) != 0) {
        return 0;
    }
    return spans[at].color.red == color.red && spans[at].color.green == color.green &&
           spans[at].color.blue == color.blue && spans[at].color.alpha == color.alpha;
}

BT_TEST(view_algs, all_cases_cover_the_full_pair_space) {
    bt_case cases[BT_ALGS_CASES_MAX];
    size_t count = bt_view_algs_all_cases(cases);
    BT_CHECK_EQ(count, (size_t)818);
    for (size_t i = 0; i < count; i++) {
        char letters[3];
        bt_view_algs_letters_of(&cases[i], letters);
        BT_CHECK(letters[0] != '\0' && letters[1] != '\0' && letters[2] == '\0');
    }
}

BT_TEST(view_algs, filter_matches_prefix_case_insensitively) {
    bt_case all[BT_ALGS_CASES_MAX];
    size_t count = bt_view_algs_all_cases(all);
    bt_case kept[BT_ALGS_CASES_MAX];
    size_t ab = bt_view_algs_filter_cases(all, count, "ab", kept);
    BT_CHECK_EQ(ab, (size_t)2);
    char letters[3];
    bt_view_algs_letters_of(&kept[0], letters);
    BT_CHECK(strcmp(letters, "AB") == 0);
    BT_CHECK_EQ(kept[0].orbit, BT_ORBIT_EDGE);
    BT_CHECK_EQ(kept[1].orbit, BT_ORBIT_CORNER);
    BT_CHECK_EQ(bt_view_algs_filter_cases(all, count, "", kept), (size_t)818);
    BT_CHECK_EQ(bt_view_algs_filter_cases(all, count, "z", kept), (size_t)0);
}

BT_TEST(view_algs, list_window_keeps_the_selection_centered_and_clamped) {
    bt_view_list_span window = bt_view_list_window(818, 0, 10);
    BT_CHECK_EQ(window.first, (size_t)0);
    BT_CHECK_EQ(window.count, (size_t)10);
    window = bt_view_list_window(818, 5, 10);
    BT_CHECK_EQ(window.first, (size_t)0);
    window = bt_view_list_window(818, 6, 10);
    BT_CHECK_EQ(window.first, (size_t)1);
    window = bt_view_list_window(818, 817, 10);
    BT_CHECK_EQ(window.first, (size_t)808);
    BT_CHECK_EQ(window.count, (size_t)10);
    window = bt_view_list_window(3, 2, 10);
    BT_CHECK_EQ(window.first, (size_t)0);
    BT_CHECK_EQ(window.count, (size_t)3);
}

BT_TEST(view_algs, list_capacity_fits_the_window_and_floors_at_one) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.height = 800;
    BT_CHECK_EQ(bt_view_algs_list_capacity(&config), (size_t)21);
    config.window.height = 0;
    BT_CHECK_EQ(bt_view_algs_list_capacity(&config), (size_t)1);
}

BT_TEST(view_algs, list_rows_mark_the_selected_row_with_a_caret) {
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_view_algs_app_init(app);
    app->browser.filtered_count = bt_view_algs_filter_cases(
        app->browser.all, app->browser.all_count, "", app->browser.filtered);
    app->browser.selected = 0;
    bt_view_case_row rows[6];
    size_t count = bt_view_algs_list_rows(&app->browser, 6, rows);
    BT_CHECK_EQ(count, (size_t)6);
    BT_CHECK(strcmp(rows[0].label, "> AB edge") == 0);
    BT_CHECK(rows[0].selected);
    BT_CHECK(rows[1].label[0] == ' ' && rows[1].label[1] == ' ');
    BT_CHECK(strstr(rows[1].label, " edge") != NULL);
    BT_CHECK(!rows[1].selected);
    app->browser.selected = 817;
    count = bt_view_algs_list_rows(&app->browser, 6, rows);
    BT_CHECK(count > 0);
    BT_CHECK(rows[count - 1].label[0] == '>' && rows[count - 1].selected);
}

BT_TEST(view_algs, list_rows_report_an_empty_match_set_once) {
    bt_view_algs_browser browser;
    memset(&browser, 0, sizeof browser);
    bt_view_case_row rows[10];
    size_t count = bt_view_algs_list_rows(&browser, 10, rows);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(rows[0].label, "no case matches the query") == 0);
    BT_CHECK(!rows[0].selected);
}

BT_TEST(view_algs, pair_expand_rows_carry_the_cancelled_flat_form) {
    bt_corpus_row cancelling =
        edge_row("UF-UB-UR", "U' R U R' U' U S U' U R U' R' U S'", "[U':[R,U],S]");
    char line[BT_ALGS_TEXT_MAX];
    BT_CHECK_EQ(bt_view_algs_pair_expand_rows(&cancelling, line), (size_t)1);
    BT_CHECK(strcmp(line, "expand U' R U R' S R U' R' U S'") == 0);

    bt_corpus_row aligned = edge_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]");
    BT_CHECK_EQ(bt_view_algs_pair_expand_rows(&aligned, line), (size_t)1);
    BT_CHECK(strcmp(line, "expand R2 U' S R2 S' R2 U R2") == 0);

    bt_corpus_row placeholder = edge_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "Not found.");
    BT_CHECK_EQ(bt_view_algs_pair_expand_rows(&placeholder, line), (size_t)0);

    bt_corpus_row junk = edge_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "junk text");
    BT_CHECK_EQ(bt_view_algs_pair_expand_rows(&junk, line), (size_t)0);
}

BT_TEST(view_algs, mechanic_spans_color_setup_insertion_and_interchange) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    if (!bt_alg_parse("R2 U' S R2 S' R2 U R2", &alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg: %s", err_token);
    }
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    BT_CHECK(bt_decompose_verified("R2 U':[S,R2]", &alg, &units));
    BT_CHECK_EQ(units.count, (size_t)1);
    bt_view_algs_span spans[BT_ALGS_SPANS_MAX];
    size_t count = bt_view_algs_mechanic_spans(&units, spans);
    BT_CHECK(span_is(spans, count, 0, "R2 U': ", bt_theme_setup()));
    BT_CHECK(span_is(spans, count, 1, "[", bt_theme_text_muted()));
    BT_CHECK(span_is(spans, count, 2, "S", bt_theme_teal()));
    BT_CHECK(span_is(spans, count, 3, ",", bt_theme_text_muted()));
    BT_CHECK(span_is(spans, count, 4, "R2", bt_theme_interchange()));
    BT_CHECK(span_is(spans, count, 5, "]", bt_theme_text_muted()));
    bt_expanded_list joined;
    bt_expanded_list_init(&joined);
    bt_decompose_verified("R2 U':[S,R2]", &alg, &joined);
    bt_expanded_list more;
    bt_expanded_list_init(&more);
    bt_decompose_verified("R2 U':[S,R2]", &alg, &more);
    bt_expanded_list_free(&more);
    bt_expanded_list_free(&joined);
    bt_expanded_list_free(&units);
    bt_alg_free(&alg);
}
