#include "common/bt_harness.h"

extern void bt_test_views_case_drill_d_on_a_repo_case_opens_the_case_drill_in_show_phase(void);
extern void bt_test_views_case_drill_grading_persists_the_card_and_returns_to_the_browser(void);
extern void bt_test_views_case_drill_due_sessions_cover_the_event_and_skip_recently_graded_cases(void);
extern void bt_test_views_case_drill_a_stores_save_never_wipes_the_case_cards(void);
extern void bt_test_views_case_drill_letters_d_keeps_the_3bld_drill_and_the_3bld_arm_stays_inert(void);
extern void bt_test_views_case_drill_an_override_replaces_the_seed_alg_in_the_drill(void);
extern void bt_test_views_case_drill_escape_from_the_case_drill_returns_to_the_menu(void);
extern void bt_test_views_case_drill_d_on_the_letters_list_after_leaving_the_repo_never_opens_a_case_drill(void);
extern void bt_test_views_case_drill_a_failed_cards_load_reports_on_entry_and_blocks_grading(void);

int main(void) {
    bt_test_entry tests[] = {
        {"views_case_drill.d_on_a_repo_case_opens_the_case_drill_in_show_phase", bt_test_views_case_drill_d_on_a_repo_case_opens_the_case_drill_in_show_phase},
        {"views_case_drill.grading_persists_the_card_and_returns_to_the_browser", bt_test_views_case_drill_grading_persists_the_card_and_returns_to_the_browser},
        {"views_case_drill.due_sessions_cover_the_event_and_skip_recently_graded_cases", bt_test_views_case_drill_due_sessions_cover_the_event_and_skip_recently_graded_cases},
        {"views_case_drill.a_stores_save_never_wipes_the_case_cards", bt_test_views_case_drill_a_stores_save_never_wipes_the_case_cards},
        {"views_case_drill.letters_d_keeps_the_3bld_drill_and_the_3bld_arm_stays_inert", bt_test_views_case_drill_letters_d_keeps_the_3bld_drill_and_the_3bld_arm_stays_inert},
        {"views_case_drill.an_override_replaces_the_seed_alg_in_the_drill", bt_test_views_case_drill_an_override_replaces_the_seed_alg_in_the_drill},
        {"views_case_drill.escape_from_the_case_drill_returns_to_the_menu", bt_test_views_case_drill_escape_from_the_case_drill_returns_to_the_menu},
        {"views_case_drill.d_on_the_letters_list_after_leaving_the_repo_never_opens_a_case_drill", bt_test_views_case_drill_d_on_the_letters_list_after_leaving_the_repo_never_opens_a_case_drill},
        {"views_case_drill.a_failed_cards_load_reports_on_entry_and_blocks_grading", bt_test_views_case_drill_a_failed_cards_load_reports_on_entry_and_blocks_grading},
    };
    return bt_harness_run("app_views_case_drill_test", tests, 9);
}
