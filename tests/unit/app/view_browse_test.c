#include "app/persist.h"
#include "app/views/algs/browse.h"
#include "data/seedpack/mod.h"

#include "common.h"

static char *dup_printf(const char *fmt, const char *arg) {
    char buf[128];
    snprintf(buf, sizeof buf, fmt, arg);
    char *out = malloc(strlen(buf) + 1);
    if (out == NULL) {
        BT_ABORT("dup_printf out of memory");
    }
    strcpy(out, buf);
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
    if (!sized) {
        out->n.kind = BT_NSPEC_UNSET;
        return;
    }
    out->n.kind = BT_NSPEC_SIZES;
    out->n.size_count = 4;
    out->n.sizes[0] = 4;
    out->n.sizes[1] = 5;
    out->n.sizes[2] = 6;
    out->n.sizes[3] = 7;
}

static void fixture_packs(bt_pack_entry *out) {
    make_pack("2x2", "cll", "cll", 0, &out[0]);
    make_pack("bigcube", "yau", "parity", 1, &out[1]);
    make_pack("bigcube", "reduction", "parity", 1, &out[2]);
    make_pack("sq1", "lbl", "cubeshape", 0, &out[3]);
    memset(&out[2].n, 0, sizeof out[2].n);
    out[2].n.kind = BT_NSPEC_SIZES;
    out[2].n.size_count = 3;
    out[2].n.sizes[0] = 5;
    out[2].n.sizes[1] = 6;
    out[2].n.sizes[2] = 7;
}

static void packs_fixed(bt_pack_entry *out) {
    make_pack("bigcube", "yau", "parity", 1, &out[0]);
    memset(&out[0].n, 0, sizeof out[0].n);
    out[0].n.kind = BT_NSPEC_SIZES;
    out[0].n.size_count = 2;
    out[0].n.sizes[0] = 4;
    out[0].n.sizes[1] = 5;
    memset(&out[1], 0, sizeof out[1]);
    snprintf(out[1].file, sizeof out[1].file, "%s", "bigcube_free_parity.tsv");
    snprintf(out[1].event, sizeof out[1].event, "%s", "bigcube");
    snprintf(out[1].method, sizeof out[1].method, "%s", "free");
    snprintf(out[1].stage, sizeof out[1].stage, "%s", "parity");
    out[1].expected_rows = 1;
    out[1].n.kind = BT_NSPEC_FIXED;
    out[1].n.fixed = 4;
}

static void make_row(const char *event, const char *method, const char *stage, const char *case_key,
                     bt_seed_row *out) {
    memset(out, 0, sizeof *out);
    snprintf(out->event, sizeof out->event, "%s", event);
    snprintf(out->method, sizeof out->method, "%s", method);
    snprintf(out->stage, sizeof out->stage, "%s", stage);
    snprintf(out->case_key, sizeof out->case_key, "%s", case_key);
    out->alg = dup_printf("alg %s", case_key);
    out->front_json = dup_printf("%s", "{}");
    out->source_url = dup_printf("%s", "https://example.test");
    out->source_at = dup_printf("%s", "2026-01-01");
}

static void free_rows(bt_seed_row *rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(rows[i].alg);
        free(rows[i].front_json);
        free(rows[i].source_url);
        free(rows[i].source_at);
    }
}

static void fixture_rows(bt_seed_row *out) {
    make_row("2x2", "cll", "cll", "cll.1", &out[0]);
    make_row("2x2", "cll", "cll", "cll.2", &out[1]);
    make_row("bigcube", "yau", "parity", "par.1", &out[2]);
    make_row("bigcube", "reduction", "parity", "red.1", &out[3]);
}

