#include "verify_test_util.h"

#include <string.h>

#include "verify/lib.h"
#include "verify/report.h"

static const char *const PIN_STAMP = "2026-10-12T00:00:00Z";

static void pin_digest(char out[BT_DIGEST_SHA256_HEX_LEN], char fill) {
    memset(out, fill, 64);
    out[64] = '\0';
}

static void load_corpus_and_sim(bt_corpus *corpus, verify_sim_map *sim) {
    char root[TV_PATH_MAX];
    tv_ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        BT_ABORT("corpus loads from ref_ground");
    }
    char sim_err[512];
    if (verify_load_simcheck(root, sim, sim_err, sizeof sim_err) != 0) {
        BT_ABORT("simcheck loads from ref_ground");
    }
}

static void copy_alg(const bt_alg *src, bt_alg *dst) {
    bt_alg_init(dst);
    for (size_t at = 0; at < bt_alg_len(src); at++) {
        bt_alg_push(dst, src->items[at]);
    }
}

static bool offenders_mention(const verify_strlist *offenders, const char *needle) {
    for (size_t at = 0; at < offenders->count; at++) {
        if (strstr(offenders->items[at], needle) != NULL) {
            return true;
        }
    }
    return false;
}

static verify_sim_entry *sim_entry_at(verify_sim_map *sim, bt_solver solver, bt_sheet_class sheet,
                                      size_t rowid) {
    for (size_t at = 0; at < sim->count; at++) {
        if (sim->items[at].solver == solver && sim->items[at].sheet == sheet &&
            sim->items[at].rowid == rowid) {
            return &sim->items[at];
        }
    }
    return NULL;
}

static void sim_map_without(verify_sim_map *out, const verify_sim_map *src, bt_solver solver,
                            bt_sheet_class sheet, size_t rowid) {
    verify_sim_map_init(out);
    for (size_t at = 0; at < src->count; at++) {
        const verify_sim_entry *entry = &src->items[at];
        if (entry->solver == solver && entry->sheet == sheet && entry->rowid == rowid) {
            continue;
        }
        verify_sim_map_insert(out, entry->solver, entry->sheet, entry->rowid, entry->row.position,
                              entry->row.qtm);
    }
}

