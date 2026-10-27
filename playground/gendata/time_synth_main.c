#include <stdio.h>
#include <time.h>

extern void bt_test_gendata_synth_best_plateau_selection_picks_the_rank_minimum(void);
extern void bt_test_gendata_synth_corner_head_pair_gets_a_verified_commutator(void);
extern void bt_test_gendata_synth_corner_tail_pair_gets_a_verified_commutator(void);
extern void bt_test_gendata_synth_edge_head_pair_gets_a_verified_commutator(void);
extern void bt_test_gendata_synth_edge_tail_pair_gets_a_verified_commutator(void);
extern void bt_test_gendata_synth_generated_entries_fill_only_uncovered_cases(void);
extern void bt_test_gendata_synth_plateau_stays_inside_the_first_matching_setup(void);
extern void bt_test_gendata_synth_scoring_flips_the_selected_alg_for_the_pinned_case(void);
extern void bt_test_gendata_synth_synthesis_is_deterministic(void);

typedef void (*test_fn)(void);

int main(void) {
    setvbuf(stderr, NULL, _IONBF, 0);
    static const char *names[] = {
        "best_plateau",  "corner_head",  "corner_tail",  "edge_head",
        "edge_tail",     "fill_uncovered", "plateau_first",
        "scoring_flip",  "deterministic",
    };
    static const test_fn fns[] = {
        bt_test_gendata_synth_best_plateau_selection_picks_the_rank_minimum,
        bt_test_gendata_synth_corner_head_pair_gets_a_verified_commutator,
        bt_test_gendata_synth_corner_tail_pair_gets_a_verified_commutator,
        bt_test_gendata_synth_edge_head_pair_gets_a_verified_commutator,
        bt_test_gendata_synth_edge_tail_pair_gets_a_verified_commutator,
        bt_test_gendata_synth_generated_entries_fill_only_uncovered_cases,
        bt_test_gendata_synth_plateau_stays_inside_the_first_matching_setup,
        bt_test_gendata_synth_scoring_flips_the_selected_alg_for_the_pinned_case,
        bt_test_gendata_synth_synthesis_is_deterministic,
    };
    for (int i = 0; i < 9; i++) {
        clock_t started = clock();
        fns[i]();
        double secs = (double)(clock() - started) / CLOCKS_PER_SEC;
        fprintf(stderr, "%6.1fs %s\n", secs, names[i]);
    }
    return 0;
}
