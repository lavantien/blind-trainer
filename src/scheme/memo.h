#ifndef BT_SCHEME_MEMO_H
#define BT_SCHEME_MEMO_H

#include "core_deps.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_MEMO_MAX_LETTERS 96
#define BT_MEMO_MAX_PAIRS 48
#define BT_MEMO_MAX_FLIPS 12
#define BT_MEMO_MAX_TWISTS 8

extern const uint8_t BT_SCHEME_CORNER_SUCC[8][3];

typedef struct {
    uint8_t edge[2];
    uint8_t corner[2];
} bt_parity;

typedef struct {
    uint8_t corner;
    uint8_t quarter;
} bt_twist;

typedef enum {
    BT_RESIDUE_NONE = 0,
    BT_RESIDUE_FLIP_PAIR,
    BT_RESIDUE_TWIST_PAIR,
    BT_RESIDUE_TWIST_TRIPLE,
    BT_RESIDUE_PARITY,
    BT_RESIDUE_LTCT,
    BT_RESIDUE_COMPOUND,
} bt_residue_kind;

typedef struct {
    bt_residue_kind kind;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} bt_residue_class;

typedef struct {
    bool has_parity;
    bt_parity parity;
    uint8_t flip_count;
    uint8_t flips[BT_MEMO_MAX_FLIPS];
    uint8_t twist_count;
    bt_twist twists[BT_MEMO_MAX_TWISTS];
} bt_residue;

typedef struct {
    size_t edge_letter_count;
    char edge_letters[BT_MEMO_MAX_LETTERS];
    size_t corner_letter_count;
    char corner_letters[BT_MEMO_MAX_LETTERS];
    size_t edge_pair_count;
    bt_case edge_pairs[BT_MEMO_MAX_PAIRS];
    size_t corner_pair_count;
    bt_case corner_pairs[BT_MEMO_MAX_PAIRS];
    size_t edge_buffer_count;
    uint8_t edge_buffers[BT_MEMO_MAX_PAIRS];
    size_t corner_buffer_count;
    uint8_t corner_buffers[BT_MEMO_MAX_PAIRS];
    bt_residue residue;
} bt_memo;

typedef struct {
    bt_orbit orbit;
    uint8_t *occ;
    uint8_t pieces[12];
    size_t piece_count;
    bool reserved[12];
    bool visited[48];
    size_t letter_count;
    char letters[BT_MEMO_MAX_LETTERS];
    size_t pair_count;
    bt_case pairs[BT_MEMO_MAX_PAIRS];
    size_t buffer_count;
    uint8_t buffers[BT_MEMO_MAX_PAIRS];
    uint8_t pending[2];
    size_t pending_count;
    bool has_parity;
    bt_parity parity;
} bt_orbit_walk;

typedef struct {
    uint8_t count;
    uint8_t items[3][3];
} bt_scheme_cycles;

void bt_scheme_state_array(const bt_cube *cube, uint8_t out[54]);
uint8_t bt_scheme_corner_succ(uint8_t pos);
uint8_t bt_scheme_edge_mate(uint8_t pos);
bool bt_scheme_edge_flipped(const uint8_t occ[54], uint8_t slot);
uint8_t bt_scheme_corner_twist_state(const uint8_t occ[54], uint8_t slot);
void bt_scheme_ideal_cycle(const bt_case *case_id, bt_scheme_cycles *out);
void bt_scheme_ideal_cycle_at(bt_orbit orbit, uint8_t buffer, uint8_t t1, uint8_t t2,
                              bt_scheme_cycles *out);
void bt_scheme_apply_sticker_cycles(uint8_t occ[54], const bt_scheme_cycles *cycles);
void bt_scheme_apply_aligned_swap(uint8_t occ[54], bt_orbit orbit, uint8_t h1, uint8_t h2);
void bt_scheme_apply_residue(uint8_t occ[54], const bt_residue *residue);
void bt_scheme_apply_memo(uint8_t occ[54], const bt_memo *memo);
bt_residue_class bt_scheme_residue_class(const bt_residue *residue);
void bt_scheme_memo_from_array(const uint8_t occ[54], bt_memo *out);
void bt_scheme_memo(const bt_cube *cube, bt_memo *out);
void bt_scheme_memo_occ(const uint8_t occ[54], bt_memo *out);
void bt_scheme_walk_init(bt_orbit_walk *walk, bt_orbit orbit, uint8_t occ[54]);
void bt_scheme_walk_run(bt_orbit_walk *walk);
bool bt_scheme_residue_eq(const bt_residue *a, const bt_residue *b);
bool bt_scheme_memo_eq(const bt_memo *a, const bt_memo *b);
bool bt_scheme_residue_class_eq(bt_residue_class a, bt_residue_class b);

#endif
