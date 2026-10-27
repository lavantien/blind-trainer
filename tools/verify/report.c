#include "verify/report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

static const bt_solver VENDOR_ORDER[BT_SOLVER_VENDOR_COUNT] = {
    BT_SOLVER_CHERRY,
    BT_SOLVER_EGGINS,
};
static const bt_sheet_class SHEET_ORDER[BT_SHEET_CLASS_COUNT] = {
    BT_SHEET_EDGE, BT_SHEET_CORNER, BT_SHEET_FLIPS, BT_SHEET_TWISTS, BT_SHEET_PARITY, BT_SHEET_LTCT,
};

void verify_note_text(const bt_note *note, char *out, size_t out_len) {
    BT_TRACE_ENTER("report.note_text");
    if (note->kind == BT_NOTE_FLIP) {
        snprintf(out, out_len, "flip:%s", note->piece);
    } else {
        snprintf(out, out_len, "twist:%s:%+d", note->piece, note->twist);
    }
    BT_TRACE_EXIT("report.note_text");
}

static void debug_option_qtm(bool present, uint32_t qtm, char *out, size_t out_len) {
    if (present) {
        snprintf(out, out_len, "Some(%u)", qtm);
    } else {
        snprintf(out, out_len, "%s", "None");
    }
}

static void debug_option_string(bool present, const char *text, char *out, size_t out_len) {
    if (!present) {
        snprintf(out, out_len, "%s", "None");
        return;
    }
    size_t used = (size_t)snprintf(out, out_len, "Some(\"");
    for (const char *cursor = text; *cursor != '\0' && used + 3 < out_len; cursor++) {
        if (*cursor == '"' || *cursor == '\\') {
            used += (size_t)snprintf(out + used, out_len - used, "\\%c", *cursor);
        } else {
            out[used++] = *cursor;
        }
    }
    snprintf(out + used, out_len - used, "\")");
}

