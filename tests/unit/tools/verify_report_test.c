#include "verify_test_util.h"

#include <string.h>

#include "data/digest.h"
#include "verify/lib.h"
#include "verify/report.h"

static void outcome_row(const char *solver, const char *sheet, size_t rowid, const char *position,
                        verify_report_row_outcome *out) {
    memset(out, 0, sizeof *out);
    snprintf(out->solver, sizeof out->solver, "%s", solver);
    snprintf(out->sheet, sizeof out->sheet, "%s", sheet);
    out->rowid = rowid;
    out->position = verify_xstrdup(position);
    out->alg = verify_xstrdup("");
    out->verify_ok = true;
    out->has_simcheck = false;
    out->qtm_match = true;
    out->position_match = true;
    out->pass = true;
}

BT_TEST(verify_report, note_text_renders_flip_and_twist_with_signs) {
    char out[VERIFY_REPORT_NOTE_LEN];
    bt_note flip = {BT_NOTE_FLIP, "UF", 0};
    verify_note_text(&flip, out, sizeof out);
    BT_CHECK_EQ(out, "flip:UF");
    bt_note plus2 = {BT_NOTE_TWIST, "UFR", 2};
    verify_note_text(&plus2, out, sizeof out);
    BT_CHECK_EQ(out, "twist:UFR:+2");
    bt_note minus1 = {BT_NOTE_TWIST, "UFL", -1};
    verify_note_text(&minus1, out, sizeof out);
    BT_CHECK_EQ(out, "twist:UFL:-1");
    bt_note zero = {BT_NOTE_TWIST, "UBL", 0};
    verify_note_text(&zero, out, sizeof out);
    BT_CHECK_EQ(out, "twist:UBL:+0");
}

static void copy_alg(const bt_alg *src, bt_alg *dst) {
    bt_alg_init(dst);
    for (size_t at = 0; at < bt_alg_len(src); at++) {
        bt_alg_push(dst, src->items[at]);
    }
}

static void row_from_corpus(const bt_corpus *base, bt_solver solver, bt_sheet_class sheet,
                            size_t rowid, bt_corpus_row *out) {
    const bt_corpus_row *row = bt_corpus_row_at(base, solver, sheet, rowid);
    if (row == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "no %s row %zu in ref_ground", bt_solver_dir(solver),
                        rowid);
        memset(out, 0, sizeof *out);
        out->solver = solver;
        out->sheet = sheet;
        out->rowid = rowid;
        bt_alg_init(&out->alg);
        return;
    }
    memset(out, 0, sizeof *out);
    out->solver = solver;
    out->sheet = sheet;
    out->rowid = row->rowid;
    out->position = verify_xstrdup(row->position);
    copy_alg(&row->alg, &out->alg);
    out->alg_text = verify_xstrdup(row->alg_text);
    out->commutator = verify_xstrdup(row->commutator);
    out->recon_link = verify_xstrdup(row->recon_link);
    out->youtube_link = verify_xstrdup(row->youtube_link);
}

