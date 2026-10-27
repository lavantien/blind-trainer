#include "app/views/drill.h"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_case edge_case(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, second);
    return case_id;
}

static bt_case corner_case(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_CORNER;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_CORNER, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_CORNER, second);
    return case_id;
}

static bt_corpus_row heap_row(bt_solver solver, bt_sheet_class sheet, const char *position,
                              const char *alg_text, const char *commutator) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = solver;
    row.sheet = sheet;
    row.rowid = 1;
    row.position = _strdup(position);
    char err_token[64];
    if (!bt_alg_parse(alg_text, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg_text, err_token);
    }
    row.alg_text = _strdup(alg_text);
    row.commutator = _strdup(commutator);
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static bt_corpus_row edge_row(const char *position, const char *alg, const char *commutator) {
    return heap_row(BT_SOLVER_CHERRY, BT_SHEET_EDGE, position, alg, commutator);
}

static bt_corpus corpus_of_rows(bt_corpus_row *rows, size_t count) {
    bt_corpus_row *heap = NULL;
    if (count > 0) {
        heap = malloc(count * sizeof *heap);
        memcpy(heap, rows, count * sizeof *heap);
    }
    bt_corpus corpus;
    bt_corpus_from_rows(heap, count, &corpus);
    return corpus;
}

static bt_corpus single_pair_corpus(const char *alg) {
    bt_corpus_row rows[1] = {edge_row("UF-UB-UR", alg, "")};
    return corpus_of_rows(rows, 1);
}

static bt_stores stores_with_reps(unsigned int reps) {
    bt_stores stores;
    memset(&stores, 0, sizeof stores);
    stores.settings.memo_cadence_ms = 800;
    stores.settings.memo_mode = BT_MEMO_SYLLABLES;
    stores.settings.drill_reps = reps;
    bt_srs_store_init(&stores.srs);
    bt_solve_log_init(&stores.solves);
    bt_user_algs_init(&stores.user_algs);
    bt_str_list_init(&stores.seeds.hidden);
    bt_str_list_init(&stores.seeds.warnings);
    return stores;
}

BT_TEST(view_drill, case_position_spells_buffer_t1_t2) {
    char position[BT_DRILL_TEXT_MAX];
    bt_case edge = edge_case('A', 'B');
    bt_view_case_position(&edge, position);
    BT_CHECK(strcmp(position, "UF-UB-UR") == 0);
    bt_case corner = corner_case('A', 'B');
    bt_view_case_position(&corner, position);
    BT_CHECK(strcmp(position, "UFR-UBL-UBR") == 0);
    bt_case flipped = bt_view_flip_case(&edge);
    bt_view_case_position(&flipped, position);
    BT_CHECK(strcmp(position, "UF-UR-UB") == 0);
}

BT_TEST(view_drill, rating_keys_map_1_to_4) {
    bt_grade rating;
    BT_CHECK(bt_view_rating_of(49, &rating) && rating == BT_GRADE_AGAIN);
    BT_CHECK(bt_view_rating_of(50, &rating) && rating == BT_GRADE_HARD);
    BT_CHECK(bt_view_rating_of(51, &rating) && rating == BT_GRADE_GOOD);
    BT_CHECK(bt_view_rating_of(52, &rating) && rating == BT_GRADE_EASY);
    BT_CHECK(!bt_view_rating_of(82, &rating));
}

BT_TEST(view_drill, even_reps_rounds_up_and_floors_at_two) {
    unsigned int expected[7] = {2, 2, 2, 4, 4, 6, 6};
    for (unsigned int n = 0; n < 7; n++) {
        BT_CHECK(bt_view_even_reps(n) == expected[n]);
    }
}

BT_TEST(view_drill, even_reps_survives_the_u32_maximum) {
    unsigned int reps = bt_view_even_reps(0xFFFFFFFFU);
    BT_CHECK(reps % 2 == 0);
    BT_CHECK(reps >= 2);
}

BT_TEST(view_drill, expand_alg_line_shows_commutator_cancellation_beyond_the_written_alg) {
    const char *alg_text = "U' R U R' U' U S U' U R U' R' U S'";
    bt_alg alg;
    memset(&alg, 0, sizeof alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse(alg_text, &alg, err_token, sizeof err_token));
    const char *displayed[2] = {"", alg_text};
    for (int i = 0; i < 2; i++) {
        char line[BT_DRILL_TEXT_MAX];
        BT_CHECK(bt_view_expand_alg_line("[U':[R,U],S]", &alg, alg_text, displayed[i], line));
        BT_CHECK(strcmp(line, "expand U' R U R' S R U' R' U S'") == 0);
    }
    bt_alg_free(&alg);
}

BT_TEST(view_drill, expand_alg_line_stays_silent_when_nothing_new_is_shown) {
    const char *alg_text = "R2 U' S R2 S' R2 U R2";
    bt_alg alg;
    memset(&alg, 0, sizeof alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse(alg_text, &alg, err_token, sizeof err_token));
    char line[BT_DRILL_TEXT_MAX];
    BT_CHECK(!bt_view_expand_alg_line("R2 U':[S,R2]", &alg, alg_text, alg_text, line));
    BT_CHECK(!bt_view_expand_alg_line("", &alg, alg_text, alg_text, line));
    BT_CHECK(!bt_view_expand_alg_line("Not found.", &alg, alg_text, alg_text, line));
    BT_CHECK(!bt_view_expand_alg_line("junk text", &alg, alg_text, alg_text, line));
    bt_alg short_alg;
    memset(&short_alg, 0, sizeof short_alg);
    BT_CHECK(bt_alg_parse("R U", &short_alg, err_token, sizeof err_token));
    BT_CHECK(!bt_view_expand_alg_line("[R,U]", &short_alg, "R U", "R U", line));
    bt_alg_free(&short_alg);
    bt_alg_free(&alg);
}

