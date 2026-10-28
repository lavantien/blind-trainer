#include "gendata/build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "data/digest.h"
#include "data/verify.h"
#include "gendata/lib.h"
#include "scheme/classifier.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

static const char *orbit_name(bt_orbit orbit) {
    return orbit == BT_ORBIT_EDGE ? "edge" : "corner";
}

static const char *orbit_debug(bt_orbit orbit) {
    return orbit == BT_ORBIT_EDGE ? "Edge" : "Corner";
}

static uint8_t buffer_sticker(bt_orbit orbit) {
    return orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER : BT_SCHEME_CORNER_BUFFER_STICKER;
}

static const char *class_debug(const bt_classification *class) {
    switch (class->kind) {
    case BT_CLASS_SOLVED:
        return "Solved";
    case BT_CLASS_THREE_STYLE:
        return "ThreeStyle";
    case BT_CLASS_FLIP_PAIR:
        return "FlipPair";
    case BT_CLASS_TWIST_PAIR:
        return "TwistPair";
    case BT_CLASS_PARITY:
        return "Parity";
    case BT_CLASS_LTCT:
        return "Ltct";
    default:
        return class->unclassified;
    }
}

static int offenders_push(bt_gd_built *built, const char *text) {
    char **grown = realloc(built->offenders, (built->offender_count + 1) * sizeof(char *));
    if (grown == NULL) {
        return -1;
    }
    built->offenders = grown;
    built->offenders[built->offender_count] = _strdup(text);
    built->offender_count++;
    return 0;
}

static void note_text(const bt_note *note, char *out, size_t out_len) {
    if (note->kind == BT_NOTE_FLIP) {
        snprintf(out, out_len, "flip:%s", note->piece);
    } else {
        snprintf(out, out_len, "twist:%s:%+d", note->piece, (int)note->twist);
    }
}

static int verdict_strings(const bt_verdict *verdict, char ***problems, size_t *problem_count,
                           char ***notes, size_t *note_count) {
    *problem_count = verdict->problem_count;
    *problems = *problem_count > 0 ? malloc(*problem_count * sizeof(char *)) : NULL;
    if (*problems == NULL && *problem_count > 0) {
        *problem_count = 0;
        *note_count = 0;
        BT_TRACE_FAIL("gd.build.verdict_strings", "problem strings allocation failed");
        return -1;
    }
    for (size_t i = 0; i < verdict->problem_count; i++) {
        char text[BT_VERIFY_DETAIL_MAX + 32];
        bt_problem_string(&verdict->problems[i], text, sizeof text);
        (*problems)[i] = _strdup(text);
    }
    *note_count = verdict->note_count;
    *notes = *note_count > 0 ? malloc(*note_count * sizeof(char *)) : NULL;
    if (*notes == NULL && *note_count > 0) {
        bt_gd_string_list_free(*problems, *problem_count);
        *problems = NULL;
        *problem_count = 0;
        *note_count = 0;
        BT_TRACE_FAIL("gd.build.verdict_strings", "note strings allocation failed");
        return -1;
    }
    for (size_t i = 0; i < verdict->note_count; i++) {
        char text[64];
        note_text(&verdict->notes[i], text, sizeof text);
        (*notes)[i] = _strdup(text);
    }
    return 0;
}

static int vendor_alg_of_row(const bt_corpus_row *row, bool ok, char **problems,
                             size_t problem_count, char **notes, size_t note_count,
                             bt_gd_vendor_alg *out) {
    memset(out, 0, sizeof *out);
    out->solver = _strdup(bt_solver_dir(row->solver));
    out->rowid = row->rowid;
    out->position = _strdup(row->position);
    out->alg = _strdup(row->alg_text);
    out->qtm = bt_alg_qtm(&row->alg);
    out->htm = bt_alg_htm(&row->alg);
    out->ok = ok;
    out->problems = problems;
    out->problem_count = problem_count;
    out->notes = notes;
    out->note_count = note_count;
    out->commutator = _strdup(row->commutator);
    out->recon_link = _strdup(row->recon_link);
    out->youtube_link = _strdup(row->youtube_link);
    return 0;
}