BT_TEST(verify_report_pins, report_passes_every_row_against_simcheck) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    BT_CHECK_EQ(report.row_count, 1077);
    BT_CHECK_EQ(report.corpus_rows, 1077);
    BT_CHECK_EQ(report.simcheck_total, 1077);
    BT_CHECK_EQ(report.simcheck_matched, 1077);
    BT_CHECK_EQ(report.failure_count, 0);
    for (size_t at = 0; at < report.row_count; at++) {
        const verify_report_row_outcome *row = &report.rows[at];
        BT_CHECK(row->pass);
        BT_CHECK(row->verify_ok);
        BT_CHECK(row->qtm_match);
        BT_CHECK(row->position_match);
        BT_CHECK(row->has_simcheck);
        BT_CHECK_EQ(row->simcheck_qtm, row->qtm);
        BT_CHECK(strcmp(row->simcheck_position, row->position) == 0);
    }
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 1077, &offenders);
    BT_CHECK_EQ(offenders.count, 0);
    verify_strlist_free(&offenders);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, per_sheet_counts_match_coverage_md) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    for (size_t at = 0; at < VERIFY_SHEET_PIN_COUNT; at++) {
        const verify_sheet_pin *pin = &VERIFY_SHEET_EXPECTED_ROWS[at];
        const verify_sheet_summary *summary = NULL;
        for (size_t sheet_at = 0; sheet_at < 12; sheet_at++) {
            if (strcmp(report.sheets[sheet_at].solver, pin->solver) == 0 &&
                strcmp(report.sheets[sheet_at].sheet, pin->sheet) == 0) {
                summary = &report.sheets[sheet_at];
                break;
            }
        }
        if (summary == NULL) {
            BT_ABORT("%s/%s summary missing", pin->solver, pin->sheet);
        }
        BT_CHECK_EQ(summary->rows, pin->rows);
        BT_CHECK_EQ(summary->passed, pin->rows);
        BT_CHECK_EQ(summary->failed, 0);
        BT_CHECK_EQ(summary->qtm_agreed, pin->rows);
        BT_CHECK_EQ(summary->position_agreed, pin->rows);
    }
    size_t total = 0;
    for (size_t sheet_at = 0; sheet_at < 12; sheet_at++) {
        total += report.sheets[sheet_at].rows;
    }
    BT_CHECK_EQ(total, 1077);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, corrupted_alg_is_flagged_with_named_problems) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    const bt_corpus_row *donor = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 4);
    BT_CHECK(donor != NULL);
    bt_alg planted;
    copy_alg(&donor->alg, &planted);
    bt_alg_free(&corpus.rows[0].alg);
    corpus.rows[0].alg = planted;
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    BT_CHECK_EQ(report.failure_count, 1);
    const verify_report_row_outcome *failure = &report.failures[0];
    BT_CHECK_EQ(failure->rowid, 2);
    BT_CHECK_EQ(failure->solver, "cherry");
    BT_CHECK_EQ(failure->sheet, "edge");
    BT_CHECK(!failure->pass);
    BT_CHECK(!failure->verify_ok);
    BT_CHECK(failure->qtm_match);
    bool names_seat = false;
    for (size_t at = 0; at < failure->problem_count; at++) {
        if (strstr(failure->problems[at], "should sit on") != NULL) {
            names_seat = true;
        }
    }
    BT_CHECK(names_seat);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 1077, &offenders);
    BT_CHECK(offenders_mention(&offenders, "cherry/edge/2"));
    verify_strlist_free(&offenders);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, qtm_mismatch_fails_only_that_row) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    verify_sim_entry *entry = sim_entry_at(&sim, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(entry != NULL);
    entry->row.qtm += 1;
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    BT_CHECK_EQ(report.failure_count, 1);
    const verify_report_row_outcome *failure = &report.failures[0];
    BT_CHECK(failure->verify_ok);
    BT_CHECK(!failure->qtm_match);
    BT_CHECK(failure->position_match);
    bt_strbuf reasons;
    bt_strbuf_init(&reasons, 64);
    verify_report_reasons(failure, &reasons);
    BT_CHECK(strstr(reasons.data, "vs simcheck") != NULL);
    bt_strbuf_free(&reasons);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 1077, &offenders);
    BT_CHECK(offenders_mention(&offenders, "cherry/edge/2"));
    verify_strlist_free(&offenders);
    const verify_sheet_summary *summary = NULL;
    for (size_t sheet_at = 0; sheet_at < 12; sheet_at++) {
        if (strcmp(report.sheets[sheet_at].solver, "cherry") == 0 &&
            strcmp(report.sheets[sheet_at].sheet, "edge") == 0) {
            summary = &report.sheets[sheet_at];
        }
    }
    BT_CHECK(summary != NULL);
    BT_CHECK_EQ(summary->qtm_agreed, 550);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, position_mismatch_fails_only_that_row) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    verify_sim_entry *entry = sim_entry_at(&sim, BT_SOLVER_EGGINS, BT_SHEET_LTCT, 2);
    BT_CHECK(entry != NULL);
    size_t len = strlen(entry->row.position);
    char *longer = malloc(len + 2);
    BT_CHECK(longer != NULL);
    snprintf(longer, len + 2, "%sX", entry->row.position);
    free(entry->row.position);
    entry->row.position = longer;
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    BT_CHECK_EQ(report.failure_count, 1);
    BT_CHECK(report.failures[0].verify_ok);
    BT_CHECK(report.failures[0].qtm_match);
    BT_CHECK(!report.failures[0].position_match);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, missing_and_leftover_simcheck_rows_are_flagged) {
    bt_corpus corpus;
    verify_sim_map loaded;
    load_corpus_and_sim(&corpus, &loaded);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_sim_map records;
    sim_map_without(&records, &loaded, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    verify_sim_map_free(&loaded);
    verify_report report;
    verify_build_report(&corpus, &records, PIN_STAMP, digest, &report);
    BT_CHECK_EQ(report.failure_count, 1);
    bool names_missing = false;
    for (size_t at = 0; at < report.failures[0].problem_count; at++) {
        if (strstr(report.failures[0].problems[at], "missing simcheck record") != NULL) {
            names_missing = true;
        }
    }
    BT_CHECK(names_missing);
    BT_CHECK_EQ(report.simcheck_matched, 1076);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 1076, &offenders);
    BT_CHECK(offenders_mention(&offenders, "simcheck matched 1076"));
    verify_strlist_free(&offenders);
    verify_report_free(&report);
    verify_sim_map_insert(&records, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 99999, "UF-UB-UL", 12);
    verify_report second;
    verify_build_report(&corpus, &records, PIN_STAMP, digest, &second);
    verify_strlist second_offenders;
    verify_strlist_init(&second_offenders);
    verify_report_audit(&second, 1077, &second_offenders);
    BT_CHECK(offenders_mention(&second_offenders, "simcheck holds 1077 rows"));
    verify_strlist_free(&second_offenders);
    verify_report_free(&second);
    verify_sim_map_free(&records);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, sheet_count_drift_is_flagged) {
    bt_corpus base;
    verify_sim_map sim;
    load_corpus_and_sim(&base, &sim);
    size_t kept = 0;
    for (size_t at = 0; at < base.count; at++) {
        bt_corpus_row *row = &base.rows[at];
        if (row->solver == BT_SOLVER_EGGINS && row->sheet == BT_SHEET_LTCT) {
            free(row->position);
            bt_alg_free(&row->alg);
            free(row->alg_text);
            free(row->commutator);
            free(row->recon_link);
            free(row->youtube_link);
            continue;
        }
        base.rows[kept] = *row;
        kept += 1;
    }
    bt_corpus short_corpus;
    bt_corpus_from_rows(base.rows, kept, &short_corpus);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, '0');
    verify_report report;
    verify_build_report(&short_corpus, &sim, PIN_STAMP, digest, &report);
    verify_strlist offenders;
    verify_strlist_init(&offenders);
    verify_report_audit(&report, 1077, &offenders);
    BT_CHECK(offenders_mention(&offenders, "corpus holds 1074 rows"));
    BT_CHECK(offenders_mention(&offenders, "eggins/ltct holds 0 rows"));
    verify_strlist_free(&offenders);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&short_corpus);
}

