#include "app/usage.h"
#include "app/views/drill.h"
#include "app/views/memo.h"
#include "app/views/solve.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"

#include "common.h"

#include "raylib.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *dup_text(const char *text) {
    char *copy = malloc(strlen(text) + 1);
    if (copy == NULL) {
        BT_ABORT("oom");
    }
    strcpy(copy, text);
    return copy;
}

static bt_corpus corpus_of(const char *position, const char *alg_text) {
    bt_corpus_row *rows = malloc(sizeof(bt_corpus_row));
    BT_CHECK(rows != NULL);
    memset(rows, 0, sizeof *rows);
    rows->solver = BT_SOLVER_CHERRY;
    rows->sheet = BT_SHEET_EDGE;
    rows->rowid = 1;
    rows->position = dup_text(position);
    bt_alg_init(&rows->alg);
    char err_token[64];
    if (!bt_alg_parse(alg_text, &rows->alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg parses");
    }
    rows->alg_text = dup_text(alg_text);
    rows->commutator = dup_text("");
    rows->recon_link = dup_text("");
    rows->youtube_link = dup_text("");
    bt_corpus corpus;
    bt_corpus_from_rows(rows, 1, &corpus);
    return corpus;
}

static bt_cube single_pair_scramble(char first, char second) {
    uint8_t t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, first);
    uint8_t t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, second);
    bt_cube solved = bt_cube_solved();
    uint8_t occ[54];
    bt_scheme_state_array(&solved, occ);
    bt_scheme_cycles inverse;
    bt_scheme_ideal_cycle_at(BT_ORBIT_EDGE, BT_SCHEME_EDGE_BUFFER_STICKER, t2, t1, &inverse);
    bt_scheme_apply_sticker_cycles(occ, &inverse);
    return bt_cube_from_occ(occ);
}

static void world_ctx(bt_app_ctx *ctx, bt_app_state state) {
    bt_app_ctx_init(ctx);
    ctx->state = state;
    ctx->drill_phase = BT_DRILL_CARD;
    ctx->memo_phase = BT_MEMO_INTRO;
    ctx->solve_phase = BT_SOLVE_READY;
    ctx->case_drill_phase = BT_CASE_DRILL_SHOW;
}

static void tour_world(bt_usage_tour *tour, const char *dir, const bt_usage_action *actions,
                       size_t count, size_t expected_shots) {
    memset(tour, 0, sizeof *tour);
    snprintf(tour->dir, sizeof tour->dir, "%s", dir);
    tour->index = 0;
    tour->mode = BT_USAGE_MODE_ACT;
    tour->exit_code = -1;
    bt_usage_tour_set_actions(tour, actions, count);
    tour->expected_shots = expected_shots;
}

static void step(bt_usage_tour *tour, bt_app_ctx *ctx, bt_usage_key_request *keys,
                 char shot_request[TA_PATH_MAX], int *has_shot_request) {
    keys->has_key = 0;
    keys->key = 0;
    keys->ch = 0;
    *has_shot_request = 0;
    shot_request[0] = '\0';
    tour->exit_code = bt_usage_step(tour, ctx, 1, 0.1F, keys, shot_request, has_shot_request);
}

BT_TEST(usage, covered_alg_text_reads_the_first_vendor_row) {
    bt_corpus corpus = corpus_of("UF-UB-UR", "R2 U' S R2 S' R2 U R2");
    bt_case pinned = bt_usage_pinned_case();
    BT_CHECK(strcmp(bt_usage_covered_alg_text(&corpus, &pinned), "R2 U' S R2 S' R2 U R2") == 0);
    bt_corpus empty;
    bt_corpus_init(&empty);
    BT_CHECK(bt_usage_covered_alg_text(&empty, &pinned) == NULL);
    bt_corpus_free(&corpus);
    bt_corpus_free(&empty);
}