BT_TEST(verify_report, build_report_groups_rows_and_tallies_sheets) {
    char root[TV_PATH_MAX];
    tv_ref_ground(root);
    bt_corpus base;
    bt_corpus_error err;
    BT_CHECK_EQ(bt_corpus_load(root, &base, &err), 0);
    bt_corpus_row *rows = malloc(4 * sizeof *rows);
    BT_CHECK(rows != NULL);
    row_from_corpus(&base, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2, &rows[0]);
    row_from_corpus(&base, BT_SOLVER_EGGINS, BT_SHEET_CORNER, 2, &rows[1]);
    row_from_corpus(&base, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 3, &rows[2]);
    bt_alg_free(&rows[2].alg);
    bt_alg_init(&rows[2].alg);
    char planted_token[64];
    BT_CHECK(bt_alg_parse("R U R' U'", &rows[2].alg, planted_token, sizeof planted_token));
    free(rows[2].alg_text);
    rows[2].alg_text = verify_xstrdup("R U R' U'");
    memset(&rows[3], 0, sizeof rows[3]);
    rows[3].solver = BT_SOLVER_CHERRY;
    rows[3].sheet = BT_SHEET_TWISTS;
    rows[3].rowid = 1;
    rows[3].position = verify_xstrdup("UFR & UFL");
    bt_alg_init(&rows[3].alg);
    rows[3].alg_text = verify_xstrdup("");
    rows[3].commutator = verify_xstrdup("");
    rows[3].recon_link = verify_xstrdup("");
    rows[3].youtube_link = verify_xstrdup("");
    bt_corpus corpus;
    bt_corpus_from_rows(rows, 4, &corpus);
    verify_sim_map sim;
    verify_sim_map_init(&sim);
    uint32_t edge_qtm = bt_alg_qtm(&rows[0].alg);
    uint32_t corner_qtm = bt_alg_qtm(&rows[1].alg);
    verify_sim_map_insert(&sim, BT_SOLVER_CHERRY, BT_SHEET_EDGE, rows[0].rowid, rows[0].position,
                          edge_qtm);
    verify_sim_map_insert(&sim, BT_SOLVER_CHERRY, BT_SHEET_EDGE, rows[2].rowid, rows[0].position,
                          4);
    verify_sim_map_insert(&sim, BT_SOLVER_EGGINS, BT_SHEET_CORNER, rows[1].rowid, rows[1].position,
                          corner_qtm);
    verify_report report;
    verify_build_report(&corpus, &sim, "stamp", "cafecafecafecafecafecafecafecafe", &report);
    BT_CHECK_EQ(report.generator, "bt-verify");
    BT_CHECK_EQ(report.generated_at, "stamp");
    BT_CHECK_EQ(report.corpus_sha256, "cafecafecafecafecafecafecafecafe");
    BT_CHECK_EQ(report.corpus_rows, 4);
    BT_CHECK_EQ(report.simcheck_total, 3);
    BT_CHECK_EQ(report.simcheck_matched, 3);
    BT_CHECK_EQ(report.row_count, 4);
    BT_CHECK_EQ(report.failure_count, 2);
    size_t sheet_total = 0;
    for (size_t at = 0; at < 12; at++) {
        sheet_total += report.sheets[at].rows;
    }
    BT_CHECK_EQ(sheet_total, 4);

    verify_report_row_outcome *passing = &report.rows[0];
    BT_CHECK_EQ(passing->solver, "cherry");
    BT_CHECK_EQ(passing->sheet, "edge");
    BT_CHECK_EQ(passing->rowid, rows[0].rowid);
    BT_CHECK_EQ(passing->position, rows[0].position);
    BT_CHECK_EQ(passing->alg, rows[0].alg_text);
    BT_CHECK_EQ(passing->qtm, edge_qtm);
    BT_CHECK_EQ(passing->htm, bt_alg_htm(&rows[0].alg));
    BT_CHECK(passing->has_simcheck);
    BT_CHECK_EQ(passing->simcheck_qtm, edge_qtm);
    BT_CHECK_EQ(passing->simcheck_position, rows[0].position);
    BT_CHECK(passing->verify_ok);
    BT_CHECK_EQ(passing->problem_count, 0);
    BT_CHECK_EQ(passing->note_count, 0);
    BT_CHECK(passing->qtm_match);
    BT_CHECK(passing->position_match);
    BT_CHECK(passing->pass);

    verify_report_row_outcome *wrong_cycle = &report.rows[2];
    BT_CHECK(!wrong_cycle->verify_ok);
    BT_CHECK(wrong_cycle->problem_count > 0);
    BT_CHECK_EQ(wrong_cycle->qtm, 4);
    BT_CHECK(wrong_cycle->qtm_match);
    BT_CHECK(wrong_cycle->position_match);
    BT_CHECK(!wrong_cycle->pass);

    verify_report_row_outcome *twists = &report.rows[3];
    BT_CHECK(!twists->verify_ok);
    BT_CHECK_EQ(twists->note_count, 2);
    BT_CHECK_EQ(twists->notes[0], "twist:UFR:+0");
    BT_CHECK_EQ(twists->notes[1], "twist:UFL:+0");
    bool has_missing = false;
    for (size_t at = 0; at < twists->problem_count; at++) {
        if (strcmp(twists->problems[at], "missing simcheck record") == 0) {
            has_missing = true;
        }
    }
    BT_CHECK(has_missing);
    BT_CHECK(!twists->qtm_match);
    BT_CHECK(!twists->position_match);
    BT_CHECK(!twists->pass);
    bt_strbuf reasons;
    bt_strbuf_init(&reasons, 64);
    verify_report_reasons(twists, &reasons);
    BT_CHECK(strstr(reasons.data, "qtm 0 vs simcheck None") != NULL);
    BT_CHECK(strstr(reasons.data, "position UFR & UFL vs simcheck None") != NULL);
    bt_strbuf_free(&reasons);

    const verify_sheet_summary *cherry_edge = NULL;
    const verify_sheet_summary *eggins_corner = NULL;
    const verify_sheet_summary *cherry_twists = NULL;
    const verify_sheet_summary *cherry_parity = NULL;
    for (size_t at = 0; at < 12; at++) {
        if (strcmp(report.sheets[at].solver, "cherry") == 0 &&
            strcmp(report.sheets[at].sheet, "edge") == 0) {
            cherry_edge = &report.sheets[at];
        }
        if (strcmp(report.sheets[at].solver, "eggins") == 0 &&
            strcmp(report.sheets[at].sheet, "corner") == 0) {
            eggins_corner = &report.sheets[at];
        }
        if (strcmp(report.sheets[at].solver, "cherry") == 0 &&
            strcmp(report.sheets[at].sheet, "twists") == 0) {
            cherry_twists = &report.sheets[at];
        }
        if (strcmp(report.sheets[at].solver, "cherry") == 0 &&
            strcmp(report.sheets[at].sheet, "parity") == 0) {
            cherry_parity = &report.sheets[at];
        }
    }
    BT_CHECK(cherry_edge != NULL && eggins_corner != NULL && cherry_twists != NULL &&
             cherry_parity != NULL);
    BT_CHECK_EQ(cherry_edge->rows, 2);
    BT_CHECK_EQ(cherry_edge->passed, 1);
    BT_CHECK_EQ(cherry_edge->failed, 1);
    BT_CHECK_EQ(cherry_edge->qtm_agreed, 2);
    BT_CHECK_EQ(cherry_edge->position_agreed, 2);
    BT_CHECK_EQ(eggins_corner->rows, 1);
    BT_CHECK_EQ(eggins_corner->passed, 1);
    BT_CHECK_EQ(eggins_corner->failed, 0);
    BT_CHECK_EQ(eggins_corner->qtm_agreed, 1);
    BT_CHECK_EQ(eggins_corner->position_agreed, 1);
    BT_CHECK_EQ(cherry_twists->rows, 1);
    BT_CHECK_EQ(cherry_twists->passed, 0);
    BT_CHECK_EQ(cherry_twists->failed, 1);
    BT_CHECK_EQ(cherry_twists->qtm_agreed, 0);
    BT_CHECK_EQ(cherry_twists->position_agreed, 0);
    BT_CHECK_EQ(cherry_parity->rows, 0);
    BT_CHECK_EQ(cherry_parity->passed, 0);
    BT_CHECK_EQ(cherry_parity->failed, 0);
    BT_CHECK_EQ(cherry_parity->qtm_agreed, 0);
    BT_CHECK_EQ(cherry_parity->position_agreed, 0);
    BT_CHECK_EQ(report.failures[0].rowid, rows[2].rowid);
    BT_CHECK_EQ(report.failures[1].sheet, "twists");
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
    bt_corpus_free(&base);
}

