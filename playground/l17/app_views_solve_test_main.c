#include "common/bt_harness.h"

extern void bt_test_views_solve_solve_cycle_records_exact_splits_and_a_passing_outcome(void);
extern void bt_test_views_solve_solve_reports_the_pair_that_broke_it_when_uncovered(void);
extern void bt_test_views_solve_solve_review_space_starts_the_next_scramble(void);
extern void bt_test_views_solve_zone_texts_drive_every_zone_through_each_phase(void);

int main(void) {
    bt_test_entry tests[] = {
        {"views_solve.solve_cycle_records_exact_splits_and_a_passing_outcome", bt_test_views_solve_solve_cycle_records_exact_splits_and_a_passing_outcome},
        {"views_solve.solve_reports_the_pair_that_broke_it_when_uncovered", bt_test_views_solve_solve_reports_the_pair_that_broke_it_when_uncovered},
        {"views_solve.solve_review_space_starts_the_next_scramble", bt_test_views_solve_solve_review_space_starts_the_next_scramble},
        {"views_solve.zone_texts_drive_every_zone_through_each_phase", bt_test_views_solve_zone_texts_drive_every_zone_through_each_phase},
    };
    return bt_harness_run("app_views_solve_test", tests, 4);
}