BT_TEST(usage, fully_covered_judges_the_whole_memo) {
    bt_corpus corpus = corpus_of("UF-UB-UR", "R2 U' S R2 S' R2 U R2");
    bt_cube scramble = single_pair_scramble('A', 'B');
    BT_CHECK(bt_usage_fully_covered(&corpus, &scramble));
    bt_corpus empty;
    bt_corpus_init(&empty);
    BT_CHECK(!bt_usage_fully_covered(&empty, &scramble));
    bt_corpus_free(&corpus);
    bt_corpus_free(&empty);
}

BT_TEST(usage, covered_seed_gives_up_within_its_attempt_budget) {
    bt_corpus empty;
    bt_corpus_init(&empty);
    uint64_t found = 99;
    BT_CHECK(!bt_usage_covered_seed(&empty, 7, 8, &found));
    BT_CHECK_EQ(found, (uint64_t)99);
    bt_corpus_free(&empty);
}

BT_TEST(usage, the_verdict_follows_the_expected_shot_count) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "finish-ok"), NULL, 0, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.exit_code, 0);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_DONE);
    bt_usage_tour gap;
    tour_world(&gap, ta_temp_dir("usage", "finish-gap"), NULL, 0, 2);
    step(&gap, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(gap.exit_code, 1);
    BT_CHECK_EQ(gap.mode, BT_USAGE_MODE_DONE);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, press_and_press_char_request_their_keys) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[2] = {0};
    actions[0].kind = BT_USAGE_ACT_PRESS;
    actions[0].key = KEY_SPACE;
    actions[1].kind = BT_USAGE_ACT_PRESS_CHAR;
    actions[1].key = KEY_B;
    actions[1].ch = 'b';
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "press"), actions, 2, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.index, (size_t)1);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ACT);
    BT_CHECK(keys.has_key);
    BT_CHECK_EQ(keys.key, KEY_SPACE);
    BT_CHECK_EQ(keys.ch, '\0');
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.index, (size_t)2);
    BT_CHECK(keys.has_key);
    BT_CHECK_EQ(keys.key, KEY_B);
    BT_CHECK_EQ(keys.ch, 'b');
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, nav_and_pin_actions_forward_their_effects) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[3] = {0};
    actions[0].kind = BT_USAGE_ACT_NAV_OPEN;
    actions[0].app = BT_APP_ALGS;
    actions[1].kind = BT_USAGE_ACT_NAV_MENU;
    actions[2].kind = BT_USAGE_ACT_PIN_DRILL;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "nav-pin"), actions, 3, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_ALGS);
    BT_CHECK_EQ(tour.index, (size_t)1);
    ctx.has_pending = 0;
    ctx.state = BT_APP_ALGS;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK(ctx.has_pending);
    BT_CHECK_EQ(ctx.pending, BT_APP_MENU);
    BT_CHECK_EQ(tour.index, (size_t)2);
    ctx.state = BT_APP_MENU;
    bt_drill_app_state *drill = bt_view_drill_app();
    BT_CHECK(!drill->has_pending);
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.index, (size_t)3);
    BT_CHECK(drill->has_pending);
    bt_case pinned = bt_usage_pinned_case();
    BT_CHECK_EQ(drill->pending.orbit, pinned.orbit);
    BT_CHECK_EQ(drill->pending.t1, pinned.t1);
    BT_CHECK_EQ(drill->pending.t2, pinned.t2);
    drill->has_pending = 0;
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, pick_solve_seed_without_a_corpus_stops_the_tour) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_view_solve_app()->seed = 0;
    bt_usage_action actions[1] = {0};
    actions[0].kind = BT_USAGE_ACT_PICK_SOLVE_SEED;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "pick-seed"), actions, 1, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.exit_code, 1);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_DONE);
    BT_CHECK_EQ(tour.index, (size_t)0);
    BT_CHECK_EQ(bt_view_solve_app()->seed, (uint64_t)0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, waits_pass_immediately_when_the_target_matches) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[6] = {0};
    actions[0].kind = BT_USAGE_ACT_WAIT_APP;
    actions[0].app = BT_APP_MENU;
    actions[1].kind = BT_USAGE_ACT_WAIT_FONTS;
    actions[2].kind = BT_USAGE_ACT_WAIT_DRILL;
    actions[2].drill = BT_DRILL_CARD;
    actions[3].kind = BT_USAGE_ACT_WAIT_MEMO;
    actions[3].memo = BT_MEMO_INTRO;
    actions[4].kind = BT_USAGE_ACT_WAIT_SOLVE;
    actions[4].solve = BT_SOLVE_READY;
    actions[5].kind = BT_USAGE_ACT_WAIT_CASE_DRILL;
    actions[5].case_drill = BT_CASE_DRILL_SHOW;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "wait-hit"), actions, 6, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    for (size_t expected = 1; expected <= 6; expected++) {
        step(&tour, &ctx, &keys, request, &has_request);
        BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_WAIT_UNTIL);
        step(&tour, &ctx, &keys, request, &has_request);
        BT_CHECK_EQ(tour.index, expected);
        BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ACT);
    }
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, waits_time_out_when_the_target_never_arrives) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    ctx.drill_phase = BT_DRILL_CARD;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "wait-timeout"), NULL, 0, 0);
    tour.mode = BT_USAGE_MODE_WAIT_UNTIL;
    tour.wait.kind = BT_USAGE_WAIT_DRILL;
    tour.wait.drill = BT_DRILL_RATE;
    tour.wait.waited = 0.0F;
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.index, (size_t)0);
    BT_CHECK_EQ(tour.exit_code, -1);
    bt_usage_tour stuck;
    tour_world(&stuck, ta_temp_dir("usage", "wait-stuck"), NULL, 0, 0);
    stuck.mode = BT_USAGE_MODE_WAIT_UNTIL;
    stuck.wait.kind = BT_USAGE_WAIT_APP;
    stuck.wait.app = BT_APP_STATS;
    stuck.wait.waited = 29.95F;
    step(&stuck, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(stuck.exit_code, 1);
    BT_CHECK_EQ(stuck.mode, BT_USAGE_MODE_DONE);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, elapse_counts_down_by_the_frame_delta) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[1] = {0};
    actions[0].kind = BT_USAGE_ACT_ELAPSE;
    actions[0].secs = 0.25F;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "elapse"), actions, 1, 0);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ELAPSE);
    BT_CHECK_F64_NEAR((double)tour.elapse_left, 0.25, 1e-6);
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_F64_NEAR((double)tour.elapse_left, 0.15, 1e-6);
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_F64_NEAR((double)tour.elapse_left, 0.05, 1e-6);
    BT_CHECK_EQ(tour.index, (size_t)0);
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.index, (size_t)1);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ACT);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, shot_dwells_then_captures_then_waits_for_the_file) {
    const char *dir = ta_temp_dir("usage", "shot");
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[1] = {0};
    actions[0].kind = BT_USAGE_ACT_SHOT;
    actions[0].name = "probe";
    bt_usage_tour tour;
    tour_world(&tour, dir, actions, 1, 1);
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_SHOT);
    BT_CHECK_F64_NEAR((double)tour.shot.clock, -0.4, 1e-6);
    BT_CHECK(!tour.shot.taken);
    int frames = 0;
    while (!tour.shot.taken) {
        step(&tour, &ctx, &keys, request, &has_request);
        frames++;
        BT_CHECK(frames < 10);
    }
    BT_CHECK(has_request);
    BT_CHECK(strcmp(request, tour.shot.path) == 0);
    BT_CHECK(strstr(request, "/probe.png") != NULL);
    BT_CHECK_EQ(tour.index, (size_t)0);
    BT_CHECK_EQ(tour.shot_count, (size_t)0);
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK(tour.shot.taken);
    BT_CHECK_EQ(tour.shot_count, (size_t)0);
    ta_write_file(tour.shot.path, "png");
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.shot_count, (size_t)1);
    char expected[TA_PATH_MAX];
    snprintf(expected, sizeof expected, "%s/probe.png", dir);
    BT_CHECK(strcmp(tour.shots[0], expected) == 0);
    BT_CHECK_EQ(tour.index, (size_t)1);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ACT);
    remove(tour.shot.path);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, done_mode_stays_done_without_side_effects) {
    bt_app_ctx ctx;
    world_ctx(&ctx, BT_APP_MENU);
    bt_usage_action actions[1] = {0};
    actions[0].kind = BT_USAGE_ACT_PRESS;
    actions[0].key = KEY_SPACE;
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "done"), actions, 1, 0);
    tour.mode = BT_USAGE_MODE_DONE;
    bt_usage_key_request keys;
    char request[TA_PATH_MAX];
    int has_request = 0;
    step(&tour, &ctx, &keys, request, &has_request);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_DONE);
    BT_CHECK_EQ(tour.index, (size_t)0);
    BT_CHECK(!keys.has_key);
    bt_app_ctx_free(&ctx);
}

