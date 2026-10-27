#include "verify_test_util.h"

#include <string.h>

#include "data/seedpack/registry.h"
#include "data/seedpack/rulings.h"
#include "verify/packs_report.h"

static verify_row_outcome outcome(const char *case_key, bool tier1_ok, const char *tier1_err,
                                  verify_tier2 tier2) {
    verify_row_outcome row = {0};
    snprintf(row.case_key, sizeof row.case_key, "%s", case_key);
    row.tier1_ok = tier1_ok;
    if (tier1_err != NULL) {
        snprintf(row.tier1_err, sizeof row.tier1_err, "%s", tier1_err);
    }
    row.tier2 = tier2;
    return row;
}

BT_TEST(verify_packs_report, an_unruled_failure_is_an_offender) {
    verify_row_outcome row = outcome("ep.99", false, "unexecutable slash sequence",
                                     verify_tier2_skipped("tier 1 failed"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "ep", &row, offender, sizeof offender), 1);
    BT_CHECK_EQ(offender, "sq1/lbl/ep/ep.99: tier1: unexecutable slash sequence");
}

BT_TEST(verify_packs_report, a_ruled_failure_is_excused) {
    verify_row_outcome row = outcome("cs.119", false, "unexecutable slash sequence: hand traced",
                                     verify_tier2_skipped("tier 1 failed"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "cubeshape", &row, offender, sizeof offender), 0);
}

BT_TEST(verify_packs_report, a_ruling_on_a_passing_row_is_stale) {
    verify_row_outcome row = outcome("cs.075", true, NULL, verify_tier2_pass());
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "cubeshape", &row, offender, sizeof offender), 1);
    BT_CHECK(strncmp(offender, "sq1/lbl/cubeshape/cs.075: stale ruling (", 39) == 0);
    size_t len = strlen(offender);
    BT_CHECK(strcmp(offender + len - 13, "), row passes") == 0);
}

BT_TEST(verify_packs_report, a_ruled_row_drifting_to_a_parse_failure_is_stale) {
    verify_row_outcome row =
        outcome("cs.119", false, "unknown token: q", verify_tier2_skipped("tier 1 failed"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "cubeshape", &row, offender, sizeof offender), 1);
    BT_CHECK(strstr(offender, "stale ruling") != NULL);
    BT_CHECK(strstr(offender, "parse") != NULL);
}

BT_TEST(verify_packs_report, a_ruled_tier2_row_drifting_to_a_tier1_failure_is_stale) {
    verify_row_outcome row =
        outcome("cs.075", false, "illegal move: /", verify_tier2_skipped("tier 1 failed"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "cubeshape", &row, offender, sizeof offender), 1);
    BT_CHECK(strstr(offender, "stale ruling") != NULL);
    BT_CHECK(strstr(offender, "apply") != NULL);
    BT_CHECK(strstr(offender, "tier2") != NULL);
}

