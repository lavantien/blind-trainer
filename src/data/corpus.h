#ifndef BT_DATA_CORPUS_H
#define BT_DATA_CORPUS_H

#include <stdbool.h>
#include <stddef.h>

#include "common/paths.h"
#include "core/alg.h"
#include "data/repo_algs.h"
#include "data/tsv.h"

#define BT_SOLVER_VENDOR_COUNT 2
#define BT_SHEET_CLASS_COUNT 6
#define BT_CORPUS_INDEX_MAX 2048

typedef enum {
    BT_SOLVER_CHERRY = 0,
    BT_SOLVER_EGGINS = 1,
    BT_SOLVER_USER = 2,
} bt_solver;

typedef enum {
    BT_SHEET_EDGE = 0,
    BT_SHEET_CORNER = 1,
    BT_SHEET_FLIPS = 2,
    BT_SHEET_TWISTS = 3,
    BT_SHEET_PARITY = 4,
    BT_SHEET_LTCT = 5,
} bt_sheet_class;

typedef enum {
    BT_CORPUS_ERR_READ,
    BT_CORPUS_ERR_PARSE,
    BT_CORPUS_ERR_ALG,
} bt_corpus_error_kind;

typedef struct {
    bt_corpus_error_kind kind;
    char path[BT_PATH_MAX];
    bt_tsv_error tsv;
    size_t rowid;
    char alg_error[64];
} bt_corpus_error;

typedef enum {
    BT_OVERLAY_ERR_UNKNOWN_SHEET,
    BT_OVERLAY_ERR_BAD_ALG,
} bt_overlay_error_kind;

typedef struct {
    bt_overlay_error_kind kind;
    char key[BT_REPO_ALGS_KEY_MAX];
    char position[BT_PATH_MAX];
    char alg_error[64];
} bt_overlay_error;

typedef struct {
    bt_solver solver;
    bt_sheet_class sheet;
    size_t rowid;
    char *position;
    bt_alg alg;
    char *alg_text;
    char *commutator;
    char *recon_link;
    char *youtube_link;
} bt_corpus_row;

typedef struct {
    bt_corpus_row *rows;
    size_t count;
} bt_corpus;

const char *bt_solver_dir(bt_solver solver);
bool bt_solver_parse(const char *text, bt_solver *out);
const char *bt_sheet_class_file(bt_sheet_class sheet);
bool bt_sheet_class_parse(const char *text, bt_sheet_class *out);
void bt_corpus_error_string(const bt_corpus_error *err, char *out, size_t out_len);
void bt_overlay_error_string(const bt_overlay_error *err, char *out, size_t out_len);
void bt_corpus_init(bt_corpus *corpus);
void bt_corpus_free(bt_corpus *corpus);
int bt_corpus_load(const char *root, bt_corpus *out, bt_corpus_error *err);
int bt_corpus_from_rows(bt_corpus_row *rows, size_t count, bt_corpus *out);
size_t bt_corpus_total_rows(const bt_corpus *corpus);
bool bt_corpus_is_empty(const bt_corpus *corpus);
const bt_corpus_row *bt_corpus_row_at(const bt_corpus *corpus, bt_solver solver,
                                      bt_sheet_class sheet, size_t rowid);
size_t bt_corpus_sheet_rows(const bt_corpus *corpus, bt_solver solver, bt_sheet_class sheet,
                            size_t *out, size_t cap);
size_t bt_corpus_rows_for_position(const bt_corpus *corpus, bt_sheet_class sheet,
                                   const char *position, size_t *out, size_t cap);
size_t bt_corpus_distinct_positions(const bt_corpus *corpus, bt_solver solver,
                                    bt_sheet_class sheet);
int bt_corpus_with_overlay(const bt_corpus *corpus, const bt_user_algs *overlay, bt_corpus *out,
                           bt_overlay_error *err);

#endif
