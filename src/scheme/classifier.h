#ifndef BT_SCHEME_CLASSIFIER_H
#define BT_SCHEME_CLASSIFIER_H

#include "core_deps.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_TRACE_BLOCKS_MAX 12
#define BT_TRACE_BLOCK_MAX 27
#define BT_CLASS_TEXT_MAX 96
#define BT_CLASS_POSITION_MAX 128

typedef struct {
    bt_orbit orbit;
    size_t block_count;
    uint8_t blocks[BT_TRACE_BLOCKS_MAX][BT_TRACE_BLOCK_MAX];
    size_t block_len[BT_TRACE_BLOCKS_MAX];
} bt_trace;

typedef enum {
    BT_CLASS_SOLVED = 0,
    BT_CLASS_THREE_STYLE,
    BT_CLASS_FLIP_PAIR,
    BT_CLASS_TWIST_PAIR,
    BT_CLASS_PARITY,
    BT_CLASS_LTCT,
    BT_CLASS_UNCLASSIFIED,
} bt_class_kind;

typedef struct {
    bt_class_kind kind;
    bt_orbit orbit;
    size_t case_count;
    bt_case cases[BT_TRACE_BLOCKS_MAX];
    uint8_t first;
    uint8_t second;
    uint8_t e1;
    uint8_t e2;
    uint8_t c1;
    uint8_t c2;
    uint8_t bracket;
    char unclassified[BT_CLASS_TEXT_MAX];
} bt_classification;

typedef struct {
    uint8_t head;
    bool flipped;
} bt_edge_flip_note;

typedef struct {
    uint8_t head;
    int8_t twist;
} bt_corner_twist_note;

typedef struct {
    bt_edge_flip_note edge_flips[12];
    bt_corner_twist_note corner_twists[8];
} bt_defects;

typedef struct {
    bt_trace edge;
    bt_trace corner;
    bt_classification class;
    bt_defects defects;
    bool has_qtm;
    uint32_t qtm;
} bt_report;

bool bt_scheme_trace_is_solved(const bt_trace *trace);
size_t bt_scheme_trace_cases(const bt_trace *trace, bt_case out[BT_TRACE_BLOCKS_MAX]);
void bt_scheme_trace(const uint8_t occ[54], bt_orbit orbit, bt_trace *out);
void bt_scheme_classify(const uint8_t occ[54], bt_report *out);
void bt_scheme_classify_alg(const bt_alg *alg, bt_report *out);
bool bt_scheme_report_position(const bt_report *report, char out[BT_CLASS_POSITION_MAX]);
bool bt_scheme_defect_flip(const bt_defects *defects, uint8_t piece);
int8_t bt_scheme_defect_twist(const bt_defects *defects, uint8_t piece);
size_t bt_scheme_flipped_pieces(const bt_defects *defects, uint8_t out[12]);
size_t bt_scheme_twisted_pieces(const bt_defects *defects, bt_corner_twist_note out[8]);
bool bt_scheme_is_style_block(const uint8_t *block, size_t len);
bool bt_scheme_is_flip_block(const uint8_t *block, size_t len);
bool bt_scheme_is_swap_block(const uint8_t *block, size_t len);
bool bt_scheme_is_twist_block(const uint8_t *block, size_t len);
bool bt_scheme_is_swap_twist_block(const uint8_t *block, size_t len);
bool bt_scheme_classification_eq(const bt_classification *a, const bt_classification *b);
bool bt_scheme_trace_eq(const bt_trace *a, const bt_trace *b);

#endif