BT_TEST(usage, tour_init_wires_the_seeds_and_counts_the_shots) {
    bt_usage_tour tour;
    const char *dir = ta_temp_dir("usage", "plugin-tour");
    BT_CHECK_EQ(bt_usage_tour_init(&tour, dir, 5, 800), 0);
    BT_CHECK_EQ(bt_view_memo_app()->seed, (uint64_t)5);
    BT_CHECK_EQ(bt_view_solve_app()->seed, (uint64_t)6);
    BT_CHECK_EQ(bt_view_drill_app()->seed, (uint64_t)7);
    BT_CHECK_EQ(tour.expected_shots, (size_t)19);
    BT_CHECK_EQ(tour.index, (size_t)0);
    BT_CHECK_EQ(tour.mode, BT_USAGE_MODE_ACT);
    BT_CHECK_EQ(tour.shot_count, (size_t)0);
    bt_usage_tour fatal;
    const char *fatal_dir = ta_temp_dir("usage", "plugin-fatal");
    BT_CHECK_EQ(bt_usage_fatal_init(&fatal, fatal_dir), 0);
    BT_CHECK_EQ(fatal.expected_shots, (size_t)1);
    BT_CHECK_EQ(fatal.action_count, (size_t)3);
}

BT_TEST(usage, finish_requires_every_expected_shot_written) {
    bt_usage_tour tour;
    tour_world(&tour, ta_temp_dir("usage", "finish-files"), NULL, 0, 2);
    strcpy(tour.shots[0], tour.dir);
    strcat(tour.shots[0], "/menu.png");
    ta_write_file(tour.shots[0], "png");
    tour.shot_count = 1;
    BT_CHECK_EQ(bt_usage_finish(&tour), 1);
    strcpy(tour.shots[1], tour.dir);
    strcat(tour.shots[1], "/stats.png");
    ta_write_file(tour.shots[1], "png");
    tour.shot_count = 2;
    BT_CHECK_EQ(bt_usage_finish(&tour), 0);
    bt_usage_tour_free(&tour);
}

