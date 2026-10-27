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
} bt_netdisp_cell;

typedef enum {
    BT_NETDISP_SLOT_PAD,
    BT_NETDISP_SLOT_CELL,
} bt_netdisp_slot_kind;

typedef struct {
    bt_netdisp_slot_kind kind;
    bt_netdisp_cell cell;
} bt_netdisp_slot;

typedef struct {
    size_t face;
    size_t width;
    size_t row_count;
    size_t row_lengths[BT_PZ_MAX_ROWS];
    bt_netdisp_slot rows[BT_PZ_MAX_ROWS][BT_PZ_MAX_COLS];
} bt_netdisp_face;

typedef enum {
    BT_NETDISP_BAND_FACE,
    BT_NETDISP_BAND_SPACER,
} bt_net_band_kind;

typedef struct {
    bt_net_band_kind kind;
    bt_netdisp_face face;
    size_t spacer;
} bt_netdisp_band_slot;

#define BT_NETDISP_LAYOUT_MAX_BANDS 8
#define BT_NETDISP_LAYOUT_MAX_SLOTS 4

typedef struct {
    size_t band_count;
    size_t band_slot_count[BT_NETDISP_LAYOUT_MAX_BANDS];
    bt_netdisp_band_slot bands[BT_NETDISP_LAYOUT_MAX_BANDS][BT_NETDISP_LAYOUT_MAX_SLOTS];
} bt_netdisp_layout;

void bt_netdisp_layout_of(const bt_pz_grid *grid, bt_netdisp_layout *out);
bt_theme_color bt_netdisp_cell_color(const bt_netdisp_cell *cell);

#endif