void bt_gd_built_free(bt_gd_built *built) {
    bt_gd_cases_file_free(&built->cases);
    bt_gd_specials_file_free(&built->specials);
    bt_gd_string_list_free(built->offenders, built->offender_count);
    built->offenders = NULL;
    built->offender_count = 0;
    built->rows_in_pair_space = 0;
    built->rows_other_buffer = 0;
}

void bt_gd_build(const bt_corpus *corpus, bt_gd_built *out) {
    BT_TRACE_ENTER("gd.build.build");
    memset(out, 0, sizeof *out);
    out->cases.count = 0;
    out->cases.cases = malloc(BT_GD_TOTAL_CASES * sizeof(bt_gd_case_entry));
    if (out->cases.cases == NULL) {
        BT_TRACE_FAIL("gd.build.build", "cases table allocation failed");
        return;
    }
    for (int o = 0; o < 2; o++) {
        bt_orbit orbit = o == 0 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        bt_case enumerated[BT_PAIRS_MAX];
        size_t enumerated_count = bt_scheme_enumerate_cases(orbit, enumerated);
        for (size_t i = 0; i < enumerated_count; i++) {
            bt_gd_case_entry *entry = &out->cases.cases[out->cases.count++];
            char letters[2];
            bt_scheme_case_letters(&enumerated[i], letters);
            memset(entry, 0, sizeof *entry);
            entry->orbit = _strdup(orbit_name(orbit));
            entry->t1 = enumerated[i].t1;
            entry->t2 = enumerated[i].t2;
            entry->t1_letter = letters[0];
            entry->t2_letter = letters[1];
            entry->t1_name = _strdup(bt_scheme_name_of(enumerated[i].t1));
            entry->t2_name = _strdup(bt_scheme_name_of(enumerated[i].t2));
            entry->covered = false;
            entry->vendor_algs = NULL;
            entry->vendor_count = 0;
        }
    }
    for (size_t row_index = 0; row_index < corpus->count; row_index++) {
        const bt_corpus_row *row = &corpus->rows[row_index];
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_verify(&row->alg, row->position, row->sheet, &verdict);
        bool ok = bt_verdict_ok(&verdict);
        char **problems = NULL;
        size_t problem_count = 0;
        char **notes = NULL;
        size_t note_count = 0;
        bool strings_owned = false;
        if (verdict_strings(&verdict, &problems, &problem_count, &notes, &note_count) != 0) {
            bt_verdict_free(&verdict);
            goto next_row;
        }
        if (!ok) {
            bt_strbuf joined;
            bt_strbuf_init(&joined, 0);
            for (size_t i = 0; i < problem_count; i++) {
                if (i > 0) {
                    bt_strbuf_append_str(&joined, "; ");
                }
                bt_strbuf_append_str(&joined, problems[i]);
            }
            char offender[BT_PATH_MAX + 256];
            snprintf(offender, sizeof offender, "%s/%s/%zu %s: %s", bt_solver_dir(row->solver),
                     bt_sheet_class_file(row->sheet), row->rowid, row->position, joined.data);
            offenders_push(out, offender);
            bt_strbuf_free(&joined);
        }
        bt_verdict_free(&verdict);
        if (row->sheet == BT_SHEET_EDGE || row->sheet == BT_SHEET_CORNER) {
            bt_orbit expected = row->sheet == BT_SHEET_EDGE ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
            bt_report report;
            bt_scheme_classify_alg(&row->alg, &report);
            char offender[BT_PATH_MAX + 256];
            if (report.class.kind != BT_CLASS_THREE_STYLE) {
                snprintf(offender, sizeof offender, "%s/%s/%zu is not a three style case: %s",
                         bt_solver_dir(row->solver), bt_sheet_class_file(row->sheet), row->rowid,
                         class_debug(&report.class));
                offenders_push(out, offender);
                goto next_row;
            }
            if (report.class.orbit != expected) {
                snprintf(offender, sizeof offender, "%s/%s/%zu walks the wrong orbit %s",
                         bt_solver_dir(row->solver), bt_sheet_class_file(row->sheet), row->rowid,
                         orbit_debug(report.class.orbit));
                offenders_push(out, offender);
                goto next_row;
            }
            if (report.class.case_count != 1) {
                snprintf(offender, sizeof offender, "%s/%s/%zu walks %zu cases at once",
                         bt_solver_dir(row->solver), bt_sheet_class_file(row->sheet), row->rowid,
                         report.class.case_count);
                offenders_push(out, offender);
                goto next_row;
            }
            {
                const bt_trace *trace =
                    report.class.orbit == BT_ORBIT_EDGE ? &report.edge : &report.corner;
                if (trace->block_count == 0) {
                    snprintf(offender, sizeof offender, "%s/%s/%zu has no walk block",
                             bt_solver_dir(row->solver), bt_sheet_class_file(row->sheet),
                             row->rowid);
                    offenders_push(out, offender);
                    goto next_row;
                }
                if (trace->blocks[0][0] != buffer_sticker(report.class.orbit)) {
                    out->rows_other_buffer += 1;
                    goto next_row;
                }
            }
            {
                bt_case walked = report.class.cases[0];
                bool found = false;
                size_t slot = 0;
                for (size_t i = 0; i < out->cases.count; i++) {
                    bt_gd_case_entry *entry = &out->cases.cases[i];
                    if (entry->t1 == walked.t1 && entry->t2 == walked.t2 &&
                        ((report.class.orbit == BT_ORBIT_EDGE &&
                          strcmp(entry->orbit, "edge") == 0) ||
                         (report.class.orbit == BT_ORBIT_CORNER &&
                          strcmp(entry->orbit, "corner") == 0))) {
                        found = true;
                        slot = i;
                        break;
                    }
                }
                if (!found) {
                    snprintf(
                        offender, sizeof offender, "%s/%s/%zu walks outside the pair space: %s-%s",
                        bt_solver_dir(row->solver), bt_sheet_class_file(row->sheet), row->rowid,
                        bt_scheme_name_of(walked.t1), bt_scheme_name_of(walked.t2));
                    offenders_push(out, offender);
                    goto next_row;
                }
                bt_gd_vendor_alg alg;
                vendor_alg_of_row(row, ok, problems, problem_count, notes, note_count, &alg);
                bt_gd_vendor_alg *grown =
                    realloc(out->cases.cases[slot].vendor_algs,
                            (out->cases.cases[slot].vendor_count + 1) * sizeof(bt_gd_vendor_alg));
                if (grown == NULL) {
                    bt_gd_vendor_alg_free(&alg);
                    goto next_row;
                }
                out->cases.cases[slot].vendor_algs = grown;
                out->cases.cases[slot].vendor_algs[out->cases.cases[slot].vendor_count++] = alg;
                out->rows_in_pair_space += 1;
                strings_owned = true;
            }
        } else {
            bt_gd_special_entry *grown = realloc(
                out->specials.specials, (out->specials.count + 1) * sizeof(bt_gd_special_entry));
            if (grown == NULL) {
                goto next_row;
            }
            out->specials.specials = grown;
            bt_gd_special_entry *special = &out->specials.specials[out->specials.count];
            memset(special, 0, sizeof *special);
            special->class_name = _strdup(bt_sheet_class_file(row->sheet));
            special->solver = _strdup(bt_solver_dir(row->solver));
            special->rowid = row->rowid;
            special->position = _strdup(row->position);
            special->alg = _strdup(row->alg_text);
            special->qtm = bt_alg_qtm(&row->alg);
            special->htm = bt_alg_htm(&row->alg);
            special->ok = ok;
            special->problems = problems;
            special->problem_count = problem_count;
            special->notes = notes;
            special->note_count = note_count;
            special->commutator = _strdup(row->commutator);
            special->recon_link = _strdup(row->recon_link);
            special->youtube_link = _strdup(row->youtube_link);
            out->specials.count++;
            strings_owned = true;
        }
        if (!strings_owned) {
            bt_gd_string_list_free(problems, problem_count);
            bt_gd_string_list_free(notes, note_count);
        }
        continue;
    next_row:
        bt_gd_string_list_free(problems, problem_count);
        bt_gd_string_list_free(notes, note_count);
    }
    for (size_t i = 0; i < out->cases.count; i++) {
        out->cases.cases[i].covered = out->cases.cases[i].vendor_count != 0;
    }
    BT_TRACE_EXIT("gd.build.build");
}