BT_TEST(verify_report_pins, markdown_renders_the_summary_and_every_sheet) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, 'a');
    verify_report report;
    verify_build_report(&corpus, &sim, PIN_STAMP, digest, &report);
    bt_strbuf md;
    bt_strbuf_init(&md, 8192);
    verify_render_markdown(&report, &md);
    BT_CHECK(strstr(md.data, "# alg verification report") != NULL);
    BT_CHECK(strstr(md.data, "generator: bt-verify") != NULL);
    BT_CHECK(strstr(md.data, "generated_at: 2026-10-12T00:00:00Z") != NULL);
    BT_CHECK(
        strstr(md.data,
               "corpus sha256: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") !=
        NULL);
    BT_CHECK(strstr(md.data, "| cherry | edge | 551 | 551 | 0 | 551 | 551 |") != NULL);
    BT_CHECK(strstr(md.data, "| eggins | ltct | 3 | 3 | 0 | 3 | 3 |") != NULL);
    BT_CHECK(strstr(md.data, "## failures") != NULL);
    BT_CHECK(strstr(md.data, "none") != NULL);
    BT_CHECK(strstr(md.data, "1077/1077 rows pass") != NULL);
    bt_strbuf_free(&md);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_report_pins, report_is_deterministic_apart_from_the_stamp) {
    bt_corpus corpus;
    verify_sim_map sim;
    load_corpus_and_sim(&corpus, &sim);
    char digest[BT_DIGEST_SHA256_HEX_LEN];
    pin_digest(digest, 'b');
    verify_report first;
    verify_build_report(&corpus, &sim, "2026-01-01T00:00:00Z", digest, &first);
    verify_report second;
    verify_build_report(&corpus, &sim, "2026-12-31T23:59:59Z", digest, &second);
    bt_json_value *first_json = NULL;
    bt_json_value *second_json = NULL;
    verify_report_to_json(&first, &first_json);
    verify_report_to_json(&second, &second_json);
    bt_strbuf first_text;
    bt_strbuf_init(&first_text, 8192);
    bt_strbuf second_text;
    bt_strbuf_init(&second_text, 8192);
    bt_json_serialize(first_json, &first_text);
    bt_json_serialize(second_json, &second_text);
    BT_CHECK(strcmp(first_text.data, second_text.data) != 0);
    bt_json_free(first_json);
    bt_json_free(second_json);
    bt_strbuf md_a;
    bt_strbuf_init(&md_a, 8192);
    bt_strbuf md_b;
    bt_strbuf_init(&md_b, 8192);
    verify_render_markdown(&first, &md_a);
    verify_render_markdown(&second, &md_b);
    char *stamp_a = strstr(md_a.data, "2026-01-01T00:00:00Z");
    BT_CHECK(stamp_a != NULL);
    memmove(stamp_a, stamp_a + 20, strlen(stamp_a + 20) + 1);
    char *stamp_b = strstr(md_b.data, "2026-12-31T23:59:59Z");
    BT_CHECK(stamp_b != NULL);
    memmove(stamp_b, stamp_b + 20, strlen(stamp_b + 20) + 1);
    BT_CHECK(strcmp(md_a.data, md_b.data) == 0);
    bt_strbuf_free(&md_a);
    bt_strbuf_free(&md_b);
    snprintf(second.generated_at, sizeof second.generated_at, "%s", first.generated_at);
    bt_json_value *first_again = NULL;
    bt_json_value *second_again = NULL;
    verify_report_to_json(&first, &first_again);
    verify_report_to_json(&second, &second_again);
    bt_strbuf first_again_text;
    bt_strbuf_init(&first_again_text, 8192);
    bt_strbuf second_again_text;
    bt_strbuf_init(&second_again_text, 8192);
    bt_json_serialize(first_again, &first_again_text);
    bt_json_serialize(second_again, &second_again_text);
    BT_CHECK(strcmp(first_again_text.data, second_again_text.data) == 0);
    bt_json_free(first_again);
    bt_json_free(second_again);
    bt_strbuf_free(&first_text);
    bt_strbuf_free(&second_text);
    bt_strbuf_free(&first_again_text);
    bt_strbuf_free(&second_again_text);
    verify_report_free(&first);
    verify_report_free(&second);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
}
