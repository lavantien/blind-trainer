#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"

const char *bt_mut_outcome_summary(bt_mut_status status) {
    switch (status) {
    case BT_MUT_KILLED:
        return "CaughtMutant";
    case BT_MUT_SURVIVED:
        return "MissedMutant";
    case BT_MUT_TIMEOUT:
        return "Timeout";
    case BT_MUT_BUILDERR:
        return "Unviable";
    case BT_MUT_PENDING:
        return "Pending";
    }
    return "Pending";
}

static bt_json_value *span_object(size_t line, size_t col, size_t end_line, size_t end_col) {
    bt_json_value *span = bt_json_new_object();
    bt_json_value *start = bt_json_new_object();
    bt_json_object_set(start, "line", bt_json_new_integer((long long)line));
    bt_json_object_set(start, "column", bt_json_new_integer((long long)col));
    bt_json_value *end = bt_json_new_object();
    bt_json_object_set(end, "line", bt_json_new_integer((long long)end_line));
    bt_json_object_set(end, "column", bt_json_new_integer((long long)end_col));
    bt_json_object_set(span, "start", start);
    bt_json_object_set(span, "end", end);
    return span;
}

bt_json_value *bt_mut_mutant_scenario(const bt_mut_row *row, const char *function) {
    bt_json_value *payload = bt_json_new_object();
    char name[512];
    snprintf(name, sizeof name, "%s:%zu:%zu: %s %s -> %s", row->file, row->line, row->col, row->op,
             row->from, row->to[0] != '\0' ? row->to : "(empty)");
    bt_json_object_set(payload, "name", bt_json_new_string(name));
    bt_json_object_set(payload, "package", bt_json_new_string("blind-trainer"));
    bt_json_object_set(payload, "file", bt_json_new_string(row->file));
    if (function != NULL && function[0] != '\0') {
        bt_json_value *function_value = bt_json_new_object();
        bt_json_object_set(function_value, "function_name", bt_json_new_string(function));
        bt_json_object_set(function_value, "return_type", bt_json_new_string(""));
        bt_json_object_set(function_value, "span", span_object(0, 0, 0, 0));
        bt_json_object_set(payload, "function", function_value);
    } else {
        bt_json_object_set(payload, "function", bt_json_new_null());
    }
    bt_json_object_set(payload, "span", span_object(row->line, row->col, row->line, row->col));
    bt_json_object_set(payload, "replacement", bt_json_new_string(row->to));
    bt_json_object_set(payload, "genre", bt_json_new_string(row->op));
    bt_json_value *scenario = bt_json_new_object();
    bt_json_object_set(scenario, "Mutant", payload);
    return scenario;
}

int bt_mut_report(const char *ledger_path, const char *repo_root, const char *out_path) {
    BT_TRACE_ENTER("mutate.report");
    bt_mut_ledger ledger;
    if (bt_mut_ledger_load(ledger_path, &ledger) != 0) {
        BT_TRACE_FAIL("mutate.report", "cannot load ledger %s", ledger_path);
        BT_TRACE_EXIT("mutate.report");
        return 1;
    }
    for (size_t i = 0; i < ledger.count; i++) {
        if (ledger.rows[i].status == BT_MUT_PENDING) {
            char pending_id[BT_MUT_ID_LEN + 1];
            memcpy(pending_id, ledger.rows[i].id, BT_MUT_ID_LEN + 1);
            bt_mut_ledger_free(&ledger);
            BT_TRACE_FAIL("mutate.report", "pending row %s refuses the report", pending_id);
            BT_TRACE_EXIT("mutate.report");
            return 1;
        }
    }
    bt_json_value *root = bt_json_new_object();
    unsigned long long counters[5] = {0};
    bt_json_value *outcomes = bt_json_new_array();
    bt_json_value *baseline = bt_json_new_object();
    bt_json_object_set(baseline, "scenario", bt_json_new_string("Baseline"));
    bt_json_object_set(baseline, "summary", bt_json_new_string("Success"));
    bt_json_array_push(outcomes, baseline);
    char now[32];
    bt_mut_now_iso(now);
    for (size_t i = 0; i < ledger.count; i++) {
        bt_mut_row *row = &ledger.rows[i];
        char path[BT_MUT_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", repo_root, row->file);
        char *text = NULL;
        size_t len = 0;
        const char *function = "";
        if (bt_mut_read_file(path, &text, &len) == 0) {
            bt_mut_tokens tokens;
            if (bt_mut_tokenize(text, len, &tokens) == 0) {
                size_t offset = bt_mut_offset_at(tokens.text, row->line, row->col);
                if (offset != (size_t)-1) {
                    function = bt_mut_function_at(&tokens, offset);
                }
                bt_mut_tokens_free(&tokens);
            }
            free(text);
        }
        bt_json_value *outcome = bt_json_new_object();
        bt_json_object_set(outcome, "scenario", bt_mut_mutant_scenario(row, function));
        bt_json_object_set(outcome, "summary",
                           bt_json_new_string(bt_mut_outcome_summary(row->status)));
        bt_json_array_push(outcomes, outcome);
        counters[0]++;
        if (row->status == BT_MUT_KILLED) {
            counters[1]++;
        } else if (row->status == BT_MUT_SURVIVED) {
            counters[2]++;
        } else if (row->status == BT_MUT_TIMEOUT) {
            counters[3]++;
        } else {
            counters[4]++;
        }
    }
    bt_json_object_set(root, "total_mutants", bt_json_new_integer((long long)counters[0]));
    bt_json_object_set(root, "caught", bt_json_new_integer((long long)counters[1]));
    bt_json_object_set(root, "missed", bt_json_new_integer((long long)counters[2]));
    bt_json_object_set(root, "timeout", bt_json_new_integer((long long)counters[3]));
    bt_json_object_set(root, "unviable", bt_json_new_integer((long long)counters[4]));
    bt_json_object_set(root, "success", bt_json_new_integer(0));
    bt_json_object_set(root, "cargo_mutants_version", bt_json_new_string("bt-mutate"));
    bt_json_object_set(root, "start_time", bt_json_new_string(now));
    bt_json_object_set(root, "end_time", bt_json_new_string(now));
    bt_json_object_set(root, "outcomes", outcomes);
    bt_strbuf serialized;
    int failed = bt_strbuf_init(&serialized, 8192) != 0;
    if (!failed) {
        failed = bt_json_serialize(root, &serialized) != 0;
    }
    if (!failed) {
        failed = bt_mut_write_file(out_path, serialized.data, serialized.len) != 0;
    }
    int rc = failed ? 1 : 0;
    bt_strbuf_free(&serialized);
    bt_json_free(root);
    bt_mut_ledger_free(&ledger);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.report", "cannot write %s", out_path);
    }
    BT_TRACE_EXIT("mutate.report");
    return rc;
}
