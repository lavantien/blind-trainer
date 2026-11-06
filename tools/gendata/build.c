#include "gendata/build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
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
    char **grown = bt_realloc(built->offenders, (built->offender_count + 1) * sizeof(char *));
    if (grown == NULL) {
        return -1;
    }
    built->offenders = grown;
    built->offenders[built->offender_count] = bt_gd_strdup(text);
    if (built->offenders[built->offender_count] == NULL) {
        return -1;
    }
    built->offender_count++;
    return 0;
}

static int verdict_strings(const bt_verdict *verdict, char ***problems, size_t *problem_count,
                           char ***notes, size_t *note_count) {
    *problem_count = verdict->problem_count;
    *problems = *problem_count > 0 ? bt_malloc(*problem_count * sizeof(char *)) : NULL;
    if (*problems == NULL && *problem_count > 0) {
        *problem_count = 0;
        *note_count = 0;
        BT_TRACE_FAIL("gd.build.verdict_strings", "problem strings allocation failed");
        return -1;
    }
    for (size_t i = 0; i < verdict->problem_count; i++) {
        char text[BT_VERIFY_DETAIL_MAX + 32];
        bt_problem_string(&verdict->problems[i], text, sizeof text);
        (*problems)[i] = bt_gd_strdup(text);
        if ((*problems)[i] == NULL) {
            bt_gd_string_list_free(*problems, i);
            *problems = NULL;
            *problem_count = 0;
            *note_count = 0;
            BT_TRACE_FAIL("gd.build.verdict_strings", "problem string duplication failed");
            return -1;
        }
    }
    *note_count = verdict->note_count;
    *notes = *note_count > 0 ? bt_malloc(*note_count * sizeof(char *)) : NULL;
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
        snprintf(text, sizeof text, "twist:%s:%+d", verdict->notes[i].piece,
                 (int)verdict->notes[i].twist);
        (*notes)[i] = bt_gd_strdup(text);
        if ((*notes)[i] == NULL) {
            bt_gd_string_list_free(*notes, i);
            bt_gd_string_list_free(*problems, *problem_count);
            *problems = NULL;
            *problem_count = 0;
            *notes = NULL;
            *note_count = 0;
            BT_TRACE_FAIL("gd.build.verdict_strings", "note string duplication failed");
            return -1;
        }
    }
    return 0;
}

