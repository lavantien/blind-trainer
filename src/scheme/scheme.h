#ifndef BT_SCHEME_SCHEME_H
#define BT_SCHEME_SCHEME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BT_ORBIT_EDGE = 0,
    BT_ORBIT_CORNER = 1,
} bt_orbit;

#define BT_SCHEME_EDGE_BUFFER_STICKER 26u
#define BT_SCHEME_CORNER_BUFFER_STICKER 2u

extern const char *const BT_SCHEME_EDGE_FLOATING[12];
extern const char *const BT_SCHEME_CORNER_FLOATING[8];

uint8_t bt_scheme_orbit_lo(bt_orbit orbit);
uint8_t bt_scheme_orbit_hi(bt_orbit orbit);
char bt_scheme_letter_of(uint8_t sticker);
uint8_t bt_scheme_sticker_of(bt_orbit orbit, char letter);
const char *bt_scheme_name_of(uint8_t sticker);
bool bt_scheme_same_piece(uint8_t a, uint8_t b);
uint8_t bt_scheme_head_sticker(const char *piece_name);
void bt_scheme_edge_floating_heads(uint8_t out[12]);
void bt_scheme_corner_floating_heads(uint8_t out[8]);

#endif
