#ifndef BT_APP_VIEWS_DRILL_VIEW_H
#define BT_APP_VIEWS_DRILL_VIEW_H

#include "app/logic.h"
#include "app/net.h"
#include "app/views/drill.h"
#include "app/views/drill_card.h"
#include "app/views/state.h"

#include <stddef.h>

typedef struct {
    float net_x;
    float net_y;
    float net_w;
    float net_h;
    float card_x;
    float card_y;
    float card_w;
    float card_h;
} bt_view_drill_layout;

bt_view_drill_layout bt_view_drill_layout_for(float content_x, float content_y, float content_w,
                                              float content_h);
bt_net_geometry bt_view_drill_net_geometry(float window_w, float window_h);
void bt_view_drill_net_update(bt_app_net_state *net, bt_views_net *pulse, float dt);
void bt_view_drill_net_draw(const void *font_bold, const bt_views_net *pulse, float region_x,
                            float region_y, float region_w, float region_h);
void bt_view_drill_card_draw(const bt_card_content *content, const void *font_regular, float x,
                             float y, float h);
void bt_view_drill_reps_draw(const bt_drill_session *session, const void *font_regular, float x,
                             float y, float h);
void bt_view_drill_rate_draw(const bt_drill_session *session, const void *font_regular, float x,
                             float y, float h);

#endif