BT_TEST(view_drill, expand_flat_line_reports_only_real_cancellations_and_repeats) {
    char line[BT_DRILL_TEXT_MAX];
    BT_CHECK(bt_view_expand_flat_line("R R U", line));
    BT_CHECK(strcmp(line, "expand R2 U") == 0);
    BT_CHECK(bt_view_expand_flat_line("(R U R')2", line));
    BT_CHECK(strcmp(line, "expand R U2 R'") == 0);
    BT_CHECK(!bt_view_expand_flat_line("R U", line));
    BT_CHECK(!bt_view_expand_flat_line("", line));
}

BT_TEST(view_drill, install_pair_prepares_the_drilling_state) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2");
    bt_stores stores = stores_with_reps(5);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    BT_CHECK(session.reps_target == 6);
    BT_CHECK(session.reps_target == bt_view_even_reps(5));
    bt_cube expected = bt_cube_solved();
    bt_cube_apply_alg(&expected, &session.side_b.alg);
    BT_CHECK(bt_cube_eq(&session.prepared, &expected));
    BT_CHECK(!bt_cube_is_solved(&session.prepared));
    bt_cube solved_again = session.prepared;
    bt_cube_apply_alg(&solved_again, &session.side_a.alg);
    BT_CHECK(bt_cube_is_solved(&solved_again));
    BT_CHECK(session.reps_side_a == 1);
    BT_CHECK(session.reps_done == 0);
    BT_CHECK(session.split_count == 0);
    bt_corpus empty = corpus_of_rows(NULL, 0);
    bt_view_install_pair(&session, &empty, &stores, edge_case('A', 'B'));
    BT_CHECK(bt_drill_session_sides(&session, NULL) == NULL);
    BT_CHECK(session.has_pin && session.pin.t1 == edge_case('A', 'B').t1 &&
             session.pin.orbit == BT_ORBIT_EDGE);
    BT_CHECK(bt_cube_is_solved(&session.prepared));
    BT_CHECK(session.reps_target == 0);
    bt_corpus_free(&empty);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill, net_sync_sets_the_side_a_targets_and_clears_without_sides) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2");
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_app_net_state net;
    bt_view_drill_net_sync(&net, &session);
    BT_CHECK(bt_cube_is_solved(&net.cube));
    BT_CHECK(net.has_buffer && net.buffer_sticker == bt_view_buffer_of(BT_ORBIT_EDGE));
    BT_CHECK(net.has_target[0] && net.targets[0] == session.side_a.case_id.t1);
    BT_CHECK(net.has_target[1] && net.targets[1] == session.side_a.case_id.t2);
    session.has_side_a = 0;
    session.has_side_b = 0;
    bt_view_drill_net_sync(&net, &session);
    BT_CHECK(bt_cube_is_solved(&net.cube));
    BT_CHECK(!net.has_buffer);
    BT_CHECK(!net.has_target[0] && !net.has_target[1]);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill, pair_is_due_reads_both_positions_of_the_pair) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2");
    bt_stores stores = stores_with_reps(4);
    bt_case case_id = edge_case('A', 'B');
    BT_CHECK(bt_view_pair_is_due(&stores, 0, &case_id));
    char key_a[BT_DRILL_TEXT_MAX];
    char key_b[BT_DRILL_TEXT_MAX];
    char position_a[BT_DRILL_TEXT_MAX];
    char position_b[BT_DRILL_TEXT_MAX];
    bt_view_case_position(&case_id, position_a);
    bt_case flipped = bt_view_flip_case(&case_id);
    bt_view_case_position(&flipped, position_b);
    bt_view_card_key(BT_SHEET_EDGE, position_a, key_a);
    bt_view_card_key(BT_SHEET_EDGE, position_b, key_b);
    bt_card card;
    memset(&card, 0, sizeof card);
    card.interval_days = 6;
    card.due_day = 10;
    card.reps = 2;
    bt_srs_store_set(&stores.srs, key_a, &card);
    bt_srs_store_set(&stores.srs, key_b, &card);
    BT_CHECK(!bt_view_pair_is_due(&stores, 0, &case_id));
    bt_card due_card = card;
    due_card.due_day = 0;
    bt_srs_store_set(&stores.srs, key_b, &due_card);
    BT_CHECK(bt_view_pair_is_due(&stores, 0, &case_id));
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill, pick_pair_draws_from_covered_cases) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2");
    bt_stores stores = stores_with_reps(4);
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    bt_case picked;
    BT_CHECK(bt_view_pick_pair(&corpus, &stores, 0, &rng, &picked));
    BT_CHECK(picked.orbit == BT_ORBIT_EDGE);
    bt_case covered = edge_case('A', 'B');
    BT_CHECK(picked.t1 == covered.t1 && picked.t2 == covered.t2);
    bt_corpus empty = corpus_of_rows(NULL, 0);
    BT_CHECK(!bt_view_pick_pair(&empty, &stores, 0, &rng, &picked));
    bt_corpus_free(&empty);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill, write_review_creates_and_advances_the_card) {
    bt_stores stores = stores_with_reps(4);
    bt_fsrs_config fsrs;
    bt_fsrs_config_defaults(&fsrs);
    bt_view_write_review(&stores, &fsrs, BT_SHEET_EDGE, "UF-UB-UR", BT_GRADE_GOOD, 0);
    const bt_card *card = bt_srs_store_get(&stores.srs, "edge/UF-UB-UR");
    BT_CHECK(card != NULL);
    BT_CHECK(card->due_day >= 1);
    BT_CHECK(!bt_card_is_due(card, 0));
    BT_CHECK(bt_card_is_due(card, card->due_day));
    bt_stores_free(&stores);
}
