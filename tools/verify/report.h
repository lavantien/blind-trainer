#ifndef VERIFY_REPORT_H
#define VERIFY_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/json.h"
#include "common/strbuf.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "data/verify.h"
#include "verify/lib.h"
#include "verify/packs.h"

#define VERIFY_REPORT_GENERATOR_LEN 16
#define VERIFY_REPORT_STAMP_LEN 32
#define VERIFY_REPORT_SOLVER_LEN 16
#define VERIFY_REPORT_SHEET_LEN 16
#define VERIFY_REPORT_NOTE_LEN 64
#define VERIFY_REPORT_PROBLEM_LEN 256

typedef struct {
    char solver[VERIFY_REPORT_SOLVER_LEN];
    char sheet[VERIFY_REPORT_SHEET_LEN];
    size_t rowid;
    char *position;
    char *alg;
    uint32_t qtm;
    uint32_t htm;
    bool has_simcheck;
    uint32_t simcheck_qtm;
    char *simcheck_position;
    bool verify_ok;
    char (*problems)[VERIFY_REPORT_PROBLEM_LEN];
    size_t problem_count;
    char (*notes)[VERIFY_REPORT_NOTE_LEN];
    size_t note_count;
    bool qtm_match;
    bool position_match;
    bool pass;
} verify_report_row_outcome;

typedef struct {
    char solver[VERIFY_REPORT_SOLVER_LEN];
    char sheet[VERIFY_REPORT_SHEET_LEN];
    size_t rows;
    size_t passed;
    size_t failed;
    size_t qtm_agreed;
    size_t position_agreed;
} verify_sheet_summary;

typedef struct {
    char generated_at[VERIFY_REPORT_STAMP_LEN];
    char generator[VERIFY_REPORT_GENERATOR_LEN];
    char corpus_sha256[BT_DIGEST_SHA256_HEX_LEN];
    size_t corpus_rows;
    size_t simcheck_total;
    size_t simcheck_matched;
    verify_sheet_summary sheets[12];
    verify_report_row_outcome *rows;
    size_t row_count;
    verify_report_row_outcome *failures;
    size_t failure_count;
} verify_report;

void verify_note_text(const bt_note *note, char *out, size_t out_len);
void verify_report_reasons(const verify_report_row_outcome *row, bt_strbuf *out);
void verify_build_report(const bt_corpus *corpus, const verify_sim_map *sim,
                         const char *generated_at, const char *corpus_sha256, verify_report *out);
void verify_report_free(verify_report *report);
void verify_report_row_free(verify_report_row_outcome *row);
void verify_report_audit(const verify_report *report, size_t simcheck_total, verify_strlist *out);
void verify_render_markdown(const verify_report *report, bt_strbuf *out);
int verify_report_to_json(const verify_report *report, bt_json_value **out);

#endif