BT_TEST(verify_packs_report, a_ruling_on_a_prose_row_is_stale) {
    verify_row_outcome row =
        outcome("cs.075", false, VERIFY_PROSE_ALG, verify_tier2_skipped("tier 1 failed"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("sq1", "lbl", "cubeshape", &row, offender, sizeof offender), 1);
    BT_CHECK(strstr(offender, "stale ruling") != NULL);
    BT_CHECK(strstr(offender, "prose") != NULL);
}

BT_TEST(verify_packs_report, ruled_failures_matching_their_recorded_class_stay_excused) {
    char offender[VERIFY_OFFENDER_MAX];
    verify_row_outcome apply_row =
        outcome("cs.119", false, "illegal move: /", verify_tier2_skipped("tier 1 failed"));
    BT_CHECK_EQ(
        verify_row_offender("sq1", "lbl", "cubeshape", &apply_row, offender, sizeof offender), 0);
    verify_row_outcome tier2_row =
        outcome("cs.075", true, NULL, verify_tier2_fail("names crossed"));
    BT_CHECK_EQ(
        verify_row_offender("sq1", "lbl", "cubeshape", &tier2_row, offender, sizeof offender), 0);
}

BT_TEST(verify_packs_report, prose_rows_and_clean_passes_never_offend) {
    char offender[VERIFY_OFFENDER_MAX];
    verify_row_outcome prose =
        outcome("op.09", false, VERIFY_PROSE_ALG, verify_tier2_skipped("tier 1 failed"));
    BT_CHECK_EQ(
        verify_row_offender("3bld", "old_pochmann", "concept", &prose, offender, sizeof offender),
        0);
    verify_row_outcome clean = outcome("op.01", true, NULL, verify_tier2_pass());
    BT_CHECK_EQ(
        verify_row_offender("3bld", "old_pochmann", "concept", &clean, offender, sizeof offender),
        0);
}

BT_TEST(verify_packs_report, tier2_failures_carry_their_message) {
    verify_row_outcome row =
        outcome("l4e.99", true, NULL, verify_tier2_fail("five edges displaced"));
    char offender[VERIFY_OFFENDER_MAX];
    BT_CHECK_EQ(verify_row_offender("pyraminx", "l4e", "l4e", &row, offender, sizeof offender), 1);
    BT_CHECK_EQ(offender, "pyraminx/l4e/l4e/l4e.99: tier2: five edges displaced");
}

BT_TEST(verify_packs_report, summarize_counts_each_row_kind_once) {
    verify_section_result section;
    verify_section_init(&section);
    verify_row_outcome rows[5] = {
        outcome("a", true, NULL, verify_tier2_pass()),
        outcome("cs.075", true, NULL, verify_tier2_fail("flipped")),
        outcome("c", true, NULL, verify_tier2_skipped("letter scheme")),
        outcome("op.09", false, VERIFY_PROSE_ALG, verify_tier2_skipped("tier 1 failed")),
        outcome("cs.119", false, "unexecutable", verify_tier2_skipped("tier 1 failed")),
    };
    for (size_t at = 0; at < 5; at++) {
        verify_section_push_row(&section, &rows[at]);
    }
    verify_section_push_honesty(&section, "note");
    bt_pack_entry entry = {0};
    snprintf(entry.file, sizeof entry.file, "%s", "probe.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "sq1");
    snprintf(entry.method, sizeof entry.method, "%s", "lbl");
    snprintf(entry.stage, sizeof entry.stage, "%s", "cubeshape");
    entry.expected_rows = 5;
    verify_pack_section_summary summary;
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_summarize(&entry, &section, &summary, &offenders);
    BT_CHECK_EQ(summary.rows, 5);
    BT_CHECK_EQ(summary.passed, 1);
    BT_CHECK_EQ(summary.failed, 2);
    BT_CHECK_EQ(summary.skipped, 1);
    BT_CHECK_EQ(summary.excused, 1);
    BT_CHECK_EQ(summary.ruled, 2);
    BT_CHECK_EQ(summary.honesty_count, 1);
    BT_CHECK_EQ(summary.honesty[0], "note");
    BT_CHECK_EQ(offenders.count, 0);
    verify_strlist_free(&offenders);
    verify_section_free(&section);
}

BT_TEST(verify_packs_report, markdown_renders_sections_honesty_and_offenders) {
    verify_pack_audit audit = {0};
    verify_strlist_init(&audit.drift);
    verify_strlist_push(&audit.drift, "pin missing");
    verify_strlist_init(&audit.offenders);
    verify_strlist_push(&audit.offenders, "pin missing");
    verify_strlist_push(&audit.offenders, "pyraminx/l4e/l4e/l4e.07: tier2: red");
    audit.sections = malloc(sizeof *audit.sections);
    audit.section_count = 1;
    verify_pack_section_summary *section = &audit.sections[0];
    memset(section, 0, sizeof *section);
    snprintf(section->event, sizeof section->event, "%s", "pyraminx");
    snprintf(section->method, sizeof section->method, "%s", "l4e");
    snprintf(section->stage, sizeof section->stage, "%s", "l4e");
    snprintf(section->file, sizeof section->file, "%s", "pyraminx_l4e.tsv");
    section->rows = 35;
    section->passed = 26;
    section->failed = 9;
    snprintf(section->honesty[0], VERIFY_HONESTY_MAX, "%s", "edges only");
    section->honesty_count = 1;
    bt_strbuf md;
    bt_strbuf_init(&md, 256);
    verify_render_packs_markdown(&audit, &md);
    BT_CHECK(strncmp(md.data, "# pack verification report\n\n", 28) == 0);
    BT_CHECK(strstr(md.data,
                    "| pyraminx | l4e | l4e | pyraminx_l4e.tsv | 35 | 26 | 9 | 0 | 0 | 0 |\n") !=
             NULL);
    BT_CHECK(strstr(md.data, "- pyraminx/l4e/l4e: edges only\n") != NULL);
    BT_CHECK(strstr(md.data, "- pin missing\n") != NULL);
    BT_CHECK(strstr(md.data, "- pyraminx/l4e/l4e/l4e.07: tier2: red\n") != NULL);
    bt_strbuf_free(&md);
    verify_strlist_free(&audit.drift);
    verify_strlist_free(&audit.offenders);
    free(audit.sections);
}

BT_TEST(verify_packs_report, markdown_with_nothing_to_say_says_none) {
    verify_pack_audit audit = {0};
    verify_strlist_init(&audit.drift);
    verify_strlist_init(&audit.offenders);
    bt_strbuf md;
    bt_strbuf_init(&md, 256);
    verify_render_packs_markdown(&audit, &md);
    BT_CHECK(strstr(md.data, "## honesty\n\nnone\n") != NULL);
    BT_CHECK(strstr(md.data, "## offenders\n\nnone\n") != NULL);
    bt_strbuf_free(&md);
    verify_strlist_free(&audit.drift);
    verify_strlist_free(&audit.offenders);
}

BT_TEST(verify_packs_report, registry_failure_keeps_the_real_error_and_the_audit_renders) {
    char scratch[TV_PATH_MAX];
    tv_scratch("vpr_regfail", scratch);
    char packs[TV_PATH_MAX];
    snprintf(packs, sizeof packs, "%s/packs", scratch);
    tv_mkdir_p(packs);
    char registry_path[TV_PATH_MAX];
    snprintf(registry_path, sizeof registry_path, "%s/registry.json", packs);
    tv_write_file(registry_path, "not a json array");
    bt_pack_entry *probe_entries = NULL;
    size_t probe_count = 0;
    bt_seed_error probe_err;
    bt_seedpack_load_registry(packs, &probe_entries, &probe_count, &probe_err);
    char want[512];
    bt_seed_error_string(&probe_err, want, sizeof want);
    verify_pack_audit audit;
    verify_audit_packs(packs, &audit);
    BT_CHECK_EQ(audit.offenders.count, 1);
    BT_CHECK_EQ(audit.offenders.items[0], want);
    BT_CHECK(strstr(audit.offenders.items[0], "registry") != NULL);
    BT_CHECK(strstr(audit.offenders.items[0], "pack registry load failed") == NULL);
    BT_CHECK_EQ(audit.section_count, 0);
    bt_strbuf md;
    bt_strbuf_init(&md, 256);
    verify_render_packs_markdown(&audit, &md);
    BT_CHECK(strncmp(md.data, "# pack verification report\n\n", 28) == 0);
    BT_CHECK(strstr(md.data, "- ") != NULL);
    bt_strbuf_free(&md);
    verify_pack_audit_free(&audit);
    tv_rm_rf(scratch);
}