static bt_repo_browser armed_browser(bt_seed_row *rows) {
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

static const char *child_label(const bt_repo_node *node, size_t index) {
    return node->children[index].label;
}

BT_TEST(view_browse, arms_catalog_letters_first_then_registry_events_excluding_hidden) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    BT_CHECK_EQ(browser.arm_count, (size_t)3);
    BT_CHECK(strcmp(browser.arms[0].label, "letters") == 0);
    BT_CHECK_EQ(browser.arms[0].kind, BT_REPO_LETTERS);
    BT_CHECK(strcmp(browser.arms[1].label, "2x2") == 0);
    BT_CHECK(strcmp(browser.arms[2].label, "bigcube") == 0);
    for (size_t arm = 0; arm < browser.arm_count; arm++) {
        BT_CHECK(strcmp(browser.arms[arm].label, "sq1") != 0);
    }
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, plain_events_hold_methods_then_stages_then_cases) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    const bt_repo_node *two_by = &browser.arms[1];
    BT_CHECK_EQ(two_by->children_count, (size_t)1);
    BT_CHECK(strcmp(child_label(two_by, 0), "cll") == 0);
    BT_CHECK_EQ(two_by->children[0].kind, BT_REPO_METHOD);
    const bt_repo_node *stage = &two_by->children[0].children[0];
    BT_CHECK(strcmp(stage->label, "cll") == 0);
    BT_CHECK_EQ(stage->kind, BT_REPO_STAGE);
    BT_CHECK_EQ(stage->children_count, (size_t)2);
    BT_CHECK(strcmp(stage->children[0].label, "cll.1") == 0);
    BT_CHECK(strcmp(stage->children[1].label, "cll.2") == 0);
    BT_CHECK_EQ(stage->children[0].kind, BT_REPO_CASE);
    BT_CHECK(stage->children[0].has_case);
    BT_CHECK(strcmp(stage->children[0].case_alg, "alg cll.1") == 0);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, bigcube_expands_sorted_sizes_with_covering_methods_in_registry_order) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    const bt_repo_node *bigcube = &browser.arms[2];
    BT_CHECK_EQ(bigcube->children_count, (size_t)4);
    BT_CHECK(strcmp(bigcube->children[0].label, "4x4") == 0);
    BT_CHECK(strcmp(bigcube->children[3].label, "7x7") == 0);
    for (size_t size = 0; size < 4; size++) {
        BT_CHECK_EQ(bigcube->children[size].kind, BT_REPO_SIZE);
    }
    BT_CHECK_EQ(bigcube->children[0].children_count, (size_t)1);
    BT_CHECK(strcmp(child_label(&bigcube->children[0], 0), "yau") == 0);
    BT_CHECK_EQ(bigcube->children[1].children_count, (size_t)2);
    BT_CHECK(strcmp(child_label(&bigcube->children[1], 0), "yau") == 0);
    BT_CHECK(strcmp(child_label(&bigcube->children[1], 1), "reduction") == 0);
    const bt_repo_node *red_case = &bigcube->children[1].children[1].children[0].children[0];
    BT_CHECK(strcmp(red_case->label, "red.1") == 0);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, fixed_sizes_join_the_size_level_of_a_sized_event) {
    bt_pack_entry packs[2];
    packs_fixed(packs);
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    browser.arms = bt_repo_arms(packs, 2, &hidden, NULL, 0, &browser.arm_count);
    bt_str_list_free(&hidden);
    const bt_repo_node *bigcube = &browser.arms[1];
    BT_CHECK_EQ(bigcube->children_count, (size_t)2);
    BT_CHECK(strcmp(bigcube->children[0].label, "4x4") == 0);
    BT_CHECK(strcmp(bigcube->children[1].label, "5x5") == 0);
    BT_CHECK_EQ(bigcube->children[0].children_count, (size_t)2);
    BT_CHECK(strcmp(child_label(&bigcube->children[0], 0), "yau") == 0);
    BT_CHECK(strcmp(child_label(&bigcube->children[0], 1), "free") == 0);
    BT_CHECK_EQ(bigcube->children[1].children_count, (size_t)1);
    BT_CHECK(strcmp(child_label(&bigcube->children[1], 0), "yau") == 0);
    bt_repo_browser_free(&browser);
}

BT_TEST(view_browse, empty_registry_leaves_the_letters_arm_alone) {
    bt_str_list hidden;
    bt_str_list_init(&hidden);
    bt_str_list_push(&hidden, "2x2");
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    browser.arms = bt_repo_arms(NULL, 0, &hidden, NULL, 0, &browser.arm_count);
    bt_str_list_free(&hidden);
    BT_CHECK_EQ(browser.arm_count, (size_t)1);
    BT_CHECK(strcmp(browser.arms[0].label, "letters") == 0);
    bt_repo_browser_free(&browser);
}

BT_TEST(view_browse, right_from_letters_enters_the_repo_only_when_seed_arms_exist) {
    bt_repo_browser browser;
    bt_repo_browser_default(&browser);
    bt_repo_deeper(&browser);
    BT_CHECK(!browser.repo_focused);

    bt_seed_row rows[4];
    bt_repo_browser armed = armed_browser(rows);
    bt_repo_deeper(&armed);
    BT_CHECK(armed.repo_focused);
    BT_CHECK_EQ(armed.arm, (size_t)0);
    BT_CHECK_EQ(armed.depth, (size_t)0);
    bt_repo_browser_free(&armed);
    free_rows(rows, 4);
}

