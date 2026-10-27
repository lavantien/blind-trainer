#include "app/views/memo.h"
#include "app/views/solve.h"
#include "app/views/state.h"
#include "data/paths.h"

#include "common.h"

#include <sys/stat.h>

enum {
    KEY_SPACE = 32,
};

static const char *COVERED_ALG = "R2 U' S R2 S' R2 U R2";
static const char *COVERED_POSITION = "UF-UB-UR";

static bt_corpus_row heap_row(bt_sheet_class sheet, const char *position, const char *alg) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = BT_SOLVER_CHERRY;
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

static bt_cube corner_pair_scramble(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_CORNER;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_CORNER, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_CORNER, second);
    uint8_t occ[54];
    bt_cube solved = bt_cube_solved();
    bt_scheme_state_array(&solved, occ);
    bt_scheme_cycles inverse;
    bt_scheme_ideal_cycle_at(BT_ORBIT_CORNER, BT_SCHEME_CORNER_BUFFER_STICKER, case_id.t2,
                             case_id.t1, &inverse);
    bt_scheme_apply_sticker_cycles(occ, &inverse);
    return bt_cube_from_occ(occ);
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
             ta_temp_dir("views_solve", tag));
}

static void enter_solve(bt_app_ctx *ctx, bt_cube scramble) {
    bt_view_solve_set_scramble_override(scramble);
    bt_view_solve_app_state *app = bt_view_solve_app();
    if (!app->initialized) {
        bt_view_solve_app_init(app);
    }
    bt_view_solve_enter_state(ctx, app);
    BT_CHECK(ctx->solve_phase == BT_SOLVE_READY);
}

static int line_count_contains(char lines[][BT_SOLVE_TEXT_MAX], size_t count, const char *text) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(lines[i], text) == 0) {
            return 1;
        }
    }
    return 0;
}

