#include "app/views/case_drill.h"
#include "app/views/state.h"
#include "puzzles/nxn.h"

#include "common.h"

static char *dup_text(const char *text) {
    char *out = malloc(strlen(text) + 1);
    if (out == NULL) {
        BT_ABORT("dup_text out of memory");
    }
    strcpy(out, text);
    return out;
}

static void make_pack(const char *event, const char *method, const char *stage, int sized,
                      bt_pack_entry *out) {
    memset(out, 0, sizeof *out);
    snprintf(out->file, sizeof out->file, "%s_%s_%s.tsv", event, method, stage);
    snprintf(out->event, sizeof out->event, "%s", event);
    snprintf(out->method, sizeof out->method, "%s", method);
    snprintf(out->stage, sizeof out->stage, "%s", stage);
    out->expected_rows = 1;
    out->n.kind = sized ? BT_NSPEC_SIZES : BT_NSPEC_UNSET;
    if (sized) {
        out->n.size_count = 4;
        out->n.sizes[0] = 4;
        out->n.sizes[1] = 5;
        out->n.sizes[2] = 6;
        out->n.sizes[3] = 7;
    }
}

static void make_row(const char *event, const char *method, const char *stage, const char *case_key,
                     bt_seed_row *out) {
    memset(out, 0, sizeof *out);
    snprintf(out->event, sizeof out->event, "%s", event);
    snprintf(out->method, sizeof out->method, "%s", method);
    snprintf(out->stage, sizeof out->stage, "%s", stage);
    snprintf(out->case_key, sizeof out->case_key, "%s", case_key);
    char alg[80];
    snprintf(alg, sizeof alg, "alg %s", case_key);
    out->alg = dup_text(alg);
    out->front_json = dup_text("{}");
    out->source_url = dup_text("https://example.test");
    out->source_at = dup_text("2026-01-01");
}

static void free_rows(bt_seed_row *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(rows[i].alg);
        free(rows[i].front_json);
        free(rows[i].source_url);
        free(rows[i].source_at);
    }
}

static void fixture_packs(bt_pack_entry *out) {
    make_pack("2x2", "cll", "cll", 0, &out[0]);
    make_pack("bigcube", "yau", "parity", 1, &out[1]);
    make_pack("bigcube", "reduction", "parity", 1, &out[2]);
    out[2].n.size_count = 3;
    out[2].n.sizes[0] = 5;
    out[2].n.sizes[1] = 6;
    out[2].n.sizes[2] = 7;
    make_pack("sq1", "lbl", "cubeshape", 0, &out[3]);
}

static void fixture_rows(bt_seed_row *out) {
    make_row("2x2", "cll", "cll", "cll.1", &out[0]);
    make_row("2x2", "cll", "cll", "cll.2", &out[1]);
    make_row("bigcube", "yau", "parity", "par.1", &out[2]);
    make_row("bigcube", "reduction", "parity", "red.1", &out[3]);
}

static bt_repo_browser armed(bt_seed_row *rows) {
    bt_pack_entry packs[4];
    fixture_packs(packs);
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_str_list_push(&hidden, "sq1");
    fixture_rows(rows);
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    browser.arms = bt_repo_arms(packs, 4, &hidden, rows, 4, &browser.arm_count);
    bt_str_list_free(&hidden);
    return browser;
}

static void at_bigcube_case(bt_repo_browser *browser) {
    bt_repo_deeper(browser);
    bt_repo_move(browser, 2);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
}

static void at_2x2_event(bt_repo_browser *browser) {
    bt_repo_deeper(browser);
    bt_repo_move(browser, 1);
    bt_repo_deeper(browser);
}

static int permutation_is(const char *event, const size_t *wanted, size_t count) {
    size_t out[BT_PZ_MAX_FACES];
    size_t got = bt_case_drill_face_permutation(event, out);
    if (got != count) {
        return 0;
    }
    return memcmp(out, wanted, count * sizeof *wanted) == 0;
}

static int remap_is(const char *event, const size_t *wanted, size_t count) {
    size_t out[16];
    size_t got = bt_case_drill_color_remap(event, out);
    if (got != count) {
        return 0;
    }
    return memcmp(out, wanted, count * sizeof *wanted) == 0;
}

