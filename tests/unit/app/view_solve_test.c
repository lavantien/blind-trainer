#include "app/views/memo.h"
#include "app/views/solve.h"
#include "app/views/state.h"

#include "common.h"

static bt_corpus_row heap_row(const char *position, const char *alg) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
    row.sheet = BT_SHEET_EDGE;
    row.rowid = 1;
    row.position = _strdup(position);
    char err_token[64];
    if (!bt_alg_parse(alg, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    row.alg_text = _strdup(alg);
    row.commutator = _strdup("");
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static void corpus_of(bt_corpus_row *rows, size_t count, bt_corpus *out) {
    bt_corpus_row *heap = malloc(count * sizeof *heap);
    memcpy(heap, rows, count * sizeof *heap);
    bt_corpus_from_rows(heap, count, out);
}

static void single_pair_solve(char first, char second, bt_memo *memo, bt_cube *scramble) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, second);
    uint8_t occ[54];
    bt_cube solved = bt_cube_solved();
    bt_scheme_state_array(&solved, occ);
    bt_scheme_cycles inverse;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, BT_SCHEME_EDGE_BUFFER_STICKER, case_id.t2, case_id.t1,
                             &inverse);
    bt_scheme_apply_sticker_cycles(occ, &inverse);
    *scramble = bt_cube_from_occ(occ);
    bt_scheme_memo(scramble, memo);
}

BT_TEST(view_solve, simulate_passes_when_the_covered_alg_solves_the_scramble) {
    bt_corpus_row rows[1] = {heap_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2")};
    bt_corpus corpus;
    corpus_of(rows, 1, &corpus);
    bt_memo memo;
    bt_cube scramble;
    single_pair_solve('A', 'B', &memo, &scramble);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)1);
    char letters[2];
    bt_scheme_case_letters(&memo.edge_pairs[0], letters);
    BT_CHECK(letters[0] == 'A' && letters[1] == 'B');
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_view_solve_simulate(&corpus, &scramble, &memo, &outcome);
    BT_CHECK(outcome.success);
    bt_solve_outcome_free(&outcome);
    bt_corpus_free(&corpus);
}

BT_TEST(view_solve, simulate_reports_the_pair_that_broke_it) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_memo memo;
    bt_cube scramble;
    single_pair_solve('A', 'B', &memo, &scramble);
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_view_solve_simulate(&corpus, &scramble, &memo, &outcome);
    BT_CHECK(!outcome.success);
    BT_CHECK_EQ(outcome.broken_count, (size_t)1);
    BT_CHECK(strcmp(outcome.broken[0], "UF-UB-UR uncovered") == 0);
    bt_solve_outcome_free(&outcome);
    bt_corpus_free(&corpus);
}

BT_TEST(view_solve, executed_moves_walks_pairs_then_residue) {
    bt_corpus_row rows[1] = {heap_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2")};
    bt_corpus corpus;
    corpus_of(rows, 1, &corpus);
    bt_memo memo;
    bt_cube scramble;
    single_pair_solve('A', 'B', &memo, &scramble);
    bt_solve_outcome broken;
    bt_solve_outcome_init(&broken);
    bt_alg moves;
    bt_alg_init(&moves);
    bt_view_solve_executed_moves(&corpus, &memo, &broken, &moves);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    BT_CHECK_EQ(bt_alg_len(&moves), (size_t)8);
    bt_cube replayed = scramble;
    for (size_t i = 0; i < bt_alg_len(&moves); i++) {
        bt_cube_apply(&replayed, moves.items[i]);
    }
    BT_CHECK(bt_cube_is_solved(&replayed));
    bt_alg_free(&moves);
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(view_solve, tokens_are_built_from_the_solve_memo) {
    bt_memo memo;
    bt_cube scramble;
    single_pair_solve('A', 'B', &memo, &scramble);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t count = 0;
    bt_memo_build_tokens(&memo, &book, tokens, &count);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(tokens[0].letters[0] == 'A' && tokens[0].letters[1] == 'B');
}

BT_TEST(view_solve, zone_texts_track_the_phase) {
    bt_solve_session session;
    bt_view_solve_session_init(&session);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_view_solve_rebuild(&session, 7, &book, 0);
    char text[BT_SOLVE_TEXT_MAX];
    bt_view_solve_phase_label(BT_SOLVE_READY, &session, text);
    BT_CHECK(strcmp(text, "ready, seed 7") == 0);
    bt_view_solve_timer_text(BT_SOLVE_READY, &session, text);
    char wanted[BT_SOLVE_TEXT_MAX];
    snprintf(wanted, sizeof wanted, "%zu pairs", session.token_count);
    BT_CHECK(strcmp(text, wanted) == 0);
    BT_CHECK(strcmp(bt_view_solve_hint_text(BT_SOLVE_READY), "space to start memo") == 0);
    session.shown = 1;
    session.memo_secs = 2.5;
    bt_view_solve_focal_text(BT_SOLVE_MEMO, &session, text);
    snprintf(wanted, sizeof wanted, "1/%zu", session.token_count);
    BT_CHECK(strcmp(text, wanted) == 0);
    bt_view_solve_timer_text(BT_SOLVE_MEMO, &session, text);
    BT_CHECK(strcmp(text, "memo 2.5s") == 0);
    bt_view_solve_focal_text(BT_SOLVE_EXEC, &session, text);
    BT_CHECK(text[0] == '\0');
    session.exec_secs = 1.2;
    bt_view_solve_timer_text(BT_SOLVE_EXEC, &session, text);
    BT_CHECK(strcmp(text, "exec 1.2s") == 0);
    bt_view_solve_session_free(&session);
}

BT_TEST(view_solve, review_parts_split_verdict_splits_and_broken) {
    bt_solve_session session;
    bt_view_solve_session_init(&session);
    bt_solve_view_parts parts;
    bt_view_solve_review_parts(&session, &parts);
    BT_CHECK(parts.result[0] == '\0' && parts.splits[0] == '\0' && parts.broken[0] == '\0');
    session.has_finished = 1;
    session.finished.record.started_ms = 1;
    session.finished.record.memo_ms = 900;
    session.finished.record.exec_ms = 500;
    session.finished.record.success = 1;
    session.finished.has_outcome = 1;
    session.finished.outcome.success = 1;
    session.finished.outcome.broken[0] = _strdup("UF-UB-UR uncovered");
    session.finished.outcome.broken_count = 1;
    bt_view_solve_review_parts(&session, &parts);
    BT_CHECK(strcmp(parts.result, "solved") == 0);
    BT_CHECK(strcmp(parts.splits, "memo 0.9s\nexec 0.5s\ntotal 1.4s") == 0);
    BT_CHECK(strcmp(parts.broken, "UF-UB-UR uncovered") == 0);
    bt_view_solve_session_free(&session);
}