BT_TEST(views_solve, solve_cycle_records_exact_splits_and_a_passing_outcome) {
    bt_corpus_row row = heap_row(BT_SHEET_EDGE, COVERED_POSITION, COVERED_ALG);
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "cycle");
    bt_corpus_row *heap = malloc(sizeof *heap);
    memcpy(heap, &row, sizeof *heap);
    bt_corpus_from_rows(heap, 1, &ctx.vendor_corpus);
    enter_solve(&ctx, single_pair_scramble('A', 'B'));

    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_MEMO);
    bt_view_solve_tick(&ctx, app, 0.3F);
    bt_view_solve_tick(&ctx, app, 0.3F);
    bt_view_solve_tick(&ctx, app, 0.3F);

    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_EXEC);
    bt_view_solve_tick(&ctx, app, 0.25F);
    bt_view_solve_tick(&ctx, app, 0.25F);

    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    BT_CHECK_EQ(ctx.stores.solves.count, (size_t)1);
    bt_solve_record record = ctx.stores.solves.items[0];
    BT_CHECK(record.success);
    BT_CHECK_EQ(record.memo_ms, (long long)900);
    BT_CHECK_EQ(record.exec_ms, (long long)500);
    BT_CHECK_EQ(record.memo_ms + record.exec_ms, (long long)1400);
    BT_CHECK(record.started_ms > 0);
    BT_CHECK(app->session.has_finished);
    BT_CHECK(app->session.finished.outcome.success);
    BT_CHECK_EQ(app->session.finished.outcome.broken_count, (size_t)0);
    char lines[BT_SOLVE_LINES_MAX][BT_SOLVE_TEXT_MAX];
    size_t count = bt_view_solve_review_lines(&app->session, lines, BT_SOLVE_LINES_MAX);
    BT_CHECK(line_count_contains(lines, count, "solved"));
    BT_CHECK(line_count_contains(lines, count, "memo 0.9s"));
    BT_CHECK(line_count_contains(lines, count, "exec 0.5s"));
    BT_CHECK(line_count_contains(lines, count, "total 1.4s"));
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx.paths.user_root, db_path);
    struct _stat info;
    BT_CHECK(_stat(db_path, &info) == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_solve, solve_reports_the_pair_that_broke_it_when_uncovered) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "broke");
    enter_solve(&ctx, corner_pair_scramble('A', 'B'));
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    BT_CHECK(!app->session.finished.outcome.success);
    BT_CHECK_EQ(app->session.finished.outcome.broken_count, (size_t)1);
    BT_CHECK(strcmp(app->session.finished.outcome.broken[0], "UFR-UBL-UBR uncovered") == 0);
    BT_CHECK(!ctx.stores.solves.items[0].success);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_solve, solve_review_space_starts_the_next_scramble) {
    bt_app_ctx ctx;
    fill_solve_ctx(&ctx, "next");
    enter_solve(&ctx, single_pair_scramble('A', 'B'));
    bt_view_solve_app_state *app = bt_view_solve_app();
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_REVIEW);
    uint64_t first_seed = app->session.seed;

    bt_view_solve_key(&ctx, app, KEY_SPACE);
    BT_CHECK(ctx.solve_phase == BT_SOLVE_READY);
    BT_CHECK(app->session.seed != first_seed);
    BT_CHECK(!app->session.has_finished);
    BT_CHECK(!bt_cube_is_solved(&app->session.scramble));
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_solve, zone_texts_drive_every_zone_through_each_phase) {
    bt_solve_session session;
    bt_view_solve_session_init(&session);
    session.seed = 7;
    session.token_count = 1;
    session.tokens[0].letters[0] = 'A';
    session.tokens[0].letters[1] = 'B';
    session.shown = 0;
    session.memo_secs = 2.5;
    session.exec_secs = 1.2;
    session.has_finished = 1;
    session.finished.record.started_ms = 1;
    session.finished.record.memo_ms = 900;
    session.finished.record.exec_ms = 500;
    session.finished.record.success = 1;
    session.finished.has_outcome = 1;
    session.finished.outcome.success = 1;
    session.finished.outcome.broken[0] = _strdup("UF & UL uncovered");
    session.finished.outcome.broken[1] = _strdup("UBL & DFR uncovered");
    session.finished.outcome.broken_count = 2;

    char text[BT_SOLVE_TEXT_MAX];
    bt_view_solve_phase_label(BT_SOLVE_READY, &session, text);
    BT_CHECK(strcmp(text, "ready, seed 7") == 0);
    bt_view_solve_focal_text(BT_SOLVE_READY, &session, text);
    BT_CHECK(text[0] == '\0');
    bt_view_solve_timer_text(BT_SOLVE_READY, &session, text);
    BT_CHECK(strcmp(text, "1 pairs") == 0);
    BT_CHECK(strcmp(bt_view_solve_hint_text(BT_SOLVE_READY), "space to start memo") == 0);
    bt_solve_view_parts parts;
    bt_view_solve_review_parts(&session, &parts);
    BT_CHECK(strcmp(parts.splits, "memo 0.9s\nexec 0.5s\ntotal 1.4s") == 0);
    BT_CHECK(strcmp(parts.broken, "UF & UL uncovered\nUBL & DFR uncovered") == 0);
    BT_CHECK(strcmp(parts.result, "solved") == 0);

    bt_view_solve_focal_text(BT_SOLVE_MEMO, &session, text);
    BT_CHECK(strcmp(text, "0/1") == 0);
    bt_view_solve_timer_text(BT_SOLVE_MEMO, &session, text);
    BT_CHECK(strcmp(text, "memo 2.5s") == 0);
    BT_CHECK(strcmp(bt_view_solve_hint_text(BT_SOLVE_MEMO), "space to finish memo") == 0);

    bt_view_solve_focal_text(BT_SOLVE_EXEC, &session, text);
    BT_CHECK(text[0] == '\0');
    bt_view_solve_timer_text(BT_SOLVE_EXEC, &session, text);
    BT_CHECK(strcmp(text, "exec 1.2s") == 0);
    BT_CHECK(strcmp(bt_view_solve_hint_text(BT_SOLVE_EXEC), "space to stop") == 0);

    bt_view_solve_focal_text(BT_SOLVE_REVIEW, &session, text);
    BT_CHECK(text[0] == '\0');
    bt_view_solve_timer_text(BT_SOLVE_REVIEW, &session, text);
    BT_CHECK(text[0] == '\0');
    BT_CHECK(strcmp(bt_view_solve_hint_text(BT_SOLVE_REVIEW), "space next solve, v 3d replay") ==
             0);
    bt_view_solve_review_parts(&session, &parts);
    BT_CHECK(strcmp(parts.result, "solved") == 0);

    session.finished.outcome.success = 0;
    bt_solve_outcome_free(&session.finished.outcome);
    session.finished.outcome.broken_count = 0;
    bt_view_solve_review_parts(&session, &parts);
    BT_CHECK(strcmp(parts.result, "failed") == 0);
    BT_CHECK(parts.broken[0] == '\0');
    bt_view_solve_session_free(&session);
}
