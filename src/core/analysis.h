#ifndef BT_CORE_ANALYSIS_H
#define BT_CORE_ANALYSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/cube.h"

typedef enum {
    BT_PIECE_CORNER = 0,
    BT_PIECE_EDGE = 1,
} bt_piece_kind;

typedef struct {
    bt_piece_kind kind;
    uint8_t index;
} bt_piece;

#define BT_CYCLES_MAX 24

typedef struct {
    uint8_t count;
    uint8_t lens[BT_CYCLES_MAX];
    uint8_t items[48];
} bt_cycles;

typedef struct {
    uint8_t piece;
    int8_t twist;
} bt_corner_state;

typedef struct {
    uint8_t piece;
    bool flipped;
} bt_edge_state;

typedef struct {
    bt_corner_state corners[8];
    bt_edge_state edges[12];
    bt_cycles cycles;
} bt_state_summary;

bt_piece bt_piece_of(uint8_t sticker);
uint8_t bt_head_sticker(bt_piece piece);
void bt_corner_stickers(uint8_t corner, uint8_t out[3]);
void bt_edge_stickers(uint8_t edge, uint8_t out[2]);
bt_cycles bt_sticker_cycles(const bt_cube *cube);
bool bt_edge_flip(const bt_cube *cube, uint8_t edge);
int8_t bt_corner_twist(const bt_cube *cube, uint8_t corner);
bt_state_summary bt_summarize(const bt_cube *cube);

#endif
