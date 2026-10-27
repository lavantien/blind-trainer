#include "app/views/memo.h"
#include "app/views/solve.h"
#include "app/views/state.h"

#include "common.h"

enum {
    KEY_SPACE = 32,
};

static const char *COVERED_ALG = "R2 U' S R2 S' R2 U R2";
static const char *COVERED_POSITION = "UF-UB-UR";

static bt_corpus_row heap_row(bt_sheet_class sheet, bt_solver solver, const char *position,
                              const char *alg) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = solver;
    row.sheet = sheet;
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

static void corpus_of_rows(bt_corpus_row *rows, size_t count, bt_corpus *out) {
    bt_corpus_row *heap = malloc(count * sizeof *heap);
    memcpy(heap, rows, count * sizeof *heap);
    bt_corpus_from_rows(heap, count, out);
}

static bt_cube single_pair_scramble(char first, char second) {
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
    return bt_cube_from_occ(occ);
}

static bt_cube alg_cube(const char *alg) {
    bt_cube cube = bt_cube_solved();
    bt_alg parsed;
    bt_alg_init(&parsed);
    char err_token[64];
    if (!bt_alg_parse(alg, &parsed, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg, err_token);
    }
    bt_cube_apply_alg(&cube, &parsed);
    bt_alg_free(&parsed);
    return cube;
}

static int same_state(const bt_cube *left, const bt_cube *right) {
    uint8_t a[54];
    uint8_t b[54];
    bt_scheme_state_array(left, a);
    bt_scheme_state_array(right, b);
    return memcmp(a, b, sizeof a) == 0;
}

static bt_residue residue_case(void) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 2;
    residue.flips[0] = 26;
    residue.flips[1] = 27;
    return residue;
}

static bt_residue twist_pair_residue(void) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 2;
    residue.twists[0].corner = 0;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 21;
    residue.twists[1].quarter = 2;
    return residue;
}

static bt_residue parity_residue(void) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.has_parity = true;
    residue.parity.edge[0] = 26;
    residue.parity.edge[1] = 24;
    residue.parity.corner[0] = 2;
    residue.parity.corner[1] = 1;
    return residue;
}

static bt_residue ltct_residue(void) {
    bt_residue residue = parity_residue();
    residue.twist_count = 2;
    residue.twists[0].corner = 0;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 21;
    residue.twists[1].quarter = 2;
    return residue;
}

static bt_cube scramble_from_residue(const bt_residue *residue) {
    uint8_t occ[54];
    bt_cube solved = bt_cube_solved();
    bt_scheme_state_array(&solved, occ);
    bt_scheme_apply_residue(occ, residue);
    return bt_cube_from_occ(occ);
}

static bt_solve_outcome run_residue(const bt_corpus *corpus, const bt_residue *residue,
                                    bt_cube *cube) {
    bt_solve_outcome broken;
    bt_solve_outcome_init(&broken);
    bt_alg moves;
    bt_alg_init(&moves);
    bt_view_solve_execute_residue(corpus, residue, &broken, &moves);
    for (size_t i = 0; i < bt_alg_len(&moves); i++) {
        bt_cube_apply(cube, moves.items[i]);
    }
    bt_alg_free(&moves);
    return broken;
}

static int broken_is(const bt_solve_outcome *broken, const char *text) {
    return broken->broken_count == 1 && strcmp(broken->broken[0], text) == 0;
}