void verify_report_reasons(const verify_report_row_outcome *row, bt_strbuf *out) {
    BT_TRACE_ENTER("report.reasons");
    for (size_t at = 0; at < row->problem_count; at++) {
        if (out->len > 0 && out->data[out->len - 1] != ';') {
            bt_strbuf_append_str(out, "; ");
        } else if (at > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_strbuf_append_str(out, row->problems[at]);
    }
    if (!row->qtm_match) {
        char option[32];
        debug_option_qtm(row->has_simcheck, row->simcheck_qtm, option, sizeof option);
        if (row->problem_count > 0) {
            bt_strbuf_append_str(out, "; ");
        }
        bt_strbuf_appendf(out, "qtm %u vs simcheck %s", row->qtm, option);
    }
    if (!row->position_match) {
        char option[VERIFY_REPORT_PROBLEM_LEN];
        debug_option_string(row->has_simcheck, row->simcheck_position ? row->simcheck_position : "",
                            option, sizeof option);
        if (row->problem_count > 0 || !row->qtm_match) {
            bt_strbuf_append_str(out, "; ");
        }
        bt_strbuf_appendf(out, "position %s vs simcheck %s", row->position, option);
    }
    BT_TRACE_EXIT("report.reasons");
}

static verify_report_row_outcome clone_row(const verify_report_row_outcome *row) {
    verify_report_row_outcome copy = *row;
    copy.position = verify_xstrdup(row->position);
    copy.alg = verify_xstrdup(row->alg);
    copy.simcheck_position =
        row->simcheck_position == NULL ? NULL : verify_xstrdup(row->simcheck_position);
    if (row->problem_count > 0) {
        copy.problems = malloc(row->problem_count * sizeof *copy.problems);
        for (size_t at = 0; at < row->problem_count; at++) {
            snprintf(copy.problems[at], VERIFY_REPORT_PROBLEM_LEN, "%s", row->problems[at]);
        }
    } else {
        copy.problems = NULL;
    }
    if (row->note_count > 0) {
        copy.notes = malloc(row->note_count * sizeof *copy.notes);
        for (size_t at = 0; at < row->note_count; at++) {
            snprintf(copy.notes[at], VERIFY_REPORT_NOTE_LEN, "%s", row->notes[at]);
        }
    } else {
        copy.notes = NULL;
    }
    return copy;
}

void verify_report_row_free(verify_report_row_outcome *row) {
    free(row->position);
    free(row->alg);
    free(row->simcheck_position);
    free(row->problems);
    free(row->notes);
    row->position = NULL;
    row->alg = NULL;
    row->simcheck_position = NULL;
    row->problems = NULL;
    row->notes = NULL;
}

void verify_build_report(const bt_corpus *corpus, const verify_sim_map *sim,
                         const char *generated_at, const char *corpus_sha256, verify_report *out) {
    BT_TRACE_ENTER("report.build_report");
    memset(out, 0, sizeof *out);
    snprintf(out->generated_at, sizeof out->generated_at, "%s", generated_at);
    snprintf(out->generator, sizeof out->generator, "%s", "bt-verify");
    snprintf(out->corpus_sha256, sizeof out->corpus_sha256, "%s", corpus_sha256);
    size_t slot = 0;
    for (size_t solver_at = 0; solver_at < BT_SOLVER_VENDOR_COUNT; solver_at++) {
        for (size_t sheet_at = 0; sheet_at < BT_SHEET_CLASS_COUNT; sheet_at++) {
            verify_sheet_summary *summary = &out->sheets[slot];
            snprintf(summary->solver, sizeof summary->solver, "%s",
                     bt_solver_dir(VENDOR_ORDER[solver_at]));
            snprintf(summary->sheet, sizeof summary->sheet, "%s",
                     bt_sheet_class_file(SHEET_ORDER[sheet_at]));
            summary->rows = 0;
            summary->passed = 0;
            summary->failed = 0;
            summary->qtm_agreed = 0;
            summary->position_agreed = 0;
            slot += 1;
        }
    }
    out->rows = malloc((corpus->count + 1) * sizeof *out->rows);
    out->failures = malloc((corpus->count + 1) * sizeof *out->failures);
    if (out->rows == NULL || out->failures == NULL) {
        BT_TRACE_FAIL("report.build_report", "oom over %zu rows", corpus->count);
        exit(1);
    }
    for (size_t at = 0; at < corpus->count; at++) {
        const bt_corpus_row *row = &corpus->rows[at];
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_verify(&row->alg, row->position, row->sheet, &verdict);
        const verify_sim_row *record = verify_sim_map_get(sim, row->solver, row->sheet, row->rowid);
        if (record != NULL) {
            out->simcheck_matched += 1;
        }
        uint32_t qtm = bt_alg_qtm(&row->alg);
        uint32_t htm = bt_alg_htm(&row->alg);
        bool qtm_match = (bool)(record != NULL && record->qtm == qtm);
        bool position_match =
            (bool)(record != NULL && strcmp(record->position, row->position) == 0);
        verify_report_row_outcome outcome = {0};
        snprintf(outcome.solver, sizeof outcome.solver, "%s", bt_solver_dir(row->solver));
        snprintf(outcome.sheet, sizeof outcome.sheet, "%s", bt_sheet_class_file(row->sheet));
        outcome.rowid = row->rowid;
        outcome.position = verify_xstrdup(row->position);
        outcome.alg = verify_xstrdup(row->alg_text);
        outcome.qtm = qtm;
        outcome.htm = htm;
        outcome.has_simcheck = record != NULL;
        if (record != NULL) {
            outcome.simcheck_qtm = record->qtm;
            outcome.simcheck_position = verify_xstrdup(record->position);
        }
        outcome.verify_ok = bt_verdict_ok(&verdict);
        if (verdict.problem_count > 0) {
            outcome.problems = malloc((verdict.problem_count + 1) * sizeof *outcome.problems);
        }
        size_t problem_count = 0;
        for (size_t problem_at = 0; problem_at < verdict.problem_count; problem_at++) {
            bt_problem_string(&verdict.problems[problem_at], outcome.problems[problem_count],
                              VERIFY_REPORT_PROBLEM_LEN);
            problem_count += 1;
        }
        if (record == NULL) {
            if (outcome.problems == NULL) {
                outcome.problems = malloc(2 * sizeof *outcome.problems);
            }
            snprintf(outcome.problems[problem_count], VERIFY_REPORT_PROBLEM_LEN, "%s",
                     "missing simcheck record");
            problem_count += 1;
        }
        outcome.problem_count = problem_count;
        if (verdict.note_count > 0) {
            outcome.notes = malloc(verdict.note_count * sizeof *outcome.notes);
            for (size_t note_at = 0; note_at < verdict.note_count; note_at++) {
                verify_note_text(&verdict.notes[note_at], outcome.notes[note_at],
                                 VERIFY_REPORT_NOTE_LEN);
            }
            outcome.note_count = verdict.note_count;
        }
        outcome.qtm_match = qtm_match;
        outcome.position_match = position_match;
        outcome.pass = (bool)(outcome.verify_ok && qtm_match && position_match);
        verify_sheet_summary *summary = NULL;
        for (size_t sheet_at = 0; sheet_at < 12; sheet_at++) {
            if (strcmp(out->sheets[sheet_at].solver, outcome.solver) == 0 &&
                strcmp(out->sheets[sheet_at].sheet, outcome.sheet) == 0) {
                summary = &out->sheets[sheet_at];
                break;
            }
        }
        if (summary == NULL) {
            BT_TRACE_FAIL("report.build_report", "summary slot missing for %s/%s", outcome.solver,
                          outcome.sheet);
            bt_verdict_free(&verdict);
            verify_report_row_free(&outcome);
            continue;
        }
        summary->rows += 1;
        if (outcome.pass) {
            summary->passed += 1;
        } else {
            summary->failed += 1;
            out->failures[out->failure_count] = clone_row(&outcome);
            out->failure_count += 1;
        }
        if (record != NULL && outcome.qtm_match) {
            summary->qtm_agreed += 1;
        }
        if (record != NULL && outcome.position_match) {
            summary->position_agreed += 1;
        }
        out->rows[out->row_count] = outcome;
        out->row_count += 1;
        bt_verdict_free(&verdict);
    }
    out->corpus_rows = bt_corpus_total_rows(corpus);
    out->simcheck_total = sim->count;
    BT_TRACE_EXIT("report.build_report");
}

void verify_report_free(verify_report *report) {
    BT_TRACE_ENTER("report.free");
    for (size_t at = 0; at < report->row_count; at++) {
        verify_report_row_free(&report->rows[at]);
    }
    free(report->rows);
    for (size_t at = 0; at < report->failure_count; at++) {
        verify_report_row_free(&report->failures[at]);
    }
    free(report->failures);
    report->rows = NULL;
    report->row_count = 0;
    report->failures = NULL;
    report->failure_count = 0;
    BT_TRACE_EXIT("report.free");
}

void verify_report_audit(const verify_report *report, size_t simcheck_total, verify_strlist *out) {
    BT_TRACE_ENTER("report.audit");
    verify_strlist_init(out);
    for (size_t at = 0; at < report->failure_count; at++) {
        const verify_report_row_outcome *failure = &report->failures[at];
        bt_strbuf reasons;
        bt_strbuf_init(&reasons, 64);
        verify_report_reasons(failure, &reasons);
        verify_strlist_pushf(out, "%s/%s/%zu %s: %s", failure->solver, failure->sheet,
                             failure->rowid, failure->position, reasons.data);
        bt_strbuf_free(&reasons);
    }
    if (report->corpus_rows != BT_DATA_TOTAL_ROWS) {
        verify_strlist_pushf(out, "corpus holds %zu rows, expected %d", report->corpus_rows,
                             BT_DATA_TOTAL_ROWS);
    }
    if (report->simcheck_matched != report->corpus_rows) {
        verify_strlist_pushf(out, "simcheck matched %zu of %zu corpus rows",
                             report->simcheck_matched, report->corpus_rows);
    }
    if (simcheck_total != report->simcheck_matched) {
        verify_strlist_pushf(out, "simcheck holds %zu rows, %zu matched", simcheck_total,
                             report->simcheck_matched);
    }
    for (size_t at = 0; at < VERIFY_SHEET_PIN_COUNT; at++) {
        const verify_sheet_pin *pin = &VERIFY_SHEET_EXPECTED_ROWS[at];
        const verify_sheet_summary *summary = NULL;
        for (size_t sheet_at = 0; sheet_at < 12; sheet_at++) {
            if (strcmp(report->sheets[sheet_at].solver, pin->solver) == 0 &&
                strcmp(report->sheets[sheet_at].sheet, pin->sheet) == 0) {
                summary = &report->sheets[sheet_at];
                break;
            }
        }
        if (summary == NULL) {
            verify_strlist_pushf(out, "%s/%s summary is missing", pin->solver, pin->sheet);
            continue;
        }
        if (summary->rows != pin->rows) {
            verify_strlist_pushf(out, "%s/%s holds %zu rows, expected %zu", pin->solver, pin->sheet,
                                 summary->rows, pin->rows);
        }
    }
    BT_TRACE_EXIT("report.audit");
}

void verify_render_markdown(const verify_report *report, bt_strbuf *out) {
    BT_TRACE_ENTER("report.render_markdown");
    bt_strbuf_append_str(out, "# alg verification report\n\n");
    bt_strbuf_appendf(out, "generator: %s\n", report->generator);
    bt_strbuf_appendf(out, "generated_at: %s\n", report->generated_at);
    bt_strbuf_appendf(out, "corpus sha256: %s\n", report->corpus_sha256);
    bt_strbuf_appendf(out, "corpus rows: %zu\n", report->corpus_rows);
    bt_strbuf_appendf(out, "simcheck rows: %zu matched: %zu\n\n", report->simcheck_total,
                      report->simcheck_matched);
    bt_strbuf_append_str(out, "## per solver and sheet\n\n");
    bt_strbuf_append_str(
        out, "| solver | sheet | rows | passed | failed | qtm agreed | position agreed |\n");
    bt_strbuf_append_str(out, "| --- | --- | --- | --- | --- | --- | --- |\n");
    for (size_t at = 0; at < 12; at++) {
        const verify_sheet_summary *sheet = &report->sheets[at];
        bt_strbuf_appendf(out, "| %s | %s | %zu | %zu | %zu | %zu | %zu |\n", sheet->solver,
                          sheet->sheet, sheet->rows, sheet->passed, sheet->failed,
                          sheet->qtm_agreed, sheet->position_agreed);
    }
    bt_strbuf_append_str(out, "\n## checks\n\n");
    bt_strbuf_append_str(out,
                         "verify: every row passes geometric verification on the emulated cube\n");
    bt_strbuf_append_str(out,
                         "qtm: quarter turn counts agree with the independent simcheck oracle\n");
    bt_strbuf_append_str(out, "position: sheet positions agree with simcheck\n\n");
    bt_strbuf_append_str(out, "## failures\n\n");
    if (report->failure_count == 0) {
        bt_strbuf_append_str(out, "none\n\n");
    } else {
        for (size_t at = 0; at < report->failure_count; at++) {
            const verify_report_row_outcome *failure = &report->failures[at];
            bt_strbuf reasons;
            bt_strbuf_init(&reasons, 64);
            verify_report_reasons(failure, &reasons);
            bt_strbuf_appendf(out, "- %s/%s/%zu %s: %s\n", failure->solver, failure->sheet,
                              failure->rowid, failure->position, reasons.data);
            bt_strbuf_free(&reasons);
        }
        bt_strbuf_append_str(out, "\n");
    }
    bt_strbuf_append_str(out, "## summary\n\n");
    bt_strbuf_appendf(out, "%zu/%zu rows pass\n", report->row_count - report->failure_count,
                      report->row_count);
    BT_TRACE_EXIT("report.render_markdown");
}

static bt_json_value *row_to_json(const verify_report_row_outcome *row) {
    bt_json_value *object = bt_json_new_object();
    bt_json_object_set(object, "solver", bt_json_new_string(row->solver));
    bt_json_object_set(object, "sheet", bt_json_new_string(row->sheet));
    bt_json_object_set(object, "rowid", bt_json_new_integer((long long)row->rowid));
    bt_json_object_set(object, "position", bt_json_new_string(row->position));
    bt_json_object_set(object, "alg", bt_json_new_string(row->alg));
    bt_json_object_set(object, "qtm", bt_json_new_integer(row->qtm));
    bt_json_object_set(object, "htm", bt_json_new_integer(row->htm));
    if (row->has_simcheck) {
        bt_json_object_set(object, "simcheck_qtm", bt_json_new_integer(row->simcheck_qtm));
        bt_json_object_set(object, "simcheck_position", bt_json_new_string(row->simcheck_position));
    } else {
        bt_json_object_set(object, "simcheck_qtm", bt_json_new_null());
        bt_json_object_set(object, "simcheck_position", bt_json_new_null());
    }
    bt_json_object_set(object, "verify_ok", bt_json_new_bool((int)row->verify_ok));
    bt_json_value *problems = bt_json_new_array();
    for (size_t at = 0; at < row->problem_count; at++) {
        bt_json_array_push(problems, bt_json_new_string(row->problems[at]));
    }
    bt_json_object_set(object, "problems", problems);
    bt_json_value *notes = bt_json_new_array();
    for (size_t at = 0; at < row->note_count; at++) {
        bt_json_array_push(notes, bt_json_new_string(row->notes[at]));
    }
    bt_json_object_set(object, "notes", notes);
    bt_json_object_set(object, "qtm_match", bt_json_new_bool((int)row->qtm_match));
    bt_json_object_set(object, "position_match", bt_json_new_bool((int)row->position_match));
    bt_json_object_set(object, "pass", bt_json_new_bool((int)row->pass));
    return object;
}

int verify_report_to_json(const verify_report *report, bt_json_value **out) {
    BT_TRACE_ENTER("report.to_json");
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, "generated_at", bt_json_new_string(report->generated_at));
    bt_json_object_set(root, "generator", bt_json_new_string(report->generator));
    bt_json_object_set(root, "corpus_sha256", bt_json_new_string(report->corpus_sha256));
    bt_json_object_set(root, "corpus_rows", bt_json_new_integer((long long)report->corpus_rows));
    bt_json_object_set(root, "simcheck_total",
                       bt_json_new_integer((long long)report->simcheck_total));
    bt_json_object_set(root, "simcheck_matched",
                       bt_json_new_integer((long long)report->simcheck_matched));
    bt_json_value *sheets = bt_json_new_array();
    for (size_t at = 0; at < 12; at++) {
        const verify_sheet_summary *sheet = &report->sheets[at];
        bt_json_value *entry = bt_json_new_object();
        bt_json_object_set(entry, "solver", bt_json_new_string(sheet->solver));
        bt_json_object_set(entry, "sheet", bt_json_new_string(sheet->sheet));
        bt_json_object_set(entry, "rows", bt_json_new_integer((long long)sheet->rows));
        bt_json_object_set(entry, "passed", bt_json_new_integer((long long)sheet->passed));
        bt_json_object_set(entry, "failed", bt_json_new_integer((long long)sheet->failed));
        bt_json_object_set(entry, "qtm_agreed", bt_json_new_integer((long long)sheet->qtm_agreed));
        bt_json_object_set(entry, "position_agreed",
                           bt_json_new_integer((long long)sheet->position_agreed));
        bt_json_array_push(sheets, entry);
    }
    bt_json_object_set(root, "sheets", sheets);
    bt_json_value *rows = bt_json_new_array();
    for (size_t at = 0; at < report->row_count; at++) {
        bt_json_array_push(rows, row_to_json(&report->rows[at]));
    }
    bt_json_object_set(root, "rows", rows);
    bt_json_value *failures = bt_json_new_array();
    for (size_t at = 0; at < report->failure_count; at++) {
        bt_json_array_push(failures, row_to_json(&report->failures[at]));
    }
    bt_json_object_set(root, "failures", failures);
    *out = root;
    BT_TRACE_EXIT("report.to_json");
    return 0;
}
