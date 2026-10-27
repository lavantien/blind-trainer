#include "common/bt_harness.h"

extern void bt_test_view_case_drill_face_permutations_follow_the_ledger_orders(void);
extern void bt_test_view_case_drill_color_remaps_send_each_face_name_to_its_palette_slot(void);
extern void bt_test_view_case_drill_themed_grid_identity_colors_a_solved_skewb(void);
extern void bt_test_view_case_drill_themed_grid_keeps_sticker_payload_under_remap(void);
extern void bt_test_view_case_drill_every_skewb_sarah_seed_row_renders_its_case_net(void);
extern void bt_test_view_case_drill_a_skewb_case_of_only_rotations_reports_the_stripped_alg_as_empty(void);
extern void bt_test_view_case_drill_case_facelets_applies_the_inverse_alg_to_solved(void);
extern void bt_test_view_case_drill_case_facelets_covers_every_pack_event_and_rejects_the_rest(void);
extern void bt_test_view_case_drill_collect_cases_walks_the_subtree_with_the_site_reference(void);
extern void bt_test_view_case_drill_collect_cases_uses_the_override_as_the_effective_alg(void);
extern void bt_test_view_case_drill_collect_cases_ignores_the_letters_and_3bld_arms(void);
extern void bt_test_view_case_drill_due_session_prioritizes_missing_and_due_cards(void);
extern void bt_test_view_case_drill_drill_request_drills_one_case_and_due_sessions_below_it(void);
extern void bt_test_view_case_drill_install_and_advance_run_the_session_queue(void);
extern void bt_test_view_case_drill_case_site_line_spells_the_site_and_size(void);

int main(void) {
    bt_test_entry tests[] = {
        {"view_case_drill.face_permutations_follow_the_ledger_orders", bt_test_view_case_drill_face_permutations_follow_the_ledger_orders},
        {"view_case_drill.color_remaps_send_each_face_name_to_its_palette_slot", bt_test_view_case_drill_color_remaps_send_each_face_name_to_its_palette_slot},
        {"view_case_drill.themed_grid_identity_colors_a_solved_skewb", bt_test_view_case_drill_themed_grid_identity_colors_a_solved_skewb},
        {"view_case_drill.themed_grid_keeps_sticker_payload_under_remap", bt_test_view_case_drill_themed_grid_keeps_sticker_payload_under_remap},
        {"view_case_drill.every_skewb_sarah_seed_row_renders_its_case_net", bt_test_view_case_drill_every_skewb_sarah_seed_row_renders_its_case_net},
        {"view_case_drill.a_skewb_case_of_only_rotations_reports_the_stripped_alg_as_empty", bt_test_view_case_drill_a_skewb_case_of_only_rotations_reports_the_stripped_alg_as_empty},
        {"view_case_drill.case_facelets_applies_the_inverse_alg_to_solved", bt_test_view_case_drill_case_facelets_applies_the_inverse_alg_to_solved},
        {"view_case_drill.case_facelets_covers_every_pack_event_and_rejects_the_rest", bt_test_view_case_drill_case_facelets_covers_every_pack_event_and_rejects_the_rest},
        {"view_case_drill.collect_cases_walks_the_subtree_with_the_site_reference", bt_test_view_case_drill_collect_cases_walks_the_subtree_with_the_site_reference},
        {"view_case_drill.collect_cases_uses_the_override_as_the_effective_alg", bt_test_view_case_drill_collect_cases_uses_the_override_as_the_effective_alg},
        {"view_case_drill.collect_cases_ignores_the_letters_and_3bld_arms", bt_test_view_case_drill_collect_cases_ignores_the_letters_and_3bld_arms},
        {"view_case_drill.due_session_prioritizes_missing_and_due_cards", bt_test_view_case_drill_due_session_prioritizes_missing_and_due_cards},
        {"view_case_drill.drill_request_drills_one_case_and_due_sessions_below_it", bt_test_view_case_drill_drill_request_drills_one_case_and_due_sessions_below_it},
        {"view_case_drill.install_and_advance_run_the_session_queue", bt_test_view_case_drill_install_and_advance_run_the_session_queue},
        {"view_case_drill.case_site_line_spells_the_site_and_size", bt_test_view_case_drill_case_site_line_spells_the_site_and_size},
    };
    return bt_harness_run("app_view_case_drill_test", tests, 15);
}
