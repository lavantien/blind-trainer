#ifndef BT_APP_VIEWS_NET_VIEW_H
#define BT_APP_VIEWS_NET_VIEW_H

#include "app/net.h"
#include "app/theme.h"

#include <stddef.h>

void bt_view_net_center(const bt_net_geometry *geo, float region_x, float region_y, float region_w,
                        float region_h, float *out_x, float *out_y);
void bt_view_net_draw(const void *font_bold, const bt_net_geometry *geo,
                      const bt_net_cell_ink ink[BT_NET_GRID_CELLS], float x, float y);

#endif
