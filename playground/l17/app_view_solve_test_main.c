#include "common/bt_harness.h"

extern void bt_test_view_solve_simulate_passes_when_the_covered_alg_solves_the_scramble(void);
extern void bt_test_view_solve_simulate_reports_the_pair_that_broke_it(void);
extern void bt_test_view_solve_executed_moves_walks_pairs_then_residue(void);
extern void bt_test_view_solve_tokens_are_built_from_the_solve_memo(void);
extern void bt_test_view_solve_zone_texts_track_the_phase(void);
extern void bt_test_view_solve_review_parts_split_verdict_splits_and_broken(void);

int main(void) {
    bt_test_entry tests[] = {
        {"view_solve.simulate_passes_when_the_covered_alg_solves_the_scramble", bt_test_view_solve_simulate_passes_when_the_covered_alg_solves_the_scramble},
        {"view_solve.simulate_reports_the_pair_that_broke_it", bt_test_view_solve_simulate_reports_the_pair_that_broke_it},
        {"view_solve.executed_moves_walks_pairs_then_residue", bt_test_view_solve_executed_moves_walks_pairs_then_residue},
        {"view_solve.tokens_are_built_from_the_solve_memo", bt_test_view_solve_tokens_are_built_from_the_solve_memo},
        {"view_solve.zone_texts_track_the_phase", bt_test_view_solve_zone_texts_track_the_phase},
        {"view_solve.review_parts_split_verdict_splits_and_broken", bt_test_view_solve_review_parts_split_verdict_splits_and_broken},
    };
    return bt_harness_run("app_view_solve_test", tests, 6);
}
