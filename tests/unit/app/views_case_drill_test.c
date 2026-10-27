#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/state.h"

#include "common.h"

#include "sqlite3.h"

enum {
    KEY_SPACE = 32,
    KEY_ONE = 49,
    KEY_FOUR = 52,
    KEY_D = 68,
    KEY_RIGHT = 262,
    KEY_LEFT = 263,
    KEY_DOWN = 264,
};

static const char *REGISTRY_TEXT =
    "[{\"file\":\"cll.tsv\",\"event\":\"2x2\",\"method\":\"cll\",\"stage\":\"cll\",\"n\":null,"
    "\"expected_rows\":2},{\"file\":\"bld.tsv\",\"event\":\"3bld\",\"method\":\"3style\","
    "\"stage\":\"parity\",\"n\":null,\"expected_rows\":1}]";

static const char *PACK_HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at\n";

static bt_app_ctx drill_fixture(const char *tag) {
    const char *base = ta_temp_dir("views_case_drill", tag);
    char packs[BT_PATH_MAX];
    char user[BT_PATH_MAX];
    snprintf(packs, sizeof packs, "%s\\vendor\\packs", base);
    snprintf(user, sizeof user, "%s\\user", base);
    char registry[BT_PATH_MAX];
    snprintf(registry, sizeof registry, "%s\\registry.json", packs);
    ta_write_file(registry, REGISTRY_TEXT);
    char cll[BT_PATH_MAX];
    snprintf(cll, sizeof cll, "%s\\cll.tsv", packs);
    ta_write_file(cll, "case_key\talg\tfront_json\tsource_url\tsource_at\n"
                       "cll.1\tR U R'\t{}\thttps://example.test/pack\t2026-01-01\n"
                       "cll.2\tF R F'\t{}\thttps://example.test/pack\t2026-01-01\n");
    char bld[BT_PATH_MAX];
    snprintf(bld, sizeof bld, "%s\\bld.tsv", packs);
    ta_write_file(bld, "case_key\talg\tfront_json\tsource_url\tsource_at\n"
                       "par.1\tR U R'\t{}\thttps://example.test/pack\t2026-01-01\n");

    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    ctx.state = BT_APP_ALGS;
    snprintf(ctx.paths.user_root, sizeof ctx.paths.user_root, "%s", user);
    snprintf(ctx.paths.packs_root, sizeof ctx.paths.packs_root, "%s", packs);
    bt_app_config_defaults(&ctx.config);
    bt_settings defaults;
    memset(&defaults, 0, sizeof defaults);
    char settings_error[128];
    BT_CHECK_EQ(
        bt_app_default_settings(&ctx.config, &defaults, settings_error, sizeof settings_error), 0);
    char error[256];
    BT_CHECK_EQ(bt_stores_load_with_packs(user, defaults, &ctx.fsrs, packs, &ctx.stores, error,
                                          sizeof error),
                0);
    ctx.has_stores = 1;
    ctx.has_corpus = 1;
    bt_corpus_init(&ctx.vendor_corpus);
    return ctx;
}

static void open_algs(bt_app_ctx *ctx) {
    bt_view_algs_enter(ctx);
}

static void open_case_drill(bt_app_ctx *ctx) {
    bt_view_algs_key(ctx, KEY_D);
    if (ctx->state == BT_APP_CASE_DRILL) {
        bt_view_case_drill_enter(ctx);
    }
}

static void walk_to_case(bt_app_ctx *ctx, const char *case_key) {
    bt_view_algs_key(ctx, KEY_RIGHT);
    bt_view_algs_key(ctx, KEY_DOWN);
    for (int i = 0; i < 3; i++) {
        bt_view_algs_key(ctx, KEY_RIGHT);
    }
    const bt_repo_node *walked = bt_repo_browser_node(bt_view_algs_repo());
    fprintf(stderr, "dbg arms=%zu arm=%zu depth=%zu focused=%d label=%s\\n",
            bt_view_algs_repo()->arm_count, bt_view_algs_repo()->arm, bt_view_algs_repo()->depth,
            bt_view_algs_repo()->repo_focused, walked != NULL ? walked->label : "null");
    BT_CHECK(strcmp(walked->label, case_key) == 0);
}

static void walk_to_3bld_case(bt_app_ctx *ctx) {
    bt_view_algs_key(ctx, KEY_RIGHT);
    bt_view_algs_key(ctx, KEY_DOWN);
    bt_view_algs_key(ctx, KEY_DOWN);
    BT_CHECK(strcmp(bt_view_algs_repo()->arms[bt_view_algs_repo()->arm].label, "3bld") == 0);
    for (int i = 0; i < 3; i++) {
        bt_view_algs_key(ctx, KEY_RIGHT);
    }
    BT_CHECK_EQ(bt_repo_browser_node(bt_view_algs_repo())->kind, BT_REPO_CASE);
}

