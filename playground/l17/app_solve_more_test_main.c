#include "common/bt_harness.h"

extern void bt_test_solve_more_flip_pair_residue_applies_the_covered_row_alg(void);
extern void bt_test_solve_more_flip_pair_residue_reports_the_position_when_uncovered(void);
extern void bt_test_solve_more_twist_pair_residue_prefers_the_canonical_row(void);
extern void bt_test_solve_more_twist_pair_residue_falls_back_to_the_swapped_row_when_canical_is_missing(void);
extern void bt_test_solve_more_twist_pair_residue_reports_canonical_when_both_orders_are_missing(void);
extern void bt_test_solve_more_parity_residue_applies_the_covered_row_alg(void);
extern void bt_test_solve_more_parity_residue_reports_the_position_when_uncovered(void);
extern void bt_test_solve_more_ltct_residue_applies_the_bracketed_position_alg(void);
extern void bt_test_solve_more_ltct_residue_reports_the_bracketed_position_when_uncovered(void);
extern void bt_test_solve_more_twist_triple_residue_is_reported_as_unsupported(void);
extern void bt_test_solve_more_compound_residue_is_reported_as_unsupported(void);
extern void bt_test_solve_more_simulate_prefers_the_user_alg_over_a_vendor_row_for_the_same_position(void);
extern void bt_test_solve_more_zero_cadence_streams_the_whole_token_list_in_one_frame(void);
extern void bt_test_solve_more_token_stream_counts_up_at_the_configured_cadence(void);
extern void bt_test_solve_more_twist_triple_scramble_fails_with_an_unsupported_residue_report(void);
extern void bt_test_solve_more_flip_pair_scramble_reports_the_flip_pair_position_as_broken(void);
extern void bt_test_solve_more_parity_scramble_reports_the_parity_position_as_broken(void);
extern void bt_test_solve_more_user_overlay_alg_beats_the_vendor_row_during_the_solve_replay(void);
extern void bt_test_solve_more_escape_mid_memo_returns_to_menu_and_a_reopen_restarts_ready(void);

int main(void) {
    bt_test_entry tests[] = {
        {"solve_more.flip_pair_residue_applies_the_covered_row_alg", bt_test_solve_more_flip_pair_residue_applies_the_covered_row_alg},
        {"solve_more.flip_pair_residue_reports_the_position_when_uncovered", bt_test_solve_more_flip_pair_residue_reports_the_position_when_uncovered},
        {"solve_more.twist_pair_residue_prefers_the_canonical_row", bt_test_solve_more_twist_pair_residue_prefers_the_canonical_row},
        {"solve_more.twist_pair_residue_falls_back_to_the_swapped_row_when_canical_is_missing", bt_test_solve_more_twist_pair_residue_falls_back_to_the_swapped_row_when_canical_is_missing},
        {"solve_more.twist_pair_residue_reports_canonical_when_both_orders_are_missing", bt_test_solve_more_twist_pair_residue_reports_canonical_when_both_orders_are_missing},
        {"solve_more.parity_residue_applies_the_covered_row_alg", bt_test_solve_more_parity_residue_applies_the_covered_row_alg},
        {"solve_more.parity_residue_reports_the_position_when_uncovered", bt_test_solve_more_parity_residue_reports_the_position_when_uncovered},
        {"solve_more.ltct_residue_applies_the_bracketed_position_alg", bt_test_solve_more_ltct_residue_applies_the_bracketed_position_alg},
        {"solve_more.ltct_residue_reports_the_bracketed_position_when_uncovered", bt_test_solve_more_ltct_residue_reports_the_bracketed_position_when_uncovered},
        {"solve_more.twist_triple_residue_is_reported_as_unsupported", bt_test_solve_more_twist_triple_residue_is_reported_as_unsupported},
        {"solve_more.compound_residue_is_reported_as_unsupported", bt_test_solve_more_compound_residue_is_reported_as_unsupported},
        {"solve_more.simulate_prefers_the_user_alg_over_a_vendor_row_for_the_same_position", bt_test_solve_more_simulate_prefers_the_user_alg_over_a_vendor_row_for_the_same_position},
        {"solve_more.zero_cadence_streams_the_whole_token_list_in_one_frame", bt_test_solve_more_zero_cadence_streams_the_whole_token_list_in_one_frame},
        {"solve_more.token_stream_counts_up_at_the_configured_cadence", bt_test_solve_more_token_stream_counts_up_at_the_configured_cadence},
        {"solve_more.twist_triple_scramble_fails_with_an_unsupported_residue_report", bt_test_solve_more_twist_triple_scramble_fails_with_an_unsupported_residue_report},
        {"solve_more.flip_pair_scramble_reports_the_flip_pair_position_as_broken", bt_test_solve_more_flip_pair_scramble_reports_the_flip_pair_position_as_broken},
        {"solve_more.parity_scramble_reports_the_parity_position_as_broken", bt_test_solve_more_parity_scramble_reports_the_parity_position_as_broken},
        {"solve_more.user_overlay_alg_beats_the_vendor_row_during_the_solve_replay", bt_test_solve_more_user_overlay_alg_beats_the_vendor_row_during_the_solve_replay},
        {"solve_more.escape_mid_memo_returns_to_menu_and_a_reopen_restarts_ready", bt_test_solve_more_escape_mid_memo_returns_to_menu_and_a_reopen_restarts_ready},
    };
    return bt_harness_run("app_solve_more_test", tests, 19);
}
