#ifndef BT_APP_VIEWS_ALGS_DISPLAY_H
#define BT_APP_VIEWS_ALGS_DISPLAY_H

#include "app/theme.h"
#include "puzzles/geometry.h"

#include <stddef.h>

typedef struct {
    size_t face;
    size_t row;
    size_t col;
    unsigned char color;
} bt_net_cell;

typedef enum {
    BT_NET_SLOT_PAD,
    BT_NET_SLOT_CELL,
} bt_net_slot_kind;

typedef struct {
    bt_net_slot_kind kind;
    bt_net_cell cell;
} bt_net_slot;

typedef struct {
    size_t face;
    size_t width;
    size_t row_count;
    size_t row_lengths[BT_PZ_MAX_ROWS];
    bt_net_slot rows[BT_PZ_MAX_ROWS][BT_PZ_MAX_COLS];
} bt_net_face;

typedef enum {
    BT_NET_BAND_FACE,
    BT_NET_BAND_SPACER,
} bt_net_band_kind;

typedef struct {
    bt_net_band_kind kind;
    bt_net_face face;
    size_t spacer;
} bt_net_band_slot;

#define BT_NET_LAYOUT_MAX_BANDS 8
#define BT_NET_LAYOUT_MAX_SLOTS 4

typedef struct {
    size_t band_count;
    size_t band_slot_count[BT_NET_LAYOUT_MAX_BANDS];
    bt_net_band_slot bands[BT_NET_LAYOUT_MAX_BANDS][BT_NET_LAYOUT_MAX_SLOTS];
} bt_net_layout;

void bt_net_display_layout(const bt_pz_grid *grid, bt_net_layout *out);
bt_theme_color bt_net_display_cell_color(const bt_net_cell *cell);

#endif
