#include "common/bt_harness.h"

extern void bt_test_view_case_drill_view_layout_cells_counts_columns_and_rows_of_any_net(void);
extern void bt_test_view_case_drill_view_quad_for_fits_the_region_and_clamps(void);
extern void bt_test_view_case_drill_view_panel_lines_spell_the_case_site_and_progress(void);

int main(void) {
    bt_test_entry tests[] = {
        {"view_case_drill_view.layout_cells_counts_columns_and_rows_of_any_net", bt_test_view_case_drill_view_layout_cells_counts_columns_and_rows_of_any_net},
        {"view_case_drill_view.quad_for_fits_the_region_and_clamps", bt_test_view_case_drill_view_quad_for_fits_the_region_and_clamps},
        {"view_case_drill_view.panel_lines_spell_the_case_site_and_progress", bt_test_view_case_drill_view_panel_lines_spell_the_case_site_and_progress},
    };
    return bt_harness_run("app_view_case_drill_view_test", tests, 3);
}