static void uniform_grid(bt_pz_grid *grid, size_t face_count, size_t rows, size_t cols) {
    memset(grid, 0, sizeof *grid);
    grid->face_count = face_count;
    for (size_t face = 0; face < face_count; face++) {
        grid->faces[face].row_count = rows;
        for (size_t row = 0; row < rows; row++) {
            grid->faces[face].rows[row].cols = cols;
            for (size_t col = 0; col < cols; col++) {
                grid->faces[face].rows[row].cells[col] = (unsigned char)face;
            }
        }
    }
}

static void solved_pattern_grid(bt_pz_grid *grid, size_t face_count) {
    static const unsigned char pattern[3][3] = {{0, 1, 2}, {1, 0, 1}, {2, 1, 0}};
    memset(grid, 0, sizeof *grid);
    grid->face_count = face_count;
    for (size_t face = 0; face < face_count; face++) {
        grid->faces[face].row_count = 3;
        for (size_t row = 0; row < 3; row++) {
            grid->faces[face].rows[row].cols = 3;
            for (size_t col = 0; col < 3; col++) {
                grid->faces[face].rows[row].cells[col] = pattern[row][col];
            }
        }
    }
}

BT_TEST(view_case_drill, face_permutations_follow_the_ledger_orders) {
    BT_CHECK(permutation_is("3x3", (const size_t[]){0, 1, 2, 3, 4, 5}, 6));
    BT_CHECK(permutation_is("skewb", (const size_t[]){0, 3, 1, 2, 4, 5}, 6));
    BT_CHECK(permutation_is("fto", (const size_t[]){0, 6, 1, 2, 4, 3, 5, 7}, 8));
    BT_CHECK(permutation_is("pyraminx", (const size_t[]){0, 1, 2, 3}, 4));
    BT_CHECK(
        permutation_is("megaminx", (const size_t[]){0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12));
    BT_CHECK(permutation_is("sq1", (const size_t[]){0, 1}, 2));
    BT_CHECK_EQ(bt_case_drill_face_permutation("3bld", NULL), (size_t)0);
    BT_CHECK_EQ(bt_case_drill_face_permutation("clock", NULL), (size_t)0);
}

BT_TEST(view_case_drill, color_remaps_send_each_face_name_to_its_palette_slot) {
    BT_CHECK(remap_is("3x3", (const size_t[]){0, 1, 2, 3, 4, 5}, 6));
    BT_CHECK(remap_is("skewb", (const size_t[]){0, 2, 3, 1, 4, 5}, 6));
    BT_CHECK(remap_is("fto", (const size_t[]){0, 2, 3, 6, 4, 7, 1, 5}, 8));
    BT_CHECK(remap_is("pyraminx", (const size_t[]){2, 1, 3, 5}, 4));
    BT_CHECK(remap_is("megaminx", (const size_t[]){0, 2, 3, 6, 7, 1, 5, 4, 8, 9, 10, 11}, 12));
    BT_CHECK(remap_is("sq1", (const size_t[]){0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5}, 16));
    BT_CHECK_EQ(bt_case_drill_color_remap("clock", NULL), (size_t)0);
}

BT_TEST(view_case_drill, themed_grid_identity_colors_a_solved_skewb) {
    bt_pz_grid grid;
    uniform_grid(&grid, 6, 2, 2);
    bt_pz_grid themed;
    BT_CHECK(bt_case_drill_themed_grid(&grid, "skewb", &themed));
    BT_CHECK_EQ(themed.face_count, (size_t)6);
    static const char *cube_faces[6] = {"U", "L", "F", "R", "B", "D"};
    for (size_t slot = 0; slot < 6; slot++) {
        unsigned char wanted = (unsigned char)bt_case_drill_face_color_index(cube_faces[slot]);
        for (size_t row = 0; row < 2; row++) {
            for (size_t col = 0; col < 2; col++) {
                BT_CHECK_EQ(themed.faces[slot].rows[row].cells[col], wanted);
            }
        }
    }
    BT_CHECK_EQ(themed.faces[1].rows[0].cells[0], (unsigned char)1);
}

BT_TEST(view_case_drill, themed_grid_keeps_sticker_payload_under_remap) {
    bt_pz_grid grid;
    solved_pattern_grid(&grid, 6);
    bt_pz_grid themed;
    BT_CHECK(bt_case_drill_themed_grid(&grid, "3x3", &themed));
    BT_CHECK(bt_pz_grid_eq(&themed, &grid));
    bt_pz_grid skewb;
    BT_CHECK(bt_case_drill_themed_grid(&grid, "skewb", &skewb));
    BT_CHECK_EQ(skewb.faces[0].rows[0].cells[0], (unsigned char)0);
    BT_CHECK_EQ(skewb.faces[0].rows[0].cells[1], (unsigned char)2);
    BT_CHECK_EQ(skewb.faces[0].rows[0].cells[2], (unsigned char)3);
    BT_CHECK_EQ(skewb.faces[2].rows[0].cells[0], (unsigned char)0);
    BT_CHECK_EQ(skewb.faces[2].rows[0].cells[1], (unsigned char)2);
    BT_CHECK_EQ(skewb.faces[2].rows[0].cells[2], (unsigned char)3);
}

BT_TEST(view_case_drill, every_skewb_sarah_seed_row_renders_its_case_net) {
    char ground[BT_PATH_MAX];
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    snprintf(ground, BT_PATH_MAX, "%s", raw);
    char packs[BT_PATH_MAX];
    snprintf(packs, BT_PATH_MAX, "%s\\packs", ground);
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", "skewb_sarah.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "skewb");
    snprintf(entry.method, sizeof entry.method, "%s", "sarah_intermediate");
    snprintf(entry.stage, sizeof entry.stage, "%s", "sarah");
    entry.expected_rows = 15;
    bt_seed_rows rows;
    bt_seed_error err;
    if (bt_seedpack_load_pack(packs, &entry, &rows, &err) != 0) {
        BT_ABORT("sarah pack load failed");
    }
    BT_CHECK_EQ(rows.count, (size_t)15);
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    for (size_t i = 0; i < rows.count; i++) {
        BT_CHECK(
            bt_case_drill_facelets("skewb", 0, 0, rows.items[i].alg, &grid, error, sizeof error));
        BT_CHECK_EQ(grid.face_count, (size_t)6);
    }
    bt_seed_rows_free(&rows);
}

