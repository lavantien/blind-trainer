#include "common/bt_harness.h"

extern void bt_test_view_algs_all_cases_cover_the_full_pair_space(void);
extern void bt_test_view_algs_filter_matches_prefix_case_insensitively(void);
extern void bt_test_view_algs_list_window_keeps_the_selection_centered_and_clamped(void);
extern void bt_test_view_algs_list_capacity_fits_the_window_and_floors_at_one(void);
extern void bt_test_view_algs_list_rows_mark_the_selected_row_with_a_caret(void);
extern void bt_test_view_algs_list_rows_report_an_empty_match_set_once(void);
extern void bt_test_view_algs_pair_expand_rows_carry_the_cancelled_flat_form(void);
extern void bt_test_view_algs_mechanic_spans_color_setup_insertion_and_interchange(void);

int main(void) {
    bt_test_entry tests[] = {
        {"view_algs.all_cases_cover_the_full_pair_space", bt_test_view_algs_all_cases_cover_the_full_pair_space},
        {"view_algs.filter_matches_prefix_case_insensitively", bt_test_view_algs_filter_matches_prefix_case_insensitively},
        {"view_algs.list_window_keeps_the_selection_centered_and_clamped", bt_test_view_algs_list_window_keeps_the_selection_centered_and_clamped},
        {"view_algs.list_capacity_fits_the_window_and_floors_at_one", bt_test_view_algs_list_capacity_fits_the_window_and_floors_at_one},
        {"view_algs.list_rows_mark_the_selected_row_with_a_caret", bt_test_view_algs_list_rows_mark_the_selected_row_with_a_caret},
        {"view_algs.list_rows_report_an_empty_match_set_once", bt_test_view_algs_list_rows_report_an_empty_match_set_once},
        {"view_algs.pair_expand_rows_carry_the_cancelled_flat_form", bt_test_view_algs_pair_expand_rows_carry_the_cancelled_flat_form},
        {"view_algs.mechanic_spans_color_setup_insertion_and_interchange", bt_test_view_algs_mechanic_spans_color_setup_insertion_and_interchange},
    };
    return bt_harness_run("app_view_algs_test", tests, 8);
}