static void grade_current(bt_app_ctx *ctx, int key) {
    bt_view_case_drill_key(ctx, bt_view_case_drill_app(), KEY_SPACE);
    BT_CHECK(ctx->case_drill_phase == BT_CASE_DRILL_GRADE);
    bt_view_case_drill_key(ctx, bt_view_case_drill_app(), key);
}

BT_TEST(views_case_drill, d_on_a_repo_case_opens_the_case_drill_in_show_phase) {
    bt_app_ctx ctx = drill_fixture("open");
    open_algs(&ctx);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    BT_CHECK(ctx.case_drill_phase == BT_CASE_DRILL_SHOW);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK(app->session.has_current);
    BT_CHECK(strcmp(app->session.current.case_ref.event, "2x2") == 0);
    BT_CHECK(strcmp(app->session.current.case_ref.method, "cll") == 0);
    BT_CHECK(strcmp(app->session.current.case_ref.stage, "cll") == 0);
    BT_CHECK(strcmp(app->session.current.case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(app->session.current.alg, "R U R'") == 0);
    BT_CHECK_EQ(app->session.total, (size_t)1);
    BT_CHECK(app->session.has_grid);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, grading_persists_the_card_and_returns_to_the_browser) {
    bt_app_ctx ctx = drill_fixture("grade");
    open_algs(&ctx);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    grade_current(&ctx, KEY_ONE + 2);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    bt_case_ref key;
    bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    const bt_card *card = bt_case_cards_get(&app->cards.cards, &key);
    BT_CHECK(card != NULL);
    BT_CHECK_EQ(card->reps, (unsigned int)1);
    BT_CHECK_EQ(card->review_count, (size_t)1);
    BT_CHECK_EQ(card->reviews[0].quality, (unsigned int)4);

    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx.paths.user_root, db_path);
    BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
    bt_case_cards stored;
    bt_srs_load_error load_err;
    BT_CHECK_EQ(bt_repo_cards_load(&db, &ctx.fsrs, &stored, &load_err), 0);
    const bt_card *stored_card = bt_case_cards_get(&stored, &key);
    BT_CHECK(stored_card != NULL);
    BT_CHECK_EQ(stored_card->reps, (unsigned int)1);
    bt_case_cards_free(&stored);
    bt_db_close(&db);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, due_sessions_cover_the_event_and_skip_recently_graded_cases) {
    bt_app_ctx ctx = drill_fixture("due");
    open_algs(&ctx);
    bt_view_algs_key(&ctx, KEY_RIGHT);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK(strcmp(bt_view_algs_repo()->arms[bt_view_algs_repo()->arm].label, "2x2") == 0);
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK_EQ(app->session.total, (size_t)2);
    BT_CHECK(strcmp(app->session.current.case_ref.case_key, "cll.1") == 0);
    grade_current(&ctx, KEY_FOUR);
    BT_CHECK(ctx.case_drill_phase == BT_CASE_DRILL_SHOW);
    BT_CHECK(strcmp(app->session.current.case_ref.case_key, "cll.2") == 0);
    grade_current(&ctx, KEY_FOUR);
    BT_CHECK(ctx.state == BT_APP_ALGS);

    bt_view_algs_key(&ctx, 256 /* escape */);
    bt_app_key_event escape;
    memset(&escape, 0, sizeof escape);
    escape.kind = BT_APP_KEY_ESCAPE;
    escape.pressed = 1;
    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_MENU);
    open_algs(&ctx);
    bt_view_algs_key(&ctx, KEY_RIGHT);
    bt_view_algs_key(&ctx, KEY_DOWN);
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    BT_CHECK_EQ(app->session.total, (size_t)2);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, a_stores_save_never_wipes_the_case_cards) {
    bt_app_ctx ctx = drill_fixture("persist");
    open_algs(&ctx);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    grade_current(&ctx, KEY_ONE + 2);
    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx.paths.user_root, db_path);
    BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
    bt_case_cards stored;
    bt_srs_load_error load_err;
    BT_CHECK_EQ(bt_repo_cards_load(&db, &ctx.fsrs, &stored, &load_err), 0);
    BT_CHECK_EQ(bt_case_cards_len(&stored), (size_t)1);
    bt_case_cards_free(&stored);
    bt_db_close(&db);
    BT_CHECK_EQ(bt_app_save_stores(&ctx), 0);
    BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
    BT_CHECK_EQ(bt_repo_cards_load(&db, &ctx.fsrs, &stored, &load_err), 0);
    BT_CHECK_EQ(bt_case_cards_len(&stored), (size_t)1);
    bt_case_cards_free(&stored);
    bt_db_close(&db);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, letters_d_keeps_the_3bld_drill_and_the_3bld_arm_stays_inert) {
    bt_app_ctx ctx = drill_fixture("letters");
    open_algs(&ctx);
    BT_CHECK(!bt_view_algs_repo()->repo_focused);
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_DRILL);
    bt_view_drill_enter(&ctx);
    bt_drill_app_state *drill = bt_view_drill_app();
    BT_CHECK(drill->session.has_pin);

    bt_app_key_event escape;
    memset(&escape, 0, sizeof escape);
    escape.kind = BT_APP_KEY_ESCAPE;
    escape.pressed = 1;
    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    ctx.state = BT_APP_ALGS;
    bt_view_algs_enter(&ctx);
    walk_to_3bld_case(&ctx);
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_ALGS);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK_EQ(app->pending.count, (size_t)0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, an_override_replaces_the_seed_alg_in_the_drill) {
    bt_app_ctx ctx = drill_fixture("override");
    {
        bt_db db;
        bt_db_error err;
        char db_path[BT_PATH_MAX];
        bt_data_db_path_at(ctx.paths.user_root, db_path);
        BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
        bt_case_ref key;
        bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.1");
        BT_CHECK_EQ(bt_repo_user_upsert(&db, &key, "F R F'", &err), 0);
        bt_db_close(&db);
    }
    open_algs(&ctx);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK(strcmp(app->session.current.seed_alg, "R U R'") == 0);
    BT_CHECK(strcmp(app->session.current.alg, "F R F'") == 0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, escape_from_the_case_drill_returns_to_the_menu) {
    bt_app_ctx ctx = drill_fixture("escape");
    open_algs(&ctx);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    bt_app_key_event escape;
    memset(&escape, 0, sizeof escape);
    escape.kind = BT_APP_KEY_ESCAPE;
    escape.pressed = 1;
    bt_app_push_key(&ctx, escape);
    bt_app_process_keys(&ctx);
    bt_app_apply_pending(&ctx);
    BT_CHECK(ctx.state == BT_APP_MENU);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, d_on_the_letters_list_after_leaving_the_repo_never_opens_a_case_drill) {
    bt_app_ctx ctx = drill_fixture("letterslist");
    open_algs(&ctx);
    bt_view_algs_key(&ctx, KEY_RIGHT);
    bt_view_algs_key(&ctx, KEY_DOWN);
    BT_CHECK(strcmp(bt_view_algs_repo()->arms[bt_view_algs_repo()->arm].label, "2x2") == 0);
    bt_view_algs_key(&ctx, KEY_LEFT);
    BT_CHECK(!bt_view_algs_repo()->repo_focused);
    BT_CHECK_EQ(bt_view_algs_repo()->arm, (size_t)1);
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_DRILL);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK_EQ(app->pending.count, (size_t)0);
    bt_app_ctx_free(&ctx);
}