BT_TEST(view_case_drill, a_skewb_case_of_only_rotations_reports_the_stripped_alg_as_empty) {
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    BT_CHECK(!bt_case_drill_facelets("skewb", 0, 0, "y2 z x'", &grid, error, sizeof error));
    BT_CHECK(strcmp(error, "alg is empty after rotation stripping") == 0);
}

BT_TEST(view_case_drill, case_facelets_applies_the_inverse_alg_to_solved) {
    bt_pz_grid shown;
    char error[BT_CASE_DRILL_TEXT_MAX];
    BT_CHECK(bt_case_drill_facelets("3x3", 0, 0, "R U R'", &shown, error, sizeof error));
    bt_pz_nxn nxn;
    BT_CHECK(bt_pz_nxn_init(&nxn, 3));
    bt_pz_engine engine = bt_pz_nxn_engine(&nxn);
    void *alg = bt_pz_alg_create();
    bt_pz_error pz_err;
    BT_CHECK(engine.parse_alg(&engine, "R U R'", strlen("R U R'"), alg, &pz_err));
    void *inverse = bt_pz_alg_create();
    engine.invert_alg(&engine, alg, inverse);
    bt_pz_nxn_state expected;
    engine.solved_state(&engine, &expected);
    BT_CHECK(engine.apply_alg(&engine, &expected, inverse, &pz_err));
    bt_pz_grid expected_grid;
    bt_pz_grid themed;
    engine.facelets(&engine, &expected, &expected_grid);
    BT_CHECK(bt_case_drill_themed_grid(&expected_grid, "3x3", &themed));
    BT_CHECK(bt_pz_grid_eq(&shown, &themed));
    bt_pz_nxn_state forward;
    engine.solved_state(&engine, &forward);
    BT_CHECK(engine.apply_alg(&engine, &forward, alg, &pz_err));
    bt_pz_grid forward_grid;
    engine.facelets(&engine, &forward, &forward_grid);
    bt_pz_grid forward_themed;
    BT_CHECK(bt_case_drill_themed_grid(&forward_grid, "3x3", &forward_themed));
    BT_CHECK(!bt_pz_grid_eq(&shown, &forward_themed));
    bt_pz_alg_destroy(inverse);
    bt_pz_alg_destroy(alg);
}

