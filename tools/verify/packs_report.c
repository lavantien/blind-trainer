#include "verify/packs_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "data/seedpack/rulings.h"

static void failure_text(const verify_row_outcome *row, char *out, size_t out_len) {
    size_t used = 0;
    if (!row->tier1_ok) {
        used += (size_t)snprintf(out + used, out_len - used, "tier1: %s", row->tier1_err);
    }
    if (row->tier2.kind == VERIFY_TIER2_FAIL) {
        if (used > 0 && used + 2 < out_len) {
            used += (size_t)snprintf(out + used, out_len - used, "; ");
        }
        snprintf(out + used, out_len - used, "tier2: %s", row->tier2.fail);
    }
}

int verify_row_offender(const char *event, const char *method, const char *stage,
                        const verify_row_outcome *row, char *out, size_t out_len) {
    BT_TRACE_ENTER("packs_report.row_offender");
    bool prose = (bool)(!row->tier1_ok && strcmp(row->tier1_err, VERIFY_PROSE_ALG) == 0);
    bool tier2_failed = (bool)(row->tier2.kind == VERIFY_TIER2_FAIL);
    bool failed = (bool)(!row->tier1_ok || tier2_failed);
    const char *reason = bt_expected_failure(event, method, stage, row->case_key);
    if (reason != NULL) {
        bt_failure_class recorded;
        bt_expected_failure_class(event, method, stage, row->case_key, &recorded);
        if (!failed) {
            snprintf(out, out_len, "%s/%s/%s/%s: stale ruling (%s), row passes", event, method,
                     stage, row->case_key, reason);
            BT_TRACE_EXIT("packs_report.row_offender");
            return 1;
        }
        if (prose) {
            snprintf(out, out_len, "%s/%s/%s/%s: stale ruling (%s), row is prose", event, method,
                     stage, row->case_key, reason);
            BT_TRACE_EXIT("packs_report.row_offender");
            return 1;
        }
        bt_failure_class live;
        if (!bt_classify_failure(row->tier1_ok ? NULL : row->tier1_err, tier2_failed, &live)) {
            snprintf(out, out_len, "%s/%s/%s/%s: a failing non-prose row classifies", event, method,
                     stage, row->case_key);
            BT_TRACE_FAIL("packs_report.row_offender", "classify failed %s", row->case_key);
            BT_TRACE_EXIT("packs_report.row_offender");
            return 1;
        }
        if (live != recorded) {
            snprintf(out, out_len,
                     "%s/%s/%s/%s: stale ruling (%s), failure class drifted to %s, ruling "
                     "records %s",
                     event, method, stage, row->case_key, reason, bt_failure_class_name(live),
                     bt_failure_class_name(recorded));
            BT_TRACE_EXIT("packs_report.row_offender");
            return 1;
        }
        BT_TRACE_EXIT("packs_report.row_offender");
        return 0;
    }
    if (failed && !prose) {
        char detail[VERIFY_ERR_MAX * 2];
        failure_text(row, detail, sizeof detail);
        snprintf(out, out_len, "%s/%s/%s/%s: %s", event, method, stage, row->case_key, detail);
        BT_TRACE_EXIT("packs_report.row_offender");
        return 1;
    }
    BT_TRACE_EXIT("packs_report.row_offender");
    return 0;
}

void verify_summarize(const bt_pack_entry *entry, const verify_section_result *section,
                      verify_pack_section_summary *summary, verify_strlist *offenders) {
    BT_TRACE_ENTER("packs_report.summarize");
    memset(summary, 0, sizeof *summary);
    snprintf(summary->event, sizeof summary->event, "%s", entry->event);
    snprintf(summary->method, sizeof summary->method, "%s", entry->method);
    snprintf(summary->stage, sizeof summary->stage, "%s", entry->stage);
    snprintf(summary->file, sizeof summary->file, "%s", entry->file);
    summary->rows = section->count;
    for (size_t at = 0; at < section->honesty_count && at < VERIFY_HONESTY_CAP; at++) {
        snprintf(summary->honesty[at], VERIFY_HONESTY_MAX, "%s", section->honesty[at]);
    }
    summary->honesty_count = section->honesty_count;
    for (size_t at = 0; at < section->count; at++) {
        const verify_row_outcome *row = &section->rows[at];
        bool prose = (bool)(!row->tier1_ok && strcmp(row->tier1_err, VERIFY_PROSE_ALG) == 0);
        bool tier2_failed = (bool)(row->tier2.kind == VERIFY_TIER2_FAIL);
        bool failed = (bool)(!row->tier1_ok || tier2_failed);
        bool ruled = (bool)(bt_expected_failure(entry->event, entry->method, entry->stage,
                                                row->case_key) != NULL);
        if (prose) {
            summary->excused += 1;
        } else if (failed) {
            summary->failed += 1;
            if (ruled) {
                summary->ruled += 1;
            }
        } else if (row->tier2.kind == VERIFY_TIER2_SKIPPED) {
            summary->skipped += 1;
        } else {
            summary->passed += 1;
        }
        char offender[VERIFY_OFFENDER_MAX];
        if (verify_row_offender(entry->event, entry->method, entry->stage, row, offender,
                                sizeof offender) == 1) {
            verify_strlist_push(offenders, offender);
        }
    }
    BT_TRACE_EXIT("packs_report.summarize");
}