static int vendor_alg_of_row(const bt_corpus_row *row, bool ok, char **problems,
                             size_t problem_count, char **notes, size_t note_count,
                             bt_gd_vendor_alg *out) {
    memset(out, 0, sizeof *out);
    out->solver = bt_gd_strdup(bt_solver_dir(row->solver));
    out->position = bt_gd_strdup(row->position);
    out->alg = bt_gd_strdup(row->alg_text);
    out->commutator = bt_gd_strdup(row->commutator);
    out->recon_link = bt_gd_strdup(row->recon_link);
    out->youtube_link = bt_gd_strdup(row->youtube_link);
    if (out->solver == NULL || out->position == NULL || out->alg == NULL ||
        out->commutator == NULL || out->recon_link == NULL || out->youtube_link == NULL) {
        bt_gd_vendor_alg_free(out);
        return -1;
    }
    out->rowid = row->rowid;
    out->qtm = bt_alg_qtm(&row->alg);
    out->htm = bt_alg_htm(&row->alg);
    out->ok = ok;
    out->problems = problems;
    out->problem_count = problem_count;
    out->notes = notes;
    out->note_count = note_count;
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

static const bt_gd_vendor_alg *best_mirror_alg(const bt_gd_case_entry *mirror) {
    const bt_gd_vendor_alg *best = NULL;
    for (size_t i = 0; i < mirror->vendor_count; i++) {
        const bt_gd_vendor_alg *row = &mirror->vendor_algs[i];
        if (!row->ok) {
            continue;
        }
        if (best == NULL || row->htm < best->htm) {
            best = row;
        }
    }
    if (best == NULL && mirror->vendor_count > 0) {
        best = &mirror->vendor_algs[0];
    }
    return best;
}

void bt_gd_derive_mirrored_inverses(bt_gd_cases_file *cases) {
    BT_TRACE_ENTER("gd.build.derive_mirrored_inverses");
    for (size_t i = 0; i < cases->count; i++) {
        bt_gd_case_entry *entry = &cases->cases[i];
        if (entry->vendor_count != 0) {
            continue;
        }
        const bt_gd_case_entry *mirror = NULL;
        for (size_t m = 0; m < cases->count; m++) {
            const bt_gd_case_entry *candidate = &cases->cases[m];
            if (strcmp(candidate->orbit, entry->orbit) == 0 && candidate->t1 == entry->t2 &&
                candidate->t2 == entry->t1) {
                mirror = candidate;
                break;
            }
        }
        if (mirror == NULL || mirror->vendor_count == 0) {
            continue;
        }
        const bt_gd_vendor_alg *source = best_mirror_alg(mirror);
        bt_orbit orbit = strcmp(entry->orbit, "edge") == 0 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        bt_sheet_class sheet = orbit == BT_ORBIT_EDGE ? BT_SHEET_EDGE : BT_SHEET_CORNER;
        char position[96];
        snprintf(position, sizeof position, "%s-%s-%s", bt_scheme_name_of(buffer_sticker(orbit)),
                 bt_scheme_name_of(entry->t1), bt_scheme_name_of(entry->t2));
        bt_alg parsed;
        bt_alg_init(&parsed);
        if (!bt_alg_try_parse(source->alg, &parsed)) {
            continue;
        }
        bt_alg inverse;
        bt_alg_init(&inverse);
        bool inverse_ok = true;
        for (size_t i = parsed.len; i > 0; i--) {
            if (!bt_alg_push(&inverse, bt_move_invert(parsed.items[i - 1]))) {
                inverse_ok = false;
                break;
            }
        }
        if (!inverse_ok) {
            bt_alg_free(&inverse);
            bt_alg_free(&parsed);
            continue;
        }
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_verify(&inverse, position, sheet, &verdict);
        bool ok = bt_verdict_ok(&verdict);
        char **problems = NULL;
        size_t problem_count = 0;
        char **notes = NULL;
        size_t note_count = 0;
        if (verdict_strings(&verdict, &problems, &problem_count, &notes, &note_count) != 0) {
            bt_verdict_free(&verdict);
            bt_alg_free(&inverse);
            bt_alg_free(&parsed);
            continue;
        }
        bt_verdict_free(&verdict);
        bt_strbuf text;
        bt_strbuf_init(&text, 0);
        bt_alg_format(&inverse, &text);
        if (text.data == NULL) {
            bt_gd_string_list_free(problems, problem_count);
            bt_gd_string_list_free(notes, note_count);
            bt_strbuf_free(&text);
            bt_alg_free(&inverse);
            bt_alg_free(&parsed);
            continue;
        }
        bt_gd_vendor_alg derived;
        memset(&derived, 0, sizeof derived);
        derived.solver = bt_gd_strdup(source->solver);
        derived.position = bt_gd_strdup(position);
        derived.alg = bt_gd_strdup(text.data);
        derived.commutator = bt_gd_strdup(text.data);
        derived.recon_link = bt_gd_strdup(source->recon_link);
        derived.youtube_link = bt_gd_strdup(source->youtube_link);
        if (derived.solver == NULL || derived.position == NULL || derived.alg == NULL ||
            derived.commutator == NULL || derived.recon_link == NULL ||
            derived.youtube_link == NULL) {
            bt_gd_vendor_alg_free(&derived);
            bt_gd_string_list_free(problems, problem_count);
            bt_gd_string_list_free(notes, note_count);
            bt_strbuf_free(&text);
            bt_alg_free(&inverse);
            bt_alg_free(&parsed);
            continue;
        }
        derived.rowid = source->rowid;
        derived.qtm = bt_alg_qtm(&inverse);
        derived.htm = bt_alg_htm(&inverse);
        derived.ok = ok;
        derived.problems = problems;
        derived.problem_count = problem_count;
        derived.notes = notes;
        derived.note_count = note_count;
        bt_gd_vendor_alg *grown = bt_realloc(entry->vendor_algs, sizeof(bt_gd_vendor_alg));
        if (grown != NULL) {
            entry->vendor_algs = grown;
            entry->vendor_algs[0] = derived;
            entry->vendor_count = 1;
            entry->covered = true;
        } else {
            bt_gd_vendor_alg_free(&derived);
        }
        bt_strbuf_free(&text);
        bt_alg_free(&inverse);
        bt_alg_free(&parsed);
    }
    BT_TRACE_EXIT("gd.build.derive_mirrored_inverses");
}

void bt_gd_build(const bt_corpus *corpus, bt_gd_built *out) {
    BT_TRACE_ENTER("gd.build.build");
    memset(out, 0, sizeof *out);
    out->cases.count = 0;
    out->cases.cases = bt_malloc(BT_GD_TOTAL_CASES * sizeof(bt_gd_case_entry));
    if (out->cases.cases == NULL) {
        BT_TRACE_FAIL("gd.build.build", "cases table allocation failed");
        return;
    }
    for (int o = 0; o < 2; o++) {
        bt_orbit orbit = o == 0 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        bt_case enumerated[BT_PAIRS_MAX];
        size_t enumerated_count = bt_scheme_enumerate_cases(orbit, enumerated);
        for (size_t i = 0; i < enumerated_count; i++) {
            bt_gd_case_entry *entry = &out->cases.cases[out->cases.count];
            char letters[2];
            bt_scheme_case_letters(&enumerated[i], letters);
            memset(entry, 0, sizeof *entry);
            entry->orbit = bt_gd_strdup(orbit_name(orbit));
            entry->t1_name = bt_gd_strdup(bt_scheme_name_of(enumerated[i].t1));
            entry->t2_name = bt_gd_strdup(bt_scheme_name_of(enumerated[i].t2));
            if (entry->orbit == NULL || entry->t1_name == NULL || entry->t2_name == NULL) {
                free(entry->orbit);
                free(entry->t1_name);
                free(entry->t2_name);
                bt_gd_built_free(out);
                BT_TRACE_FAIL("gd.build.build", "case entry strings allocation failed");
                return;
            }
            entry->t1 = enumerated[i].t1;
            entry->t2 = enumerated[i].t2;
            entry->t1_letter = letters[0];
            entry->t2_letter = letters[1];
            entry->covered = false;
            entry->vendor_algs = NULL;
            entry->vendor_count = 0;
            out->cases.count++;
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
                     bt_sheet_class_file(row->sheet), row->rowid, row->position,
                     joined.data != NULL ? joined.data : "");
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
                if (vendor_alg_of_row(row, ok, problems, problem_count, notes, note_count, &alg) !=
                    0) {
                    goto next_row;
                }
                bt_gd_vendor_alg *grown = bt_realloc(out->cases.cases[slot].vendor_algs,
                                                     (out->cases.cases[slot].vendor_count + 1) *
                                                         sizeof(bt_gd_vendor_alg));
                if (grown == NULL) {
                    alg.problems = NULL;
                    alg.problem_count = 0;
                    alg.notes = NULL;
                    alg.note_count = 0;
                    bt_gd_vendor_alg_free(&alg);
                    goto next_row;
                }
                out->cases.cases[slot].vendor_algs = grown;
                out->cases.cases[slot].vendor_algs[out->cases.cases[slot].vendor_count++] = alg;
                out->rows_in_pair_space += 1;
                strings_owned = true;
            }
        } else {
            bt_gd_special_entry *grown = bt_realloc(
                out->specials.specials, (out->specials.count + 1) * sizeof(bt_gd_special_entry));
            if (grown == NULL) {
                goto next_row;
            }
            out->specials.specials = grown;
            bt_gd_special_entry *special = &out->specials.specials[out->specials.count];
            memset(special, 0, sizeof *special);
            special->class_name = bt_gd_strdup(bt_sheet_class_file(row->sheet));
            special->solver = bt_gd_strdup(bt_solver_dir(row->solver));
            special->position = bt_gd_strdup(row->position);
            special->alg = bt_gd_strdup(row->alg_text);
            special->commutator = bt_gd_strdup(row->commutator);
            special->recon_link = bt_gd_strdup(row->recon_link);
            special->youtube_link = bt_gd_strdup(row->youtube_link);
            if (special->class_name == NULL || special->solver == NULL ||
                special->position == NULL || special->alg == NULL || special->commutator == NULL ||
                special->recon_link == NULL || special->youtube_link == NULL) {
                free(special->class_name);
                free(special->solver);
                free(special->position);
                free(special->alg);
                free(special->commutator);
                free(special->recon_link);
                free(special->youtube_link);
                bt_gd_specials_file_free(&out->specials);
                goto next_row;
            }
            special->rowid = row->rowid;
            special->qtm = bt_alg_qtm(&row->alg);
            special->htm = bt_alg_htm(&row->alg);
            special->ok = ok;
            special->problems = problems;
            special->problem_count = problem_count;
            special->notes = notes;
            special->note_count = note_count;
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
    out->generated_at = bt_gd_strdup(generated_at);
    out->generator = bt_gd_strdup(bt_gd_crate_id());
    out->synth_params_sha256 = bt_gd_strdup(synth_params_sha256);
    out->corpus_sha256 = bt_gd_strdup(digest->sha256);
    if (out->generated_at == NULL || out->generator == NULL || out->synth_params_sha256 == NULL ||
        out->corpus_sha256 == NULL) {
        bt_gd_manifest_free(out);
        BT_TRACE_FAIL("gd.build.manifest", "header strings allocation failed");
        return;
    }
    out->corpus_rows = digest->rows;
    out->corpus_files = digest->file_count > 0
                            ? bt_malloc(digest->file_count * sizeof(bt_gd_corpus_file_entry))
                            : NULL;
    if (out->corpus_files == NULL && digest->file_count > 0) {
        bt_gd_manifest_free(out);
        BT_TRACE_FAIL("gd.build.manifest", "corpus file table allocation failed");
        return;
    }
    out->corpus_file_count = digest->file_count;
    for (size_t i = 0; i < digest->file_count; i++) {
        out->corpus_files[i].solver = bt_gd_strdup(digest->files[i].solver);
        out->corpus_files[i].sheet = bt_gd_strdup(digest->files[i].sheet);
        out->corpus_files[i].sha256 = bt_gd_strdup(digest->files[i].sha256);
        if (out->corpus_files[i].solver == NULL || out->corpus_files[i].sheet == NULL ||
            out->corpus_files[i].sha256 == NULL) {
            out->corpus_file_count = i + 1;
            bt_gd_manifest_free(out);
            BT_TRACE_FAIL("gd.build.manifest", "corpus file strings allocation failed");
            return;
        }
        out->corpus_files[i].rows = digest->files[i].rows;
    }
    out->rows_in_pair_space = built->rows_in_pair_space;
    out->rows_other_buffer = built->rows_other_buffer;
    out->cases.path = bt_gd_strdup("cases.json");
    out->cases.sha256 = bt_gd_strdup(cases_sha256);
    out->specials.path = bt_gd_strdup("specials.json");
    out->specials.sha256 = bt_gd_strdup(specials_sha256);
    if (out->cases.path == NULL || out->cases.sha256 == NULL || out->specials.path == NULL ||
        out->specials.sha256 == NULL) {
        bt_gd_manifest_free(out);
        BT_TRACE_FAIL("gd.build.manifest", "artifact strings allocation failed");
        return;
    }
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
    out->specials.entries = built->specials.count;
    out->specials.counts = bt_malloc(BT_GD_SPECIAL_COUNTS_LEN * sizeof(bt_gd_class_count));
    if (out->specials.counts == NULL) {
        bt_gd_manifest_free(out);
        BT_TRACE_FAIL("gd.build.manifest", "class counts allocation failed");
        return;
    }
    out->specials.count_count = BT_GD_SPECIAL_COUNTS_LEN;
    for (size_t i = 0; i < BT_GD_SPECIAL_COUNTS_LEN; i++) {
        size_t rows = 0;
        for (size_t s = 0; s < built->specials.count; s++) {
            if (strcmp(built->specials.specials[s].solver, BT_GD_SPECIAL_COUNTS[i].solver) == 0 &&
                strcmp(built->specials.specials[s].class_name,
                       BT_GD_SPECIAL_COUNTS[i].class_name) == 0) {
                rows++;
            }
        }
        out->specials.counts[i].solver = bt_gd_strdup(BT_GD_SPECIAL_COUNTS[i].solver);
        out->specials.counts[i].class_name = bt_gd_strdup(BT_GD_SPECIAL_COUNTS[i].class_name);
        if (out->specials.counts[i].solver == NULL || out->specials.counts[i].class_name == NULL) {
            out->specials.count_count = i + 1;
            bt_gd_manifest_free(out);
            BT_TRACE_FAIL("gd.build.manifest", "class count strings allocation failed");
            return;
        }
        out->specials.counts[i].rows = rows;
    }
    BT_TRACE_EXIT("gd.build.manifest");
}