BT_TEST(view_case_drill, case_facelets_covers_every_pack_event_and_rejects_the_rest) {
    static const struct {
        const char *event;
        int has_size;
        long long size;
        const char *alg;
    } covered[] = {
        {"2x2", 0, 0, "R U R'"},    {"bigcube", 1, 5, "Rw U Rw'"}, {"skewb", 0, 0, "R F' L"},
        {"pyraminx", 0, 0, "R U"},  {"fto", 0, 0, "R U R'"},       {"sq1", 0, 0, "(1,0)/"},
        {"megaminx", 0, 0, "R U'"},
    };
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    for (size_t i = 0; i < sizeof covered / sizeof covered[0]; i++) {
        BT_CHECK(bt_case_drill_facelets(covered[i].event, covered[i].has_size, covered[i].size,
                                        covered[i].alg, &grid, error, sizeof error));
    }
    BT_CHECK(!bt_case_drill_facelets("clock", 0, 0, "R", &grid, error, sizeof error));
    BT_CHECK(strcmp(error, "no case geometry for event clock") == 0);
    BT_CHECK(!bt_case_drill_facelets("3bld", 0, 0, "R", &grid, error, sizeof error));
    BT_CHECK(strcmp(error, "no case geometry for event 3bld") == 0);
    BT_CHECK(!bt_case_drill_facelets("3x3", 0, 0, "QQ", &grid, error, sizeof error));
}

BT_TEST(view_case_drill, collect_cases_walks_the_subtree_with_the_site_reference) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed(rows);
    at_bigcube_case(&browser);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "par.1") == 0);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_drill_case cases[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_collect_cases(&browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases[0].case_ref.event, "bigcube") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.method, "yau") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.stage, "parity") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.case_key, "par.1") == 0);
    BT_CHECK(cases[0].has_size);
    BT_CHECK_EQ(cases[0].size, (long long)4);
    BT_CHECK(strcmp(cases[0].alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases[0].seed_alg, "alg par.1") == 0);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    count = bt_case_drill_collect_cases(&event_browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(cases[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(cases[1].case_ref.case_key, "cll.2") == 0);
    BT_CHECK(!cases[0].has_size && !cases[1].has_size);
    bt_overrides_free(&store);
    bt_repo_browser_free(&event_browser);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_case_drill, collect_cases_uses_the_override_as_the_effective_alg) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed(rows);
    at_bigcube_case(&browser);
    bt_case_ref case_ref;
    bt_case_ref_of(&case_ref, "bigcube", "yau", "parity", "par.1");
    bt_overrides store;
    bt_overrides_init(&store);
    bt_overrides_put(&store, &case_ref, "R U R'");
    bt_drill_case cases[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_collect_cases(&browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases[0].seed_alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases[0].alg, "R U R'") == 0);
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_case_drill, collect_cases_ignores_the_letters_and_3bld_arms) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed(rows);
    bt_repo_deeper(&browser);
    bt_overrides store;
    bt_overrides_init(&store);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&browser, &store, NULL), (size_t)0);

    bt_pack_entry packs[2];
    make_pack("3bld", "3style", "parity", 0, &packs[0]);
    make_pack("2x2", "cll", "cll", 0, &packs[1]);
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    make_row("3bld", "3style", "parity", "par.1", &rows[0]);
    make_row("2x2", "cll", "cll", "cll.1", &rows[1]);
    bt_repo_browser bld;
    bt_repo_browser_default(&bld);
    bld.arms = bt_repo_arms(packs, 2, &hidden, rows, 2, &bld.arm_count);
    bt_repo_deeper(&bld);
    bt_repo_move(&bld, 1);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&bld, &store, NULL), (size_t)0);
    bt_str_list_free(&hidden);
    bt_overrides_free(&store);
    bt_repo_browser_free(&bld);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

static bt_drill_case build_case(const char *key) {
    bt_drill_case out;
    memset(&out, 0, sizeof out);
    bt_case_ref_of(&out.case_ref, "2x2", "cll", "cll", key);
    return out;
}

