#include "common/bt_harness.h"

extern void bt_test_views_algs_algs_search_routes_the_selected_case_into_drill(void);
extern void bt_test_views_algs_arrows_keep_navigating_while_the_filter_box_is_focused(void);
extern void bt_test_views_algs_escape_releases_a_focused_text_input_before_leaving_the_view(void);

int main(void) {
    bt_test_entry tests[] = {
        {"views_algs.algs_search_routes_the_selected_case_into_drill", bt_test_views_algs_algs_search_routes_the_selected_case_into_drill},
        {"views_algs.arrows_keep_navigating_while_the_filter_box_is_focused", bt_test_views_algs_arrows_keep_navigating_while_the_filter_box_is_focused},
        {"views_algs.escape_releases_a_focused_text_input_before_leaving_the_view", bt_test_views_algs_escape_releases_a_focused_text_input_before_leaving_the_view},
    };
    return bt_harness_run("app_views_algs_test", tests, 3);
}
