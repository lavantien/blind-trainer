#ifndef BT_DATA_VERIFY_H
#define BT_DATA_VERIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/alg.h"
#include "core/cube.h"
#include "data/corpus.h"

#define BT_VERIFY_DETAIL_MAX 128
#define BT_VERIFY_NAME_MAX 16

typedef enum {
    BT_PROBLEM_BAD_POSITION,
    BT_PROBLEM_CENTERS_MOVED,
    BT_PROBLEM_WRONG_CYCLE,
    BT_PROBLEM_DISPLACED,
    BT_PROBLEM_UNEXPECTED_DEFECT,
    BT_PROBLEM_MISSING_DEFECT,
    BT_PROBLEM_NON_FIXED_RESIDUE,
    BT_PROBLEM_TWISTS_NOT_OPPOSITE,
    BT_PROBLEM_TWIST_SUM_NOT_ZERO,
} bt_problem_kind;

typedef enum {
    BT_DEFECT_FLIP,
    BT_DEFECT_TWIST,
} bt_defect_kind;

typedef enum {
    BT_NOTE_FLIP,
    BT_NOTE_TWIST,
} bt_note_kind;

typedef struct {
    bt_problem_kind kind;
    char detail[BT_VERIFY_DETAIL_MAX];
    char sticker[BT_VERIFY_NAME_MAX];
    char expected[BT_VERIFY_NAME_MAX];
    char actual[BT_VERIFY_NAME_MAX];
    char piece[BT_VERIFY_NAME_MAX];
    char expected_slot[BT_VERIFY_NAME_MAX];
    char actual_slot[BT_VERIFY_NAME_MAX];
    char first[BT_VERIFY_NAME_MAX];
    char second[BT_VERIFY_NAME_MAX];
    bt_defect_kind defect;
    long long sum;
} bt_problem;

typedef struct {
    bt_note_kind kind;
    char piece[BT_VERIFY_NAME_MAX];
    int8_t twist;
} bt_note;

typedef struct {
    bt_problem *problems;
    size_t problem_count;
    bt_note *notes;
    size_t note_count;
} bt_verdict;

int bt_verify_chirality(const uint8_t triple[3]);
void bt_verdict_init(bt_verdict *verdict);
void bt_verdict_free(bt_verdict *verdict);
bool bt_verdict_ok(const bt_verdict *verdict);
void bt_verdict_string(const bt_verdict *verdict, char *out, size_t out_len);
bool bt_problem_eq(const bt_problem *a, const bt_problem *b);
bool bt_note_eq(const bt_note *a, const bt_note *b);
bool bt_verdict_contains_problem(const bt_verdict *verdict, bt_problem_kind kind,
                                 const char *a_text, const char *b_text, const char *c_text);
bool bt_verdict_contains_note(const bt_verdict *verdict, bt_note_kind kind, const char *piece,
                              int8_t twist);
void bt_verify(const bt_alg *alg, const char *position, bt_sheet_class sheet, bt_verdict *out);

#endif