BT_TEST(view_case_drill, due_session_prioritizes_missing_and_due_cards) {
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    bt_drill_case fresh = build_case("cll.1");
    bt_drill_case future = build_case("cll.2");
    bt_drill_case overdue = build_case("cll.3");
    bt_card scheduled;
    bt_card_init(&scheduled, 0);
    scheduled.due_day = 900;
    bt_case_ref future_ref = future.case_ref;
    bt_case_ref overdue_ref = overdue.case_ref;
    bt_drill_case queue_in[3] = {fresh, future, overdue};
    bt_case_cards_put(&cards, &future_ref, &scheduled);
    bt_card fresh_card;
    bt_card_init(&fresh_card, 0);
    bt_case_cards_put(&cards, &overdue_ref, &fresh_card);
    bt_drill_case queue_out[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_due_session(queue_in, 3, &cards, 1, queue_out);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(queue_out[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(queue_out[1].case_ref.case_key, "cll.3") == 0);
    bt_drill_case only_future[1] = {future};
    count = bt_case_drill_due_session(only_future, 1, &cards, 1, queue_out);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(queue_out[0].case_ref.case_key, "cll.2") == 0);
    bt_case_cards_free(&cards);
}

BT_TEST(view_case_drill, drill_request_drills_one_case_and_due_sessions_below_it) {
    bt_case_card_store cards;
    memset(&cards, 0, sizeof cards);
    bt_case_cards_init(&cards.cards);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = 0;

    bt_seed_row rows[4];
    bt_repo_browser case_browser = armed(rows);
    at_bigcube_case(&case_browser);
    BT_CHECK(bt_case_drill_request(&case_browser, &store, &cards, 0, out, &count));
    BT_CHECK_EQ(count, (size_t)1);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    BT_CHECK(bt_case_drill_request(&event_browser, &store, &cards, 0, out, &count));
    BT_CHECK_EQ(count, (size_t)2);

    bt_repo_browser letters = armed(rows);
    bt_repo_deeper(&letters);
    BT_CHECK(!bt_case_drill_request(&letters, &store, &cards, 0, out, &count));

    bt_overrides_free(&store);
    bt_case_cards_free(&cards.cards);
    bt_repo_browser_free(&letters);
    bt_repo_browser_free(&event_browser);
    bt_repo_browser_free(&case_browser);
    free_rows(rows, 4);
}

BT_TEST(view_case_drill, install_and_advance_run_the_session_queue) {
    bt_case_drill_session session;
    memset(&session, 0, sizeof session);
    bt_drill_case queue[2];
    queue[0] = build_case("pll.1");
    bt_case_ref_of(&queue[0].case_ref, "3x3", "cfop", "pll", "pll.1");
    snprintf(queue[0].seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    snprintf(queue[0].alg, BT_CASE_DRILL_TEXT_MAX, "%s", "R U R'");
    queue[1] = build_case("pll.2");
    bt_case_ref_of(&queue[1].case_ref, "3x3", "cfop", "pll", "pll.2");
    snprintf(queue[1].seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    snprintf(queue[1].alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    bt_case_drill_install(&session, queue, 2);
    BT_CHECK_EQ(session.total, (size_t)2);
    BT_CHECK_EQ(session.done, (size_t)0);
    BT_CHECK(session.has_current);
    BT_CHECK(strcmp(session.current.case_ref.case_key, "pll.1") == 0);
    BT_CHECK(session.has_grid);
    BT_CHECK(!session.has_grid_error);
    bt_case_drill_advance(&session);
    BT_CHECK(session.has_current);
    BT_CHECK(strcmp(session.current.case_ref.case_key, "pll.2") == 0);
    BT_CHECK(!session.has_grid);
    BT_CHECK(session.has_grid_error);
    bt_pz_grid grid;
    char error[BT_CASE_DRILL_TEXT_MAX];
    bt_case_drill_facelets("3x3", 0, 0, "QQ", &grid, error, sizeof error);
    BT_CHECK(strcmp(session.grid_error, error) == 0);
    bt_case_drill_advance(&session);
    BT_CHECK(!session.has_current);
    BT_CHECK(!session.has_grid);
    BT_CHECK(!session.has_grid_error);
}

BT_TEST(view_case_drill, case_site_line_spells_the_site_and_size) {
    bt_drill_case case_drill = build_case("par.1");
    bt_case_ref_of(&case_drill.case_ref, "bigcube", "yau", "parity", "par.1");
    case_drill.has_size = 1;
    case_drill.size = 5;
    char line[BT_CASE_DRILL_TEXT_MAX];
    bt_case_drill_case_site_line(&case_drill, line);
    BT_CHECK(strcmp(line, "bigcube > yau > parity 5x5") == 0);
}
