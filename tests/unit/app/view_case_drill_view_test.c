#include "app/net.h"
#include "app/views/algs/display.h"
#include "app/views/case_drill.h"
#include "app/views/case_drill_view.h"
#include "app/views/state.h"

#include "common.h"

#include <math.h>

static int close_to(float value, float wanted, float tolerance) {
    float diff = value - wanted;
    if (diff < 0.0F) {
        diff = -diff;
    }
    return diff < tolerance;
}

BT_TEST(view_case_drill_view, layout_cells_counts_columns_and_rows_of_any_net) {
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    bt_netdisp_layout layout;
    size_t cols = 0;
    size_t rows = 0;

    BT_CHECK(bt_case_drill_facelets("3x3", 0, 0, "R", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);
    bt_case_view_layout_cells(&layout, &cols, &rows);
    BT_CHECK_EQ(cols, (size_t)12);
    BT_CHECK_EQ(rows, (size_t)9);

    BT_CHECK(bt_case_drill_facelets("skewb", 0, 0, "R", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);
    bt_case_view_layout_cells(&layout, &cols, &rows);
    BT_CHECK_EQ(cols, (size_t)12);
    BT_CHECK_EQ(rows, (size_t)9);

    BT_CHECK(bt_case_drill_facelets("megaminx", 0, 0, "R", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);
    bt_case_view_layout_cells(&layout, &cols, &rows);
    BT_CHECK_EQ(cols, (size_t)12);
    BT_CHECK_EQ(rows, (size_t)15);

    BT_CHECK(bt_case_drill_facelets("sq1", 0, 0, "(1,0)/", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);
    bt_case_view_layout_cells(&layout, &cols, &rows);
    BT_CHECK_EQ(cols, (size_t)12);
    BT_CHECK_EQ(rows, (size_t)2);
}

BT_TEST(view_case_drill_view, quad_for_fits_the_region_and_clamps) {
    BT_CHECK(close_to(bt_case_view_quad_for(12, 9, 576.0F, 576.0F), 43.0F, 1.0F));
    BT_CHECK_EQ(bt_case_view_quad_for(12, 9, 48.0F, 48.0F), BT_CASE_VIEW_QUAD_MIN);
    BT_CHECK_EQ(bt_case_view_quad_for(2, 2, 4000.0F, 4000.0F), BT_CASE_VIEW_QUAD_MAX);
    BT_CHECK(bt_case_view_quad_for(12, 9, 576.0F, 576.0F) <= 576.0F / 12.0F);
}

BT_TEST(view_case_drill_view, panel_lines_spell_the_case_site_and_progress) {
    bt_case_drill_session session;
    memset(&session, 0, sizeof session);
    session.has_current = 1;
    bt_case_ref_of(&session.current.case_ref, "2x2", "cll", "cll", "cll.1");
    snprintf(session.current.alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    session.total = 2;
    session.done = 1;
    bt_case_view_line lines[BT_CASE_VIEW_LINES_MAX];

    size_t count = bt_case_view_lines(&session, 0, lines);
    BT_CHECK(strcmp(lines[0].text, "cll.1") == 0);
    BT_CHECK(strcmp(lines[1].text, "2x2 > cll > cll") == 0);
    BT_CHECK(strcmp(lines[2].text, "case 2 of 2") == 0);
    BT_CHECK(strcmp(lines[3].text, "space to reveal") == 0);
    BT_CHECK(!lines[3].danger);

    count = bt_case_view_lines(&session, 1, lines);
    BT_CHECK(strcmp(lines[3].text, "R U R'") == 0);
    BT_CHECK(strcmp(lines[4].text, "1 again 2 hard 3 good 4 easy") == 0);

    session.has_save_error = 1;
    snprintf(session.save_error, sizeof session.save_error, "%s", "db locked");
    count = bt_case_view_lines(&session, 1, lines);
    BT_CHECK(strcmp(lines[5].text, "db locked") == 0);
    BT_CHECK(lines[5].danger);

    session.has_save_error = 0;
    session.has_grid_error = 1;
    snprintf(session.grid_error, sizeof session.grid_error, "%s", "no case geometry for event zzz");
    count = bt_case_view_lines(&session, 1, lines);
    BT_CHECK(count >= 6);
    BT_CHECK(strcmp(lines[5].text, "no case geometry for event zzz") == 0);
    BT_CHECK(lines[5].danger);

    session.has_current = 0;
    count = bt_case_view_lines(&session, 0, lines);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(lines[0].text, "no case") == 0);
}

BT_TEST(view_case_drill_view, panel_lines_flag_the_alg_and_errors_for_fitted_drawing) {
    bt_case_drill_session session;
    memset(&session, 0, sizeof session);
    session.has_current = 1;
    bt_case_ref_of(&session.current.case_ref, "2x2", "cll", "cll", "cll.1");
    snprintf(session.current.alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R' U' R' F R2 U' R' U'");
    bt_case_view_line lines[BT_CASE_VIEW_LINES_MAX];

    size_t count = bt_case_view_lines(&session, 0, lines);
    for (size_t i = 0; i < count; i++) {
        BT_CHECK(!lines[i].fitted);
    }

    count = bt_case_view_lines(&session, 1, lines);
    BT_CHECK(!lines[0].fitted);
    BT_CHECK(!lines[1].fitted);
    BT_CHECK(!lines[2].fitted);
    BT_CHECK(lines[3].fitted);
    BT_CHECK(strcmp(lines[3].text, "R U R' U' R' F R2 U' R' U'") == 0);
    BT_CHECK(!lines[4].fitted);

    session.has_save_error = 1;
    snprintf(session.save_error, sizeof session.save_error, "%s", "db locked");
    session.has_grid_error = 1;
    snprintf(session.grid_error, sizeof session.grid_error, "%s", "no case geometry for event zzz");
    count = bt_case_view_lines(&session, 1, lines);
    BT_CHECK(lines[count - 2].fitted);
    BT_CHECK(lines[count - 2].danger);
    BT_CHECK(lines[count - 1].fitted);
    BT_CHECK(lines[count - 1].danger);
}

BT_TEST(view_case_drill_view, text_band_height_sums_the_drawn_line_advances) {
    const bt_theme *theme = bt_theme_table();
    float title = theme->font_title * theme->root_rem;
    float body = theme->font_small * theme->root_rem;
    float spacing = theme->spacing_rem * theme->root_rem;
    BT_CHECK_EQ(bt_case_view_text_band_height(theme, 0), 0.0F);
    BT_CHECK_EQ(bt_case_view_text_band_height(theme, 1), title + spacing);
    BT_CHECK_F64_NEAR(bt_case_view_text_band_height(theme, 6),
                      title + spacing + 5.0F * (body + spacing * 0.5F), 1e-4);
    BT_CHECK(bt_case_view_text_band_height(theme, 7) > bt_case_view_text_band_height(theme, 6));
}

BT_TEST(view_case_drill_view, net_extent_sums_plates_borders_and_gaps) {
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    bt_netdisp_layout layout;
    BT_CHECK(bt_case_drill_facelets("3x3", 0, 0, "R", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);

    float quad = 43.0F;
    float gap = fmaxf(quad * BT_CASE_VIEW_CELL_GAP_RATIO, 1.5F);
    float stroke = bt_net_cell_stroke(quad);
    float w = 0.0F;
    float h = 0.0F;
    bt_case_view_net_extent(&layout, quad, &w, &h);
    BT_CHECK_F64_NEAR(w, 12.0F * quad + 11.0F * gap + 8.0F * stroke, 1e-3);
    BT_CHECK_F64_NEAR(h, 9.0F * quad + 8.0F * gap + 6.0F * stroke, 1e-3);

    bt_case_view_net_extent(&layout, quad + 10.0F, &w, &h);
    gap = fmaxf((quad + 10.0F) * BT_CASE_VIEW_CELL_GAP_RATIO, 1.5F);
    stroke = bt_net_cell_stroke(quad + 10.0F);
    BT_CHECK_F64_NEAR(w, 12.0F * (quad + 10.0F) + 11.0F * gap + 8.0F * stroke, 1e-3);
    BT_CHECK_F64_NEAR(h, 9.0F * (quad + 10.0F) + 8.0F * gap + 6.0F * stroke, 1e-3);
}

BT_TEST(view_case_drill_view, net_place_fits_the_net_inside_the_region_and_centers_it) {
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    bt_netdisp_layout layout;
    BT_CHECK(bt_case_drill_facelets("3x3", 0, 0, "R", &grid, error, sizeof error));
    bt_netdisp_layout_of(&grid, &layout);

    float x = 0.0F;
    float y = 0.0F;
    float quad = 0.0F;
    bt_case_view_net_place(&layout, 100.0F, 300.0F, 560.0F, 380.0F, &x, &y, &quad);
    BT_CHECK_EQ(quad, 36.0F);
    float w = 0.0F;
    float h = 0.0F;
    bt_case_view_net_extent(&layout, quad, &w, &h);
    BT_CHECK(w <= 560.0F);
    BT_CHECK(h <= 380.0F);
    BT_CHECK_F64_NEAR(x - 100.0F, (560.0F - w) / 2.0F, 1e-3);
    BT_CHECK_F64_NEAR(y - 300.0F, (380.0F - h) / 2.0F, 1e-3);

    bt_case_view_net_place(&layout, 0.0F, 0.0F, 4000.0F, 4000.0F, &x, &y, &quad);
    BT_CHECK_EQ(quad, BT_CASE_VIEW_QUAD_MAX);
    bt_case_view_net_extent(&layout, quad, &w, &h);
    BT_CHECK_F64_NEAR(x, (4000.0F - w) / 2.0F, 1e-3);
    BT_CHECK_F64_NEAR(y, (4000.0F - h) / 2.0F, 1e-3);
}