bool bt_gd_regen_needed(const bt_gd_manifest *existing, bool has_existing,
                        const char *synth_params_sha256, const char *corpus_sha256) {
    if (!has_existing || existing == NULL) {
        return true;
    }
    if (strcmp(existing->synth_params_sha256, synth_params_sha256) != 0) {
        return true;
    }
    if (strcmp(existing->corpus_sha256, corpus_sha256) != 0) {
        return true;
    }
    return false;
}

void bt_gd_manifest_make(const bt_gd_built *built, const bt_corpus_digest *digest,
                         const char *cases_sha256, const char *specials_sha256,
                         const char *synth_params_sha256, const char *generated_at,
                         bt_gd_manifest *out) {
    BT_TRACE_ENTER("gd.build.manifest");
    memset(out, 0, sizeof *out);
    out->generated_at = _strdup(generated_at);
    out->generator = _strdup(bt_gd_crate_id());
    out->synth_params_sha256 = _strdup(synth_params_sha256);
    out->corpus_sha256 = _strdup(digest->sha256);
    out->corpus_rows = digest->rows;
    out->corpus_file_count = digest->file_count;
    out->corpus_files = digest->file_count > 0
                            ? malloc(digest->file_count * sizeof(bt_gd_corpus_file_entry))
                            : NULL;
    for (size_t i = 0; i < digest->file_count; i++) {
        out->corpus_files[i].solver = _strdup(digest->files[i].solver);
        out->corpus_files[i].sheet = _strdup(digest->files[i].sheet);
        out->corpus_files[i].rows = digest->files[i].rows;
        out->corpus_files[i].sha256 = _strdup(digest->files[i].sha256);
    }
    out->rows_in_pair_space = built->rows_in_pair_space;
    out->rows_other_buffer = built->rows_other_buffer;
    out->cases.path = _strdup("cases.json");
    out->cases.sha256 = _strdup(cases_sha256);
    out->cases.entries = built->cases.count;
    for (size_t i = 0; i < built->cases.count; i++) {
        bt_gd_orbit_coverage *slot = strcmp(built->cases.cases[i].orbit, "edge") == 0
                                         ? &out->cases.edge
                                         : &out->cases.corner;
        slot->total++;
        if (built->cases.cases[i].covered) {
            slot->covered++;
        } else {
            slot->uncovered++;
        }
    }
    out->specials.path = _strdup("specials.json");
    out->specials.sha256 = _strdup(specials_sha256);
    out->specials.entries = built->specials.count;
    out->specials.count_count = BT_GD_SPECIAL_COUNTS_LEN;
    out->specials.counts = malloc(BT_GD_SPECIAL_COUNTS_LEN * sizeof(bt_gd_class_count));
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        size_t rows = 0;
        for (size_t s = 0; s < built->specials.count; s++) {
            if (strcmp(built->specials.specials[s].solver, BT_GD_SPECIAL_COUNTS[i].solver) == 0 &&
                strcmp(built->specials.specials[s].class_name,
                       BT_GD_SPECIAL_COUNTS[i].class_name) == 0) {
                rows++;
            }
        }
        out->specials.counts[i].solver = _strdup(BT_GD_SPECIAL_COUNTS[i].solver);
        out->specials.counts[i].class_name = _strdup(BT_GD_SPECIAL_COUNTS[i].class_name);
        out->specials.counts[i].rows = rows;
    }
    BT_TRACE_EXIT("gd.build.manifest");
}
