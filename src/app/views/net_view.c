#include "app/views/net_view.h"

#include "app/window.h"
#include "common/bt_trace.h"

#include "raylib.h"

static float net_center_offset(float total, float used) {
    return (total - used) / 2.0F;
}

void bt_view_net_center(const bt_net_geometry *geo, float region_x, float region_y, float region_w,
                        float region_h, float *out_x, float *out_y) {
    BT_TRACE_ENTER("app.view_net_center");
    float width = bt_net_width(geo);
    float height = bt_net_height(geo);
    *out_x = region_x + net_center_offset(region_w, width);
    *out_y = region_y + net_center_offset(region_h, height);
    BT_TRACE_EXIT("app.view_net_center");
}

static void draw_cell(const void *font_bold, const bt_net_geometry *geo, size_t face, size_t row,
                      size_t col, const bt_net_cell_ink ink[BT_NET_GRID_CELLS], float x, float y) {
    size_t grid = face * 9 + row * 3 + col;
    Rectangle cell = {x, y, geo->quad, geo->quad};
    DrawRectangleRounded(cell, 0.3F, 0, bt_app_color(ink[grid].background));
    DrawRectangleRoundedLinesEx(cell, 0.3F, 0, bt_net_cell_stroke(geo->quad),
                                bt_app_color(ink[grid].border));
    if (row == 1 && col == 1) {
        const char *name = bt_theme_face_name(face);
        float size = geo->quad * 0.45F;
        Font bold = *(const Font *)font_bold;
        Vector2 box = MeasureTextEx(bold, name, size, 1.0F);
        bt_app_draw_text(font_bold, name, x + net_center_offset(geo->quad, box.x),
                         y + net_center_offset(geo->quad, box.y), size, bt_net_letter_color(face));
    }
}

static void draw_face(const void *font_bold, const bt_net_geometry *geo, size_t face,
                      const bt_net_cell_ink ink[BT_NET_GRID_CELLS], float x, float y) {
    float stroke = bt_net_face_stroke(geo->quad);
    float plate = bt_net_face_plate(*geo);
    Rectangle frame = {x, y, plate, plate};
    DrawRectangleLinesEx(frame, stroke, bt_app_color(bt_theme_text_primary()));
    float inner = x + stroke;
    float top = y + stroke;
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 3; col++) {
            draw_cell(font_bold, geo, face, row, col, ink,
                      inner + (float)col * (geo->quad + geo->gap),
                      top + (float)row * (geo->quad + geo->gap));
        }
    }
}

void bt_view_net_draw(const void *font_bold, const bt_net_geometry *geo,
                      const bt_net_cell_ink ink[BT_NET_GRID_CELLS], float x, float y) {
    BT_TRACE_ENTER("app.view_net_draw");
    float plate = bt_net_face_plate(*geo);
    for (int band = 0; band < BT_NET_BAND_COUNT; band++) {
        float band_y = y + (float)band * (plate + geo->gap);
        for (int slot = 0; slot < BT_NET_BAND_SLOTS; slot++) {
            int face = BT_NET_BANDS[band][slot];
            if (face < 0) {
                continue;
            }
            draw_face(font_bold, geo, (size_t)face, ink, x + (float)slot * (plate + geo->gap),
                      band_y);
        }
    }
    BT_TRACE_EXIT("app.view_net_draw");
}

void bt_view_net_update(bt_app_net_state *net, bt_views_net *pulse, float dt) {
    BT_TRACE_ENTER("app.view_net_update");
    pulse->elapsed += dt;
    bt_net_update_cells(net, pulse->pulse, dt, (float)pulse->elapsed, pulse->frozen, pulse->ink);
    BT_TRACE_EXIT("app.view_net_update");
}