BT_TEST(solve_more, flip_pair_residue_applies_the_covered_row_alg) {
    bt_corpus_row rows[1] = {heap_row(BT_SHEET_FLIPS, BT_SOLVER_CHERRY, "UF & UL", "R U R'")};
    bt_corpus corpus;
    corpus_of_rows(rows, 1, &corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = residue_case();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    bt_cube expected = alg_cube("R U R'");
    BT_CHECK(same_state(&cube, &expected));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, flip_pair_residue_reports_the_position_when_uncovered) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = residue_case();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "UF & UL uncovered"));
    BT_CHECK(bt_cube_is_solved(&cube));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, twist_pair_residue_prefers_the_canonical_row) {
    bt_corpus_row rows[2] = {
        heap_row(BT_SHEET_TWISTS, BT_SOLVER_CHERRY, "DFR & UBL", "F2 D2"),
        heap_row(BT_SHEET_TWISTS, BT_SOLVER_CHERRY, "UBL & DFR", "L' B' L"),
    };
    bt_corpus corpus;
    corpus_of_rows(rows, 2, &corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = twist_pair_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    bt_cube expected = alg_cube("L' B' L");
    BT_CHECK(same_state(&cube, &expected));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, twist_pair_residue_falls_back_to_the_swapped_row_when_canical_is_missing) {
    bt_corpus_row rows[1] = {heap_row(BT_SHEET_TWISTS, BT_SOLVER_CHERRY, "DFR & UBL", "F2 D2")};
    bt_corpus corpus;
    corpus_of_rows(rows, 1, &corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = twist_pair_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    bt_cube expected = alg_cube("F2 D2");
    BT_CHECK(same_state(&cube, &expected));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, twist_pair_residue_reports_canonical_when_both_orders_are_missing) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = twist_pair_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "UBL & DFR uncovered"));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, parity_residue_applies_the_covered_row_alg) {
    bt_corpus_row rows[1] = {
        heap_row(BT_SHEET_PARITY, BT_SOLVER_CHERRY, "UF-UB, UFR-UBR", "R U R' U'"),
    };
    bt_corpus corpus;
    corpus_of_rows(rows, 1, &corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = parity_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    bt_cube expected = alg_cube("R U R' U'");
    BT_CHECK(same_state(&cube, &expected));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, parity_residue_reports_the_position_when_uncovered) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = parity_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "UF-UB, UFR-UBR uncovered"));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, ltct_residue_applies_the_bracketed_position_alg) {
    bt_corpus_row rows[1] = {
        heap_row(BT_SHEET_LTCT, BT_SOLVER_CHERRY, "UF-UB, UFR-UBR[UBL]", "B2 L2"),
    };
    bt_corpus corpus;
    corpus_of_rows(rows, 1, &corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = ltct_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK_EQ(broken.broken_count, (size_t)0);
    bt_cube expected = alg_cube("B2 L2");
    BT_CHECK(same_state(&cube, &expected));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, ltct_residue_reports_the_bracketed_position_when_uncovered) {
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_residue residue = ltct_residue();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "UF-UB, UFR-UBR[UBL] uncovered"));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, twist_triple_residue_is_reported_as_unsupported) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 3;
    residue.twists[0].corner = 0;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 3;
    residue.twists[1].quarter = 1;
    residue.twists[2].corner = 21;
    residue.twists[2].quarter = 1;
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "residue unsupported: TwistTriple(0, 3, 21)"));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, compound_residue_is_reported_as_unsupported) {
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.flip_count = 3;
    residue.flips[0] = 26;
    residue.flips[1] = 27;
    residue.flips[2] = 25;
    bt_corpus corpus;
    bt_corpus_init(&corpus);
    bt_cube cube = bt_cube_solved();
    bt_solve_outcome broken = run_residue(&corpus, &residue, &cube);
    BT_CHECK(broken_is(&broken, "residue unsupported: Compound"));
    bt_solve_outcome_free(&broken);
    bt_corpus_free(&corpus);
}

BT_TEST(solve_more, simulate_prefers_the_user_alg_over_a_vendor_row_for_the_same_position) {
    bt_cube scramble = single_pair_scramble('A', 'B');
    bt_memo memo;
    bt_scheme_memo(&scramble, &memo);
    BT_CHECK_EQ(memo.edge_pair_count, (size_t)1);
    bt_corpus_row rows[2] = {
        heap_row(BT_SHEET_EDGE, BT_SOLVER_CHERRY, COVERED_POSITION, "R U R'"),
        heap_row(BT_SHEET_EDGE, BT_SOLVER_USER, COVERED_POSITION, COVERED_ALG),
    };
    bt_corpus overlay_wins;
    corpus_of_rows(rows, 2, &overlay_wins);
    bt_solve_outcome outcome;
    bt_solve_outcome_init(&outcome);
    bt_view_solve_simulate(&overlay_wins, &scramble, &memo, &outcome);
    BT_CHECK(outcome.success);
    bt_solve_outcome_free(&outcome);
    bt_corpus_free(&overlay_wins);

    bt_corpus_row vendor_rows[1] = {
        heap_row(BT_SHEET_EDGE, BT_SOLVER_CHERRY, COVERED_POSITION, "R U R'"),
    };
    bt_corpus vendor_only;
    corpus_of_rows(vendor_rows, 1, &vendor_only);
    bt_solve_outcome_init(&outcome);
    bt_view_solve_simulate(&vendor_only, &scramble, &memo, &outcome);
    BT_CHECK(!outcome.success);
    BT_CHECK_EQ(outcome.broken_count, (size_t)0);
    bt_solve_outcome_free(&outcome);
    bt_corpus_free(&vendor_only);
}

