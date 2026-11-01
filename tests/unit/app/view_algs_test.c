#include "app/views/algs/mod.h"
#include "app/views/frame.h"
#include "app/views/state.h"

#include "common.h"

static int near_value(float value, float expected) {
    float diff = value - expected;
    if (diff < 0.0F) {
        diff = -diff;
    }
    return diff < 0.02F;
}

static int color_is(const bt_theme_color *color, bt_theme_color want) {
    return color->red == want.red && color->green == want.green && color->blue == want.blue &&
           color->alpha == want.alpha;
}

static bt_corpus_row edge_row_recon(const char *position, const char *alg, const char *commutator,
                                    const char *recon) {
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
    row.recon_link = _strdup(recon);
    row.youtube_link = _strdup("");
    return row;
}

static bt_corpus_row edge_row(const char *position, const char *alg, const char *commutator) {
    return edge_row_recon(position, alg, commutator, "");
}

static bt_corpus one_row_corpus(const bt_corpus_row *row) {
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, row, sizeof *heap);
    bt_corpus corpus;
    bt_corpus_from_rows(heap, 1, &corpus);
    return corpus;
}

static bt_view_algs_browser selected_ab_browser(void) {
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_view_algs_app_init(app);
    app->browser.filtered_count = bt_view_algs_filter_cases(
        app->browser.all, app->browser.all_count, "", app->browser.filtered);
    app->browser.selected = 0;
    return app->browser;
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
    const bt_theme *theme = bt_theme_table();
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.height = 800;
    float line = theme->root_rem * theme->font_body * 1.2F;
    float chrome = theme->root_rem * (theme->header_height_rem + 2.0F * theme->panel_padding_rem);
    float query_box = theme->root_rem * 3.0F;
    BT_CHECK_EQ(bt_view_algs_list_capacity(&config),
                (size_t)(((float)800 - chrome - query_box) / line));
    config.window.height = 0;
    BT_CHECK_EQ(bt_view_algs_list_capacity(&config), (size_t)1);
}

BT_TEST(view_algs, detail_fit_keeps_a_tail_line_when_the_panel_overflows) {
    float line = bt_theme_table()->root_rem * 1.2F;
    float tail = 0.9F * line;
    float sizes[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, 4.0F * line + 1.0F), (size_t)4);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, 4.0F * line + 0.05F), (size_t)4);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, 4.0F * line - 0.05F), (size_t)3);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, 3.0F * line + tail + 0.05F), (size_t)3);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, 3.0F * line + tail - 0.05F), (size_t)2);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, line + tail + 0.05F), (size_t)1);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 4, line - 0.05F), (size_t)0);
    BT_CHECK_EQ(bt_view_algs_detail_visible(sizes, 0, 4.0F * line), (size_t)0);
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

BT_TEST(view_algs, pair_expand_line_renders_at_the_alg_style) {
    bt_corpus_row row = edge_row("UF-UB-UR", "U' R U R' U' U S U' U R U' R' U S'", "[U':[R,U],S]");
    bt_corpus corpus = one_row_corpus(&row);
    bt_view_algs_browser browser = selected_ab_browser();
    bt_view_algs_detail_line *lines = NULL;
    size_t count = bt_view_algs_detail_lines(&corpus, &browser, &lines);
    int saw_expand = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(lines[i].text, "expand ", 7) == 0) {
            saw_expand = 1;
            BT_CHECK_EQ(lines[i].style, BT_ALGS_STYLE_ALG);
        }
    }
    BT_CHECK(saw_expand);
    free(lines);
}

BT_TEST(view_algs, recon_line_links_only_when_the_recon_url_is_openable) {
    bt_corpus_row openable = edge_row_recon("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]",
                                            "https://example.test/recon");
    bt_corpus corpus = one_row_corpus(&openable);
    bt_view_algs_browser browser = selected_ab_browser();
    bt_view_algs_detail_line *lines = NULL;
    size_t count = bt_view_algs_detail_lines(&corpus, &browser, &lines);
    size_t linked = 0;
    for (size_t i = 0; i < count; i++) {
        if (lines[i].link) {
            linked++;
            BT_CHECK_EQ(lines[i].style, BT_ALGS_STYLE_MICRO);
            BT_CHECK(color_is(&lines[i].color, bt_theme_accent()));
            BT_CHECK(strstr(lines[i].text, "recon https://example.test/recon") == lines[i].text);
        }
    }
    BT_CHECK_EQ(linked, (size_t)1);
    free(lines);

    bt_corpus_row blank = edge_row_recon("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]", "");
    bt_corpus blank_corpus = one_row_corpus(&blank);
    lines = NULL;
    count = bt_view_algs_detail_lines(&blank_corpus, &browser, &lines);
    for (size_t i = 0; i < count; i++) {
        BT_CHECK(!lines[i].link);
        if (strncmp(lines[i].text, "recon ", 6) == 0) {
            BT_CHECK_EQ(lines[i].style, BT_ALGS_STYLE_MICRO);
            BT_CHECK(color_is(&lines[i].color, bt_theme_text_muted()));
        }
    }
    free(lines);

    bt_corpus_row foreign =
        edge_row_recon("UF-UB-UR", "R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]", "file:///C:/Users/x");
    bt_corpus foreign_corpus = one_row_corpus(&foreign);
    lines = NULL;
    count = bt_view_algs_detail_lines(&foreign_corpus, &browser, &lines);
    for (size_t i = 0; i < count; i++) {
        BT_CHECK(!lines[i].link);
    }
    free(lines);
}

BT_TEST(view_algs, detail_column_pins_the_split_and_its_center) {
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    bt_view_algs_column column = bt_view_algs_detail_column(&metrics);
    BT_CHECK(near_value(column.x, metrics.content_x + metrics.content_w * 0.4F));
    BT_CHECK(near_value(column.width, metrics.content_w * 0.6F));
    BT_CHECK(near_value(column.center, column.x + column.width * 0.5F));
}

BT_TEST(view_algs, centered_line_x_and_link_rect_pin_the_hit_geometry) {
    BT_CHECK(near_value(bt_view_algs_centered_line_x(500.0F, 200.0F), 400.0F));
    BT_CHECK(near_value(bt_view_algs_centered_line_x(500.0F, 0.0F), 500.0F));
    bt_ui_rect hit = bt_view_algs_link_rect(500.0F, 100.0F, 200.0F, 44.0F);
    BT_CHECK(near_value(hit.x, 400.0F));
    BT_CHECK(near_value(hit.y, 100.0F));
    BT_CHECK(near_value(hit.width, 200.0F));
    BT_CHECK(near_value(hit.height, 44.0F));
}