BT_TEST(verify_report, simcheck_mismatches_surface_in_reasons) {
    char root[TV_PATH_MAX];
    tv_ref_ground(root);
    bt_corpus base;
    bt_corpus_error err;
    BT_CHECK_EQ(bt_corpus_load(root, &base, &err), 0);
    bt_corpus_row *rows = malloc(1 * sizeof *rows);
    BT_CHECK(rows != NULL);
    row_from_corpus(&base, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2, &rows[0]);
    bt_corpus corpus;
    bt_corpus_from_rows(rows, 1, &corpus);
    uint32_t qtm = bt_alg_qtm(&rows[0].alg);
    verify_sim_map sim;
    verify_sim_map_init(&sim);
    verify_sim_map_insert(&sim, BT_SOLVER_CHERRY, BT_SHEET_EDGE, rows[0].rowid, "UB-UF-UR",
                          qtm + 1);
    verify_report report;
    verify_build_report(&corpus, &sim, "t",
                        "0000000000000000000000000000000000000000000000000000000000000000",
                        &report);
    verify_report_row_outcome *outcome = &report.rows[0];
    BT_CHECK(outcome->verify_ok);
    BT_CHECK(!outcome->qtm_match);
    BT_CHECK(!outcome->position_match);
    BT_CHECK(!outcome->pass);
    BT_CHECK_EQ(report.failure_count, 1);
    bt_strbuf reasons;
    bt_strbuf_init(&reasons, 64);
    verify_report_reasons(outcome, &reasons);
    char want[512];
    snprintf(want, sizeof want, "qtm %u vs simcheck Some(%u)", qtm, qtm + 1);
    BT_CHECK(strstr(reasons.data, want) != NULL);
    BT_CHECK(strstr(reasons.data, "position ") != NULL);
    BT_CHECK(strstr(reasons.data, " vs simcheck Some(\"UB-UF-UR\")") != NULL);
    bt_strbuf_free(&reasons);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
    bt_corpus_free(&base);
}