void verify_audit_packs(const char *packs_root, verify_pack_audit *out) {
    BT_TRACE_ENTER("packs_report.audit_packs");
    memset(out, 0, sizeof *out);
    verify_pinned_vs_registry(packs_root, &out->drift);
    for (size_t at = 0; at < out->drift.count; at++) {
        verify_strlist_push(&out->offenders, out->drift.items[at]);
    }
    bt_pack_entry *entries = NULL;
    size_t entry_count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs_root, &entries, &entry_count, &err) != 0) {
        BT_TRACE_FAIL("packs_report.audit_packs", "registry load failed");
        BT_TRACE_EXIT("packs_report.audit_packs");
        return;
    }
    for (size_t at = 0; at < entry_count; at++) {
        const bt_pack_entry *entry = &entries[at];
        bt_seed_rows rows;
        if (bt_seedpack_load_pack(packs_root, entry, &rows, &err) != 0) {
            char text[512];
            bt_seed_error_string(&err, text, sizeof text);
            verify_strlist_pushf(&out->offenders, "%s/%s/%s: pack %s fails to load: %s",
                                 entry->event, entry->method, entry->stage, entry->file, text);
            continue;
        }
        verify_section_result section;
        verify_rows(entry, &rows, &section);
        if (out->section_count == out->section_cap) {
            size_t cap = out->section_cap == 0 ? 16 : out->section_cap * 2;
            verify_pack_section_summary *sections = realloc(out->sections, cap * sizeof *sections);
            if (sections == NULL) {
                BT_TRACE_FAIL("packs_report.audit_packs", "sections oom");
                bt_seed_rows_free(&rows);
                verify_section_free(&section);
                continue;
            }
            out->sections = sections;
            out->section_cap = cap;
        }
        verify_pack_section_summary summary;
        verify_summarize(entry, &section, &summary, &out->offenders);
        out->sections[out->section_count] = summary;
        out->section_count += 1;
        bt_seed_rows_free(&rows);
        verify_section_free(&section);
    }
    bt_seedpack_free_entries(entries);
    BT_TRACE_EXIT("packs_report.audit_packs");
}

void verify_pack_audit_free(verify_pack_audit *audit) {
    verify_strlist_free(&audit->drift);
    verify_strlist_free(&audit->offenders);
    free(audit->sections);
    audit->sections = NULL;
    audit->section_count = 0;
    audit->section_cap = 0;
}

void verify_render_packs_markdown(const verify_pack_audit *audit, bt_strbuf *out) {
    BT_TRACE_ENTER("packs_report.render_packs_markdown");
    bt_strbuf_append_str(out, "# pack verification report\n\n");
    bt_strbuf_append_str(
        out, "fail closed: every tier 1 or tier 2 failure without an expected_failure ruling is "
             "an offender, every ruled row failing nothing is a stale ruling\n\n");
    bt_strbuf_append_str(out, "## sections\n\n");
    bt_strbuf_append_str(
        out, "| event | method | stage | file | rows | passed | failed | skipped | excused | "
             "ruled |\n");
    bt_strbuf_append_str(out, "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    for (size_t at = 0; at < audit->section_count; at++) {
        const verify_pack_section_summary *section = &audit->sections[at];
        bt_strbuf_appendf(out, "| %s | %s | %s | %s | %zu | %zu | %zu | %zu | %zu | %zu |\n",
                          section->event, section->method, section->stage, section->file,
                          section->rows, section->passed, section->failed, section->skipped,
                          section->excused, section->ruled);
    }
    bt_strbuf_append_str(out, "\n## honesty\n\n");
    size_t honesty_lines = 0;
    for (size_t at = 0; at < audit->section_count; at++) {
        const verify_pack_section_summary *section = &audit->sections[at];
        for (size_t note = 0; note < section->honesty_count; note++) {
            bt_strbuf_appendf(out, "- %s/%s/%s: %s\n", section->event, section->method,
                              section->stage, section->honesty[note]);
            honesty_lines += 1;
        }
    }
    if (honesty_lines == 0) {
        bt_strbuf_append_str(out, "none\n");
    }
    bt_strbuf_append_str(out, "\n## offenders\n\n");
    if (audit->offenders.count == 0) {
        bt_strbuf_append_str(out, "none\n");
    } else {
        for (size_t at = 0; at < audit->offenders.count; at++) {
            bt_strbuf_appendf(out, "- %s\n", audit->offenders.items[at]);
        }
    }
    BT_TRACE_EXIT("packs_report.render_packs_markdown");
}
