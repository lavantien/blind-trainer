#include "app/persist.h"
#include "app/views/algs/browse.h"
#include "app/views/algs/detail.h"
#include "common/paths.h"
#include "common/space.h"
#include "data/repo_user.h"
#include "data/seedpack/mod.h"
#include "data/seedpack/parse.h"

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

static bt_repo_browser armed(bt_seed_row *rows) {
    bt_pack_entry packs[3];
    make_pack("2x2", "cll", "cll", 0, &packs[0]);
    make_pack("bigcube", "yau", "parity", 1, &packs[1]);
    make_pack("sq1", "lbl", "cubeshape", 0, &packs[2]);
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_str_list_push(&hidden, "sq1");
    make_row("2x2", "cll", "cll", "cll.1", &rows[0]);
    make_row("2x2", "cll", "cll", "cll.2", &rows[1]);
    make_row("bigcube", "yau", "parity", "par.1", &rows[2]);
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    browser.arms = bt_repo_arms(packs, 3, &hidden, rows, 3, &browser.arm_count);
    bt_str_list_free(&hidden);
    return browser;
}

static void bigcube_case_browser(bt_repo_browser *browser) {
    bt_repo_deeper(browser);
    bt_repo_move(browser, 2);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
    bt_repo_deeper(browser);
}

static size_t lines_of(const bt_repo_browser *browser, const bt_overrides *store,
                       const bt_view_override_draft *draft, bt_detail_line *out) {
    size_t count = 0;
    bt_view_repo_detail_lines(browser, store, draft, out, BT_DETAIL_LINES_MAX, &count);
    return count;
}

static void expect_line(const bt_detail_line *lines, size_t count, size_t at, bt_detail_kind kind,
                        const char *text) {
    BT_CHECK(at < count);
    BT_CHECK_EQ(lines[at].kind, kind);
    BT_CHECK(strcmp(lines[at].text, text) == 0);
}

BT_TEST(view_detail, letters_arm_detail_names_the_pair_browser) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bt_repo_deeper(&browser);
    BT_CHECK(browser.repo_focused);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 0, BT_DETAIL_META, "repo");
    expect_line(lines, count, 1, BT_DETAIL_TITLE, "letters");
    expect_line(lines, count, 2, BT_DETAIL_META, "the 3bld pair browser");
    BT_CHECK_EQ(count, (size_t)3);
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);
}

BT_TEST(view_detail, event_arm_detail_summarizes_its_subtree) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bt_repo_deeper(&browser);
    bt_repo_move(&browser, 1);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 0, BT_DETAIL_META, "repo");
    expect_line(lines, count, 1, BT_DETAIL_TITLE, "2x2");
    expect_line(lines, count, 2, BT_DETAIL_META, "methods 1 stages 1 cases 2");
    bt_repo_deeper(&browser);
    bt_repo_deeper(&browser);
    count = lines_of(&browser, &store, &draft, lines);
    BT_CHECK(strcmp(lines[count - 1].text, "cases 2") == 0);
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);
}

BT_TEST(view_detail, sized_event_summarizes_across_covering_packs) {
    bt_seed_row rows[4];
    bt_pack_entry packs[4];
    make_pack("2x2", "cll", "cll", 0, &packs[0]);
    make_pack("bigcube", "yau", "parity", 1, &packs[1]);
    make_pack("bigcube", "reduction", "parity", 1, &packs[2]);
    packs[2].n.size_count = 3;
    packs[2].n.sizes[0] = 5;
    packs[2].n.sizes[1] = 6;
    packs[2].n.sizes[2] = 7;
    make_pack("sq1", "lbl", "cubeshape", 0, &packs[3]);
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_str_list_push(&hidden, "sq1");
    make_row("2x2", "cll", "cll", "cll.1", &rows[0]);
    make_row("2x2", "cll", "cll", "cll.2", &rows[1]);
    make_row("bigcube", "yau", "parity", "par.1", &rows[2]);
    make_row("bigcube", "reduction", "parity", "red.1", &rows[3]);
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    browser.arms = bt_repo_arms(packs, 4, &hidden, rows, 4, &browser.arm_count);
    bt_str_list_free(&hidden);
    bt_repo_deeper(&browser);
    bt_repo_move(&browser, 2);
    bt_repo_deeper(&browser);
    bt_repo_move(&browser, 1);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 0, BT_DETAIL_META, "repo > bigcube");
    expect_line(lines, count, 1, BT_DETAIL_TITLE, "5x5");
    expect_line(lines, count, 2, BT_DETAIL_META, "methods 2 stages 2 cases 2");
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_detail, case_detail_shows_alg_seed_badge_and_source) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bigcube_case_browser(&browser);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "par.1") == 0);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 0, BT_DETAIL_META, "repo > bigcube > 4x4 > yau > parity");
    expect_line(lines, count, 1, BT_DETAIL_TITLE, "par.1");
    expect_line(lines, count, 2, BT_DETAIL_ALG, "alg par.1");
    expect_line(lines, count, 3, BT_DETAIL_META, "seed");
    BT_CHECK_EQ(lines[4].kind, BT_DETAIL_WARN);
    expect_line(lines, count, 5, BT_DETAIL_META, "source https://example.test 2026-01-01");
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);
}