BT_TEST(verify_report, audit_lists_failures_and_population_gaps_in_order) {
    verify_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.generated_at, sizeof report.generated_at, "%s", "t");
    snprintf(report.generator, sizeof report.generator, "%s", "bt-verify");
    snprintf(report.corpus_sha256, sizeof report.corpus_sha256, "%s",
             "0000000000000000000000000000000000000000000000000000000000000000");
    report.corpus_rows = 5;
    report.simcheck_total = 9;
    report.simcheck_matched = 4;
    verify_sheet_summary summary = {"cherry", "edge", 3, 2, 1, 3, 3};
    report.sheets[0] = summary;
    verify_report_row_outcome failure;
    outcome_row("cherry", "edge", 7, "UB", &failure);
    failure.qtm = 5;
    failure.verify_ok = false;
    failure.problems = malloc(sizeof *failure.problems);
    failure.problem_count = 1;
    snprintf(failure.problems[0], VERIFY_REPORT_PROBLEM_LEN, "%s",
             "bad position: unknown sticker name UB");
    failure.qtm_match = false;
    failure.position_match = false;
    failure.pass = false;
    report.failures = malloc(sizeof *report.failures);
    report.failure_count = 1;
    report.failures[0] = failure;
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 7, &offenders);
    BT_CHECK_EQ(offenders.count, 16);
    BT_CHECK_EQ(offenders.items[0],
                "cherry/edge/7 UB: bad position: unknown sticker name UB; qtm 5 vs simcheck "
                "None; position UB vs simcheck None");
    BT_CHECK_EQ(offenders.items[1], "corpus holds 5 rows, expected 1077");
    BT_CHECK_EQ(offenders.items[2], "simcheck matched 4 of 5 corpus rows");
    BT_CHECK_EQ(offenders.items[3], "simcheck holds 7 rows, 4 matched");
    BT_CHECK_EQ(offenders.items[4], "cherry/edge holds 3 rows, expected 551");
    BT_CHECK_EQ(offenders.items[5], "cherry/corner summary is missing");
    verify_strlist_free(&offenders);
    report.row_count = 0;
    report.rows = NULL;
    verify_report_free(&report);
}

BT_TEST(verify_report, audit_accepts_a_fully_consistent_report) {
    verify_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.corpus_sha256, sizeof report.corpus_sha256, "%s",
             "0000000000000000000000000000000000000000000000000000000000000000");
    report.corpus_rows = BT_DATA_TOTAL_ROWS;
    report.simcheck_total = BT_DATA_TOTAL_ROWS;
    report.simcheck_matched = BT_DATA_TOTAL_ROWS;
    for (size_t at = 0; at < VERIFY_SHEET_PIN_COUNT; at++) {
        const verify_sheet_pin *pin = &VERIFY_SHEET_EXPECTED_ROWS[at];
        verify_sheet_summary *summary = &report.sheets[at];
        snprintf(summary->solver, sizeof summary->solver, "%s", pin->solver);
        snprintf(summary->sheet, sizeof summary->sheet, "%s", pin->sheet);
        summary->rows = pin->rows;
        summary->passed = pin->rows;
        summary->qtm_agreed = pin->rows;
        summary->position_agreed = pin->rows;
    }
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, BT_DATA_TOTAL_ROWS, &offenders);
    BT_CHECK_EQ(offenders.count, 0);
    verify_strlist_free(&offenders);
}

