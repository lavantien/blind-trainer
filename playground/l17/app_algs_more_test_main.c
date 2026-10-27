#include "common/bt_harness.h"

extern void bt_test_algs_more_slash_focuses_the_filter_and_plain_keys_never_grab_it(void);
extern void bt_test_algs_more_typed_prefix_flows_from_the_input_into_the_filter(void);
extern void bt_test_algs_more_the_list_bind_renders_caret_rows_and_blanks_the_overflow(void);
extern void bt_test_algs_more_the_detail_shows_coverage_mechanics_meta_and_links(void);
extern void bt_test_algs_more_extra_vendor_rows_render_as_alternatives_with_their_own_meta(void);
extern void bt_test_algs_more_movement_keys_step_wrap_and_respect_the_typing_guard(void);
extern void bt_test_algs_more_movement_wraps_inside_a_small_match_set(void);
extern void bt_test_algs_more_enter_sends_the_selected_case_to_the_drill(void);
extern void bt_test_algs_more_a_shrunk_match_set_clamps_the_selection_and_blocks_the_handoff(void);
extern void bt_test_algs_more_meta_line_badges_verification_and_problem_counts(void);

int main(void) {
    bt_test_entry tests[] = {
        {"algs_more.slash_focuses_the_filter_and_plain_keys_never_grab_it", bt_test_algs_more_slash_focuses_the_filter_and_plain_keys_never_grab_it},
        {"algs_more.typed_prefix_flows_from_the_input_into_the_filter", bt_test_algs_more_typed_prefix_flows_from_the_input_into_the_filter},
        {"algs_more.the_list_bind_renders_caret_rows_and_blanks_the_overflow", bt_test_algs_more_the_list_bind_renders_caret_rows_and_blanks_the_overflow},
        {"algs_more.the_detail_shows_coverage_mechanics_meta_and_links", bt_test_algs_more_the_detail_shows_coverage_mechanics_meta_and_links},
        {"algs_more.extra_vendor_rows_render_as_alternatives_with_their_own_meta", bt_test_algs_more_extra_vendor_rows_render_as_alternatives_with_their_own_meta},
        {"algs_more.movement_keys_step_wrap_and_respect_the_typing_guard", bt_test_algs_more_movement_keys_step_wrap_and_respect_the_typing_guard},
        {"algs_more.movement_wraps_inside_a_small_match_set", bt_test_algs_more_movement_wraps_inside_a_small_match_set},
        {"algs_more.enter_sends_the_selected_case_to_the_drill", bt_test_algs_more_enter_sends_the_selected_case_to_the_drill},
        {"algs_more.a_shrunk_match_set_clamps_the_selection_and_blocks_the_handoff", bt_test_algs_more_a_shrunk_match_set_clamps_the_selection_and_blocks_the_handoff},
        {"algs_more.meta_line_badges_verification_and_problem_counts", bt_test_algs_more_meta_line_badges_verification_and_problem_counts},
    };
    return bt_harness_run("app_algs_more_test", tests, 10);
}