static void fill_solve_ctx(bt_app_ctx *ctx, const char *tag) {
    bt_app_ctx_init(ctx);
    ctx->state = BT_APP_SOLVE;
    ctx->has_corpus = 1;
    bt_corpus_init(&ctx->vendor_corpus);
    ctx->has_stores = 1;
    memset(&ctx->stores, 0, sizeof ctx->stores);
    ctx->stores.settings.memo_cadence_ms = 800;
    bt_srs_store_init(&ctx->stores.srs);
    bt_solve_log_init(&ctx->stores.solves);
    bt_user_algs_init(&ctx->stores.user_algs);
    bt_str_list_init(&ctx->stores.seeds.hidden);
    bt_str_list_init(&ctx->stores.seeds.warnings);
    bt_fsrs_config_defaults(&ctx->fsrs);
    snprintf(ctx->paths.user_root, sizeof ctx->paths.user_root, "%s",
             ta_temp_dir("solve_more", tag));
}

static bt_app_ctx covered_ctx(const char *tag, bt_corpus_row *row_out) {
    *row_out = heap_row(BT_SHEET_EDGE, BT_SOLVER_CHERRY, COVERED_POSITION, COVERED_ALG);
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, tag);
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, row_out, sizeof *heap);
    bt_corpus_from_rows(heap, 1, &ctx.vendor_corpus);
    return ctx;
}

static void enter_solve(bt_app_ctx *ctx, bt_cube scramble) {
    bt_view_solve_set_scramble_override(scramble);
    bt_view_solve_app_state *app = bt_view_solve_app();
    if (!app->initialized) {
        bt_view_solve_app_init(app);
    }
    bt_view_solve_enter_state(ctx, app);
}

static void finish_fast(bt_app_ctx *ctx) {
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(ctx, app, KEY_SPACE);
    BT_CHECK(ctx->solve_phase == BT_SOLVE_MEMO);
    bt_view_solve_key(ctx, app, KEY_SPACE);
    BT_CHECK(ctx->solve_phase == BT_SOLVE_EXEC);
    bt_view_solve_key(ctx, app, KEY_SPACE);
    BT_CHECK(ctx->solve_phase == BT_SOLVE_REVIEW);
}