BT_TEST(view_browse, navigation_walks_the_full_bigcube_cascade) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    bt_repo_deeper(&browser);
    bt_repo_move(&browser, 2);
    BT_CHECK(strcmp(browser.arms[browser.arm].label, "bigcube") == 0);
    bt_repo_deeper(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)1);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "4x4") == 0);
    bt_repo_move(&browser, 1);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "5x5") == 0);
    bt_repo_deeper(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)2);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "yau") == 0);
    bt_repo_deeper(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)3);
    BT_CHECK(strcmp(bt_repo_browser_node(&browser)->label, "parity") == 0);
    bt_repo_deeper(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)4);
    BT_CHECK_EQ(bt_repo_browser_node(&browser)->kind, BT_REPO_CASE);
    bt_repo_deeper(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)4);
    bt_repo_shallower(&browser);
    bt_repo_shallower(&browser);
    bt_repo_shallower(&browser);
    bt_repo_shallower(&browser);
    BT_CHECK_EQ(browser.depth, (size_t)0);
    bt_repo_shallower(&browser);
    BT_CHECK(!browser.repo_focused);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, arm_selection_wraps_and_right_on_letters_returns_letters_focus) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    bt_repo_deeper(&browser);
    for (int i = 0; i < 3; i++) {
        bt_repo_move(&browser, 1);
    }
    BT_CHECK_EQ(browser.arm, (size_t)0);
    bt_repo_deeper(&browser);
    BT_CHECK(!browser.repo_focused);
    BT_CHECK_EQ(browser.depth, (size_t)0);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, list_rows_mark_the_current_selection_with_a_caret) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    bt_repo_deeper(&browser);
    bt_repo_list_row list_rows[16];
    size_t count = bt_repo_list_rows(&browser, 16, list_rows);
    BT_CHECK_EQ(count, (size_t)3);
    BT_CHECK(strcmp(list_rows[0].label, "> letters") == 0);
    BT_CHECK(list_rows[0].selected);
    BT_CHECK(strcmp(list_rows[1].label, "  2x2") == 0);
    BT_CHECK(!list_rows[1].selected);
    bt_repo_move(&browser, 2);
    bt_repo_deeper(&browser);
    bt_repo_deeper(&browser);
    bt_repo_deeper(&browser);
    bt_repo_deeper(&browser);
    count = bt_repo_list_rows(&browser, 16, list_rows);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(list_rows[0].label, "> par.1") == 0);
    BT_CHECK(list_rows[0].selected);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, breadcrumb_trails_the_walked_path) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    bt_repo_deeper(&browser);
    char crumb[256];
    bt_repo_breadcrumb(&browser, crumb, sizeof crumb);
    BT_CHECK(strcmp(crumb, "repo") == 0);
    bt_repo_move(&browser, 2);
    bt_repo_deeper(&browser);
    bt_repo_move(&browser, 1);
    bt_repo_deeper(&browser);
    bt_repo_breadcrumb(&browser, crumb, sizeof crumb);
    BT_CHECK(strcmp(crumb, "repo > bigcube > 5x5") == 0);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}

BT_TEST(view_browse, subtree_counts_feed_the_detail_summary) {
    bt_seed_row rows[4];
    bt_repo_browser browser = armed_browser(rows);
    const bt_repo_node *two_by = &browser.arms[1];
    BT_CHECK_EQ(bt_repo_count_kind(two_by, BT_REPO_METHOD), (size_t)1);
    BT_CHECK_EQ(bt_repo_count_kind(two_by, BT_REPO_STAGE), (size_t)1);
    BT_CHECK_EQ(bt_repo_count_kind(two_by, BT_REPO_CASE), (size_t)2);
    const bt_repo_node *bigcube = &browser.arms[2];
    BT_CHECK_EQ(bt_repo_count_kind(bigcube, BT_REPO_SIZE), (size_t)4);
    BT_CHECK_EQ(bt_repo_count_kind(bigcube, BT_REPO_METHOD), (size_t)7);
    BT_CHECK_EQ(bt_repo_count_kind(bigcube, BT_REPO_CASE), (size_t)7);
    bt_repo_browser_free(&browser);
    free_rows(rows, 4);
}