BT_TEST(views_case_drill, a_failed_cards_load_reports_on_entry_and_blocks_grading) {
    bt_app_ctx ctx = drill_fixture("loadfail");
    {
        bt_db db;
        bt_db_error err;
        char db_path[BT_PATH_MAX];
        bt_data_db_path_at(ctx.paths.user_root, db_path);
        BT_CHECK_EQ(bt_db_open_at(db_path, &db, &err), 0);
        bt_case_ref key;
        bt_case_ref_of(&key, "2x2", "cll", "cll", "cll.2");
        bt_card card;
        bt_card_init(&card, 0);
        BT_CHECK_EQ(bt_repo_cards_save_card(&db, &key, &card, &err), 0);
        sqlite3_exec(bt_db_conn(&db), "UPDATE srs_state SET stability = -1.0", NULL, NULL, NULL);
        bt_db_close(&db);
    }
    open_algs(&ctx);
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    BT_CHECK(app->cards.has_load_error);
    walk_to_case(&ctx, "cll.1");
    open_case_drill(&ctx);
    BT_CHECK(ctx.state == BT_APP_CASE_DRILL);
    BT_CHECK(app->session.has_save_error);
    bt_view_case_drill_key(&ctx, app, KEY_SPACE);
    bt_view_case_drill_key(&ctx, app, KEY_ONE + 2);
    BT_CHECK_EQ(app->session.done, (size_t)0);
    BT_CHECK(strcmp(app->session.current.case_ref.case_key, "cll.1") == 0);
    bt_app_ctx_free(&ctx);
}