BT_TEST(view_detail, letters_browser_keeps_no_repository_lines) {
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 0, BT_DETAIL_META, "repo");
    expect_line(lines, count, 1, BT_DETAIL_TITLE, "letters");
    expect_line(lines, count, 2, BT_DETAIL_META, "the 3bld pair browser");
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
}

BT_TEST(view_detail, case_site_walks_the_cascade_into_a_full_reference) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bigcube_case_browser(&browser);
    bt_case_site site;
    BT_CHECK(bt_view_case_site_of(&browser, &site));
    BT_CHECK(strcmp(site.case_ref.event, "bigcube") == 0);
    BT_CHECK(strcmp(site.case_ref.method, "yau") == 0);
    BT_CHECK(strcmp(site.case_ref.stage, "parity") == 0);
    BT_CHECK(strcmp(site.case_ref.case_key, "par.1") == 0);
    BT_CHECK(site.has_size);
    BT_CHECK_EQ(site.size, (long long)4);
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);

    bt_seed_row rows2[3];
    bt_repo_browser second = armed(rows2);
    bt_repo_deeper(&second);
    bt_repo_move(&second, 1);
    bt_repo_deeper(&second);
    bt_repo_deeper(&second);
    bt_repo_deeper(&second);
    BT_CHECK(bt_view_case_site_of(&second, &site));
    BT_CHECK(strcmp(site.case_ref.event, "2x2") == 0);
    BT_CHECK(strcmp(site.case_ref.method, "cll") == 0);
    BT_CHECK(strcmp(site.case_ref.stage, "cll") == 0);
    BT_CHECK(strcmp(site.case_ref.case_key, "cll.1") == 0);
    BT_CHECK(!site.has_size);
    bt_repo_browser_free(&second);
    free_rows(rows2, 3);
}

BT_TEST(view_detail, case_site_is_none_above_the_case_level) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bt_repo_deeper(&browser);
    bt_case_site site;
    BT_CHECK(!bt_view_case_site_of(&browser, &site));
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);
}

BT_TEST(view_detail, bld_case_expand_line_gates_on_event_and_cancellation) {
    char line[BT_ALGS_TEXT_MAX];
    BT_CHECK(bt_view_case_expand_line("3bld", "R R U", line));
    BT_CHECK(strcmp(line, "expand R2 U") == 0);
    BT_CHECK(!bt_view_case_expand_line("3bld", "R U", line));
    BT_CHECK(!bt_view_case_expand_line("2x2", "R R U", line));
    BT_CHECK(!bt_view_case_expand_line("3bld", "junk", line));
}

BT_TEST(view_detail, tier1_gate_accepts_clean_cube_algs_and_rejects_bad_ones) {
    char error[BT_ALGS_TEXT_MAX];
    BT_CHECK_EQ(bt_view_geometry_check("2x2", 0, 0, "R U R'", error, sizeof error), 0);
    BT_CHECK_EQ(bt_view_geometry_check("bigcube", 1, 5, "Rw U Rw'", error, sizeof error), 0);
    BT_CHECK(bt_view_geometry_check("3x3", 0, 0, "QQ", error, sizeof error) != 0);
    BT_CHECK_EQ(bt_view_geometry_check("bigcube", 0, 0, "R U R'", error, sizeof error) != 0, 1);
    BT_CHECK(bt_view_geometry_check("bigcube", 0, 0, "R U R'", error, sizeof error) != 0);
    BT_CHECK(strcmp(error, "no cube size in the browser path for a bigcube case") == 0);
    BT_CHECK(bt_view_geometry_check("clock", 0, 0, "R U R'", error, sizeof error) != 0);
    BT_CHECK(strcmp(error, "no tier 1 geometry for event clock") == 0);
}

BT_TEST(view_detail, cube_override_gate_accepts_the_bt_core_notation) {
    char error[BT_ALGS_TEXT_MAX];
    BT_CHECK_EQ(bt_view_geometry_check("3x3", 0, 0, "[R U R', M]", error, sizeof error), 0);
    BT_CHECK_EQ(bt_view_geometry_check("3bld", 0, 0, "(R U R')2", error, sizeof error), 0);
    BT_CHECK_EQ(bt_view_geometry_check("oh", 0, 0, "R U R'", error, sizeof error), 0);
    BT_CHECK_EQ(bt_view_geometry_check("3x3", 0, 0, "R R'", error, sizeof error), 0);
    BT_CHECK(bt_view_geometry_check("3x3", 0, 0, "[R U", error, sizeof error) != 0);
    BT_CHECK(strcmp(error, "alg does not parse in the bt-core notation") == 0);
    BT_CHECK(bt_view_geometry_check("3bld", 0, 0, "  ", error, sizeof error) != 0);
    BT_CHECK(strcmp(error, "alg is empty") == 0);
}