BT_TEST(verify_report, render_markdown_prints_tables_checks_and_the_summary) {
    verify_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.generated_at, sizeof report.generated_at, "%s", "2026-10-14T00:00:00Z");
    snprintf(report.generator, sizeof report.generator, "%s", "bt-verify");
    snprintf(report.corpus_sha256, sizeof report.corpus_sha256, "%s",
             "ababababababababababababababababababababababababababababababababab");
    report.corpus_rows = 3;
    report.simcheck_total = 2;
    report.simcheck_matched = 2;
    verify_sheet_summary cherry_edge = {"cherry", "edge", 2, 1, 1, 2, 2};
    verify_sheet_summary eggins_ltct = {"eggins", "ltct", 1, 1, 0, 1, 1};
    report.sheets[0] = cherry_edge;
    report.sheets[1] = eggins_ltct;
    report.rows = malloc(3 * sizeof *report.rows);
    report.row_count = 3;
    outcome_row("cherry", "edge", 1, "a", &report.rows[0]);
    outcome_row("cherry", "edge", 2, "b", &report.rows[1]);
    outcome_row("eggins", "ltct", 3, "c", &report.rows[2]);
    report.failures = malloc(1 * sizeof *report.failures);
    report.failure_count = 1;
    outcome_row("eggins", "corner", 4, "pos", &report.failures[0]);
    report.failures[0].verify_ok = false;
    report.failures[0].problems = malloc(sizeof *report.failures[0].problems);
    report.failures[0].problem_count = 1;
    snprintf(report.failures[0].problems[0], VERIFY_REPORT_PROBLEM_LEN, "%s", "problems here");
    report.failures[0].pass = false;
    bt_strbuf md;
    bt_strbuf_init(&md, 256);
    verify_render_markdown(&report, &md);
    BT_CHECK(strncmp(md.data, "# alg verification report\n\n", 27) == 0);
    BT_CHECK(strstr(md.data, "generator: bt-verify\n") != NULL);
    BT_CHECK(strstr(md.data, "generated_at: 2026-10-14T00:00:00Z\n") != NULL);
    BT_CHECK(strstr(md.data, "corpus rows: 3\n") != NULL);
    BT_CHECK(strstr(md.data, "simcheck rows: 2 matched: 2\n") != NULL);
    BT_CHECK(strstr(md.data, "| solver | sheet | rows | passed | failed | qtm agreed | position "
                             "agreed |\n| --- | --- | --- | --- | --- | --- | --- |\n") != NULL);
    BT_CHECK(strstr(md.data, "| cherry | edge | 2 | 1 | 1 | 2 | 2 |\n") != NULL);
    BT_CHECK(strstr(md.data, "| eggins | ltct | 1 | 1 | 0 | 1 | 1 |\n") != NULL);
    BT_CHECK(strstr(md.data, "verify: every row passes geometric verification on the emulated "
                             "cube\n") != NULL);
    BT_CHECK(strstr(md.data, "- eggins/corner/4 pos: problems here\n") != NULL);
    size_t len = strlen(md.data);
    BT_CHECK(strcmp(md.data + len - 26, "## summary\n\n2/3 rows pass\n") == 0);
    bt_strbuf_free(&md);
    verify_report_free(&report);
}

BT_TEST(verify_report, render_markdown_prints_none_when_nothing_failed) {
    verify_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.corpus_sha256, sizeof report.corpus_sha256, "%s",
             "0000000000000000000000000000000000000000000000000000000000000000");
    report.corpus_rows = 1;
    report.simcheck_total = 1;
    report.simcheck_matched = 1;
    report.rows = malloc(1 * sizeof *report.rows);
    report.row_count = 1;
    outcome_row("cherry", "edge", 1, "a", &report.rows[0]);
    bt_strbuf md;
    bt_strbuf_init(&md, 256);
    verify_render_markdown(&report, &md);
    BT_CHECK(strstr(md.data, "## failures\n\nnone\n\n") != NULL);
    size_t len = strlen(md.data);
    BT_CHECK(strcmp(md.data + len - 26, "## summary\n\n1/1 rows pass\n") == 0);
    bt_strbuf_free(&md);
    verify_report_free(&report);
}
