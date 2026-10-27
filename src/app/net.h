#ifndef BT_APP_NET_H
#define BT_APP_NET_H

#include "app/logic.h"
#include "app/theme.h"

#include <stddef.h>

#define BT_NET_GRID_CELLS 54
#define BT_NET_BAND_COUNT 3
#define BT_NET_BAND_SLOTS 4

typedef struct {
    float quad;
    float gap;
} bt_net_geometry;

typedef struct {
    size_t face;
    size_t row;
    size_t col;
} bt_net_cell;

typedef struct {
    bt_theme_color background;
    bt_theme_color border;
} bt_net_cell_ink;

extern const int BT_NET_BANDS[BT_NET_BAND_COUNT][BT_NET_BAND_SLOTS];

int bt_net_face_of(unsigned char sticker);
bt_theme_color bt_net_ink_for(bt_theme_color color);
bt_theme_color bt_net_letter_color(size_t face);
void bt_net_grid_of(unsigned char sticker, bt_net_cell *out);
size_t bt_net_grid_index(unsigned char sticker);
int bt_net_sticker_at_grid(size_t grid);
float bt_net_cell_stroke(float quad);
float bt_net_face_stroke(float quad);
float bt_net_face_plate(bt_net_geometry geo);
bt_net_geometry bt_net_for_window(float width, float height);
bt_net_geometry bt_net_for_region(float width, float height);
float bt_net_width(const bt_net_geometry *geo);
float bt_net_height(const bt_net_geometry *geo);
void bt_net_update_cells(const bt_app_net_state *net, float pulse[BT_NET_GRID_CELLS], float dt,
                         float elapsed, int frozen, bt_net_cell_ink out[BT_NET_GRID_CELLS]);

#endif