BT_TEST(view_detail, every_skewb_sarah_seed_row_passes_the_tier1_gate) {
    char ground[BT_PATH_MAX];
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    if (raw[0] == '/' || (raw[1] == ':' && (raw[2] == '\\' || raw[2] == '/'))) {
        snprintf(ground, BT_PATH_MAX, "%s", raw);
    } else {
        char root[BT_PATH_MAX];
        bt_paths_workspace_root(root);
        snprintf(ground, BT_PATH_MAX, "%s\\%s", root, raw);
    }
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
        char message[1024];
        bt_seed_error_string(&err, message, sizeof message);
        BT_ABORT("sarah pack load failed: %s", message);
    }
    BT_CHECK_EQ(rows.count, (size_t)15);
    char error[BT_ALGS_TEXT_MAX];
    for (size_t i = 0; i < rows.count; i++) {
        BT_CHECK_EQ(bt_view_geometry_check("skewb", 0, 0, rows.items[i].alg, error, sizeof error),
                    0);
    }
    bt_seed_rows_free(&rows);
}

BT_TEST(view_detail, a_skewb_override_with_only_rotations_is_rejected_as_empty) {
    char error[BT_ALGS_TEXT_MAX];
    BT_CHECK(bt_view_geometry_check("skewb", 0, 0, "y2 z x'", error, sizeof error) != 0);
    BT_CHECK(strcmp(error, "alg is empty after rotation stripping") == 0);
}

BT_TEST(view_detail, effective_alg_prefers_the_override_over_the_seed_row) {
    bt_case_ref case_ref;
    bt_case_ref_of(&case_ref, "2x2", "cll", "cll", "cll.1");
    bt_overrides store;
    bt_overrides_init(&store);
    bt_effective_alg effective = bt_view_effective_alg(&case_ref, "R U R'", &store);
    BT_CHECK(strcmp(effective.alg, "R U R'") == 0);
    BT_CHECK(!effective.overridden);
    bt_overrides_put(&store, &case_ref, "F R F'");
    effective = bt_view_effective_alg(&case_ref, "R U R'", &store);
    BT_CHECK(strcmp(effective.alg, "F R F'") == 0);
    BT_CHECK(effective.overridden);
    bt_overrides_free(&store);
}

BT_TEST(view_detail, case_detail_marks_the_override_and_warns_on_a_broken_effective_alg) {
    bt_seed_row rows[3];
    bt_repo_browser browser = armed(rows);
    bigcube_case_browser(&browser);
    bt_case_ref case_ref;
    bt_case_ref_of(&case_ref, "bigcube", "yau", "parity", "par.1");
    bt_overrides store;
    bt_overrides_init(&store);
    bt_view_override_draft draft;
    bt_view_override_draft_default(&draft);
    bt_overrides_put(&store, &case_ref, "R U R'");
    bt_detail_line lines[BT_DETAIL_LINES_MAX];
    size_t count = lines_of(&browser, &store, &draft, lines);
    expect_line(lines, count, 2, BT_DETAIL_ALG, "R U R'");
    expect_line(lines, count, 3, BT_DETAIL_META, "override");
    expect_line(lines, count, 4, BT_DETAIL_META, "source https://example.test 2026-01-01");

    bt_overrides_put(&store, &case_ref, "QQ");
    count = lines_of(&browser, &store, &draft, lines);
    BT_CHECK_EQ(lines[4].kind, BT_DETAIL_WARN);

    bt_case_site site;
    bt_view_case_site_of(&browser, &site);
    bt_view_override_draft editing;
    bt_view_override_draft_default(&editing);
    editing.has_case = 1;
    editing.site = site;
    snprintf(editing.text, sizeof editing.text, "%s", "F R F'");
    count = lines_of(&browser, &store, &editing, lines);
    int saw_editing = 0;
    int saw_draft_alg = 0;
    for (size_t i = 0; i < count; i++) {
        if (lines[i].kind == BT_DETAIL_META && strcmp(lines[i].text, "editing") == 0) {
            saw_editing = 1;
        }
        if (lines[i].kind == BT_DETAIL_ALG && strcmp(lines[i].text, "F R F'") == 0) {
            saw_draft_alg = 1;
        }
    }
    BT_CHECK(saw_editing);
    BT_CHECK(saw_draft_alg);
    bt_overrides_free(&store);
    bt_repo_browser_free(&browser);
    free_rows(rows, 3);
}