BT_TEST(usage, tour_storage_holds_more_than_the_fixed_capacity) {
    bt_usage_tour tour;
    memset(&tour, 0, sizeof tour);
    snprintf(tour.dir, sizeof tour.dir, "%s", ta_temp_dir("usage", "grow"));
    bt_usage_action actions[125];
    for (size_t i = 0; i < 125; i++) {
        memset(&actions[i], 0, sizeof actions[i]);
        actions[i].kind = BT_USAGE_ACT_ELAPSE;
        actions[i].secs = 0.1F;
    }
    bt_usage_tour_set_actions(&tour, actions, 125);
    BT_CHECK_EQ(tour.action_count, (size_t)125);
    BT_CHECK_EQ(tour.actions[124].kind, BT_USAGE_ACT_ELAPSE);
    bt_usage_action extra[1] = {0};
    extra[0].kind = BT_USAGE_ACT_SHOT;
    extra[0].name = "tail";
    bt_usage_tour_set_actions(&tour, extra, 1);
    BT_CHECK_EQ(tour.action_count, (size_t)1);
    BT_CHECK_EQ(tour.actions[0].kind, BT_USAGE_ACT_SHOT);
    bt_usage_tour_free(&tour);
    BT_CHECK(tour.actions == NULL);
    BT_CHECK_EQ(tour.action_count, (size_t)0);
}