BT_TEST(solve_more, zero_cadence_streams_the_whole_token_list_in_one_frame) {
    bt_corpus_row row;
    bt_app_ctx ctx = covered_ctx("cadence-zero", &row);
    ctx.stores.settings.memo_cadence_ms = 0;
    enter_solve(&ctx, single_pair_scramble('A', 'B'));
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_MEMO);
    bt_view_solve_tick(&ctx, app, 0.1F);
    BT_CHECK_EQ(app->session.token_count, (size_t)1);
    BT_CHECK_EQ(app->session.shown, app->session.token_count);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_tick(&ctx, app, 0.25F);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    BT_CHECK_EQ(ctx.stores.solves.count, (size_t)1);
    BT_CHECK(ctx.stores.solves.items[0].success);
    BT_CHECK_EQ(ctx.stores.solves.items[0].memo_ms, (long long)100);
    BT_CHECK_EQ(ctx.stores.solves.items[0].exec_ms, (long long)250);
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, token_stream_counts_up_at_the_configured_cadence) {
    bt_corpus_row row;
    bt_app_ctx ctx = covered_ctx("cadence-steps", &row);
    bt_cube scramble = single_pair_scramble('A', 'B');
    enter_solve(&ctx, scramble);
    bt_view_solve_app_state *app = bt_view_solve_app();
    BT_CHECK(same_state(&ctx.net.cube, &scramble));
    BT_CHECK(!ctx.net.has_buffer);
    BT_CHECK(!ctx.net.has_target[0] && !ctx.net.has_target[1]);

    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_MEMO);
    bt_view_solve_tick(&ctx, app, 0.3F);
    bt_view_solve_tick(&ctx, app, 0.3F);
    BT_CHECK_EQ(app->session.shown, (size_t)0);
    bt_view_solve_tick(&ctx, app, 0.3F);
    BT_CHECK_EQ(app->session.shown, (size_t)1);
    long long rounded = (long long)(app->session.memo_secs * 1000.0 + 0.5);
    BT_CHECK_EQ(rounded, (long long)900);

    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_tick(&ctx, app, 0.25F);
    bt_view_solve_tick(&ctx, app, 0.25F);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    BT_CHECK(ctx.stores.solves.items[0].success);
    BT_CHECK_EQ(ctx.stores.solves.items[0].memo_ms, (long long)900);
    BT_CHECK_EQ(ctx.stores.solves.items[0].exec_ms, (long long)500);
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, twist_triple_scramble_fails_with_an_unsupported_residue_report) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "twist-triple");
    bt_residue residue;
    memset(&residue, 0, sizeof residue);
    residue.twist_count = 3;
    residue.twists[0].corner = 0;
    residue.twists[0].quarter = 1;
    residue.twists[1].corner = 3;
    residue.twists[1].quarter = 1;
    residue.twists[2].corner = 21;
    residue.twists[2].quarter = 1;
    enter_solve(&ctx, scramble_from_residue(&residue));
    finish_fast(&ctx);
    bt_view_solve_app_state *app = bt_view_solve_app();
    static const char twist_triple_prefix[] = "residue unsupported: TwistTriple(";
    BT_CHECK(!app->session.finished.outcome.success);
    BT_CHECK_EQ(app->session.finished.outcome.broken_count, (size_t)1);
    BT_CHECK(strncmp(app->session.finished.outcome.broken[0], twist_triple_prefix,
                     sizeof twist_triple_prefix - 1) == 0);
    char lines[BT_SOLVE_LINES_MAX][BT_SOLVE_TEXT_MAX];
    size_t count = bt_view_solve_review_lines(&app->session, lines, BT_SOLVE_LINES_MAX);
    BT_CHECK(count > 0 && strcmp(lines[0], "failed") == 0);
    int saw_report = 0;
    for (size_t i = 0; i < count; i++) {
        saw_report = saw_report ||
                     strncmp(lines[i], twist_triple_prefix, sizeof twist_triple_prefix - 1) == 0;
    }
    BT_CHECK(saw_report);
    BT_CHECK(!ctx.stores.solves.items[0].success);
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, flip_pair_scramble_reports_the_flip_pair_position_as_broken) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "flip-pair");
    uint8_t occ[54];
    bt_cube solved = bt_cube_solved();
    bt_scheme_state_array(&solved, occ);
    unsigned char swap;
    swap = occ[26];
    occ[26] = occ[32];
    occ[32] = swap;
    swap = occ[27];
    occ[27] = occ[28];
    occ[28] = swap;
    enter_solve(&ctx, bt_cube_from_occ(occ));
    finish_fast(&ctx);
    bt_view_solve_app_state *app = bt_view_solve_app();
    BT_CHECK(!app->session.finished.outcome.success);
    BT_CHECK(broken_is(&app->session.finished.outcome, "UF & UL uncovered"));
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, parity_scramble_reports_the_parity_position_as_broken) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "parity");
    bt_residue residue = parity_residue();
    enter_solve(&ctx, scramble_from_residue(&residue));
    finish_fast(&ctx);
    bt_view_solve_app_state *app = bt_view_solve_app();
    BT_CHECK(!app->session.finished.outcome.success);
    BT_CHECK(broken_is(&app->session.finished.outcome, "UF-UB, UFR-UBR uncovered"));
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, user_overlay_alg_beats_the_vendor_row_during_the_solve_replay) {
    bt_corpus_row row = heap_row(BT_SHEET_EDGE, BT_SOLVER_CHERRY, COVERED_POSITION, "R U R'");
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "overlay");
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, &row, sizeof *heap);
    bt_corpus vendor;
    bt_corpus_from_rows(heap, 1, &vendor);
    bt_user_algs_set_entry(&ctx.stores.user_algs, "edge/UF-UB-UR", COVERED_ALG);
    bt_overlay_error err;
    BT_CHECK_EQ(bt_corpus_with_overlay(&vendor, &ctx.stores.user_algs, &ctx.vendor_corpus, &err),
                0);
    enter_solve(&ctx, single_pair_scramble('A', 'B'));
    finish_fast(&ctx);
    bt_view_solve_app_state *app = bt_view_solve_app();
    BT_CHECK(app->session.finished.outcome.success);
    BT_CHECK(ctx.stores.solves.items[0].success);
    bt_corpus_free(&vendor);
    bt_app_ctx_free(&ctx);
}

BT_TEST(solve_more, escape_mid_memo_returns_to_menu_and_a_reopen_restarts_ready) {
    bt_corpus_row row;
    bt_app_ctx ctx = covered_ctx("esc", &row);
    enter_solve(&ctx, single_pair_scramble('A', 'B'));
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_MEMO);

    bt_app_key_event escape;
    memset(&escape, 0, sizeof escape);
    escape.kind = BT_APP_KEY_ESCAPE;
    escape.pressed = 1;
    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_MENU);

    bt_view_solve_set_scramble_override(single_pair_scramble('A', 'B'));
    bt_view_solve_enter_state(&ctx, bt_view_solve_app());
    BT_CHECK(ctx.solve_phase == BT_SOLVE_READY);
    BT_CHECK(!app->has_scramble_override);
    BT_CHECK(!app->session.has_finished);
    finish_fast(&ctx);
    BT_CHECK_EQ(ctx.stores.solves.count, (size_t)1);
    BT_CHECK(ctx.stores.solves.items[0].success);
    bt_app_ctx_free(&ctx);
}
