#include "app/net.h"

#include "common.h"

#include "scheme/scheme.h"

#include <math.h>
#include <string.h>

typedef struct {
    int x;
    int y;
    int z;
} vec3;

static vec3 axis_unit(unsigned char byte) {
    switch (byte) {
    case 'U':
        return (vec3){0, 1, 0};
    case 'D':
        return (vec3){0, -1, 0};
    case 'R':
        return (vec3){1, 0, 0};
    case 'L':
        return (vec3){-1, 0, 0};
    case 'F':
        return (vec3){0, 0, -1};
    case 'B':
        return (vec3){0, 0, 1};
    default:
        BT_ABORT("axis byte unexpected: %c", byte);
    }
}

typedef struct {
    vec3 pos;
    vec3 normal;
} placement;

static placement sticker_placement(unsigned char sticker) {
    const char *name = bt_scheme_name_of(sticker);
    placement spot = {{0, 0, 0}, {0, 0, 0}};
    spot.normal = axis_unit((unsigned char)name[0]);
    for (const char *cursor = name; *cursor != '\0'; cursor++) {
        vec3 unit = axis_unit((unsigned char)*cursor);
        spot.pos.x += unit.x;
        spot.pos.y += unit.y;
        spot.pos.z += unit.z;
    }
    return spot;
}

static placement cell_placement(size_t face, size_t row, size_t col) {
    int r = (int)row;
    int c = (int)col;
    switch (face) {
    case 0:
        return (placement){{c - 1, 1, 1 - r}, {0, 1, 0}};
    case 1:
        return (placement){{-1, 1 - r, 1 - c}, {-1, 0, 0}};
    case 2:
        return (placement){{c - 1, 1 - r, -1}, {0, 0, -1}};
    case 3:
        return (placement){{1, 1 - r, c - 1}, {1, 0, 0}};
    case 4:
        return (placement){{1 - c, 1 - r, 1}, {0, 0, 1}};
    case 5:
        return (placement){{c - 1, -1, r - 1}, {0, -1, 0}};
    default:
        BT_ABORT("face index unexpected: %zu", face);
    }
}

static int placement_eq(placement a, placement b) {
    return a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z &&
           a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z;
}

static bt_theme_color srgb(float r, float g, float b) {
    bt_theme_color color = {r, g, b, 1.0F};
    return color;
}

static int color_eq(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

static float luminance(bt_theme_color color) {
    bt_theme_color linear = bt_theme_to_linear(color);
    return 0.2126F * linear.red + 0.7152F * linear.green + 0.0722F * linear.blue;
}

BT_TEST(net, grid_matches_physical_unfold) {
    for (int sticker = 0; sticker < 48; sticker++) {
        placement spot = sticker_placement((unsigned char)sticker);
        int hits = 0;
        size_t at_face = 0;
        size_t at_row = 0;
        size_t at_col = 0;
        for (size_t face = 0; face < 6; face++) {
            for (size_t row = 0; row < 3; row++) {
                for (size_t col = 0; col < 3; col++) {
                    if (placement_eq(cell_placement(face, row, col), spot)) {
                        hits++;
                        at_face = face;
                        at_row = row;
                        at_col = col;
                    }
                }
            }
        }
        BT_CHECK_EQ(hits, 1);
        bt_net_cell cell;
        bt_net_grid_of((unsigned char)sticker, &cell);
        BT_CHECK(cell.face == at_face && cell.row == at_row && cell.col == at_col);
        BT_CHECK_EQ(bt_net_grid_index((unsigned char)sticker), at_face * 9 + at_row * 3 + at_col);
    }
}

BT_TEST(net, grid_covers_all_cells_once) {
    int seen[BT_NET_GRID_CELLS] = {0};
    for (int sticker = 0; sticker < 48; sticker++) {
        size_t grid = bt_net_grid_index((unsigned char)sticker);
        BT_CHECK(!seen[grid]);
        seen[grid] = 1;
        BT_CHECK_EQ(bt_net_sticker_at_grid(grid), sticker);
    }
    for (size_t face = 0; face < 6; face++) {
        BT_CHECK_EQ(bt_net_sticker_at_grid(face * 9 + 4), -1);
    }
}

BT_TEST(net, face_of_follows_sticker_name) {
    BT_CHECK_EQ(bt_net_face_of(0), 0);
    BT_CHECK_EQ(bt_net_face_of(26), 0);
    BT_CHECK_EQ(bt_net_face_of(29), 1);
    BT_CHECK_EQ(bt_net_face_of(33), 2);
    BT_CHECK_EQ(bt_net_face_of(36), 3);
    BT_CHECK_EQ(bt_net_face_of(41), 4);
    BT_CHECK_EQ(bt_net_face_of(44), 5);
}

BT_TEST(net, letter_ink_contrasts_face_luminance) {
    bt_theme_color dark = srgb(0.08F, 0.08F, 0.08F);
    bt_theme_color light = srgb(0.96F, 0.96F, 0.96F);
    BT_CHECK(color_eq(bt_net_letter_color(0), dark));
    BT_CHECK(color_eq(bt_net_letter_color(5), dark));
    BT_CHECK(color_eq(bt_net_letter_color(3), light));
    BT_CHECK(color_eq(bt_net_letter_color(4), light));
}

BT_TEST(net, ink_for_contrasts_faces_highlights_and_mixes) {
    for (size_t face = 0; face < BT_THEME_FACE_COUNT; face++) {
        bt_theme_color colors[3];
        colors[0] = bt_theme_face_color(face);
        colors[1] = bt_theme_mix(colors[0], bt_theme_teal(), 0.5F);
        colors[2] = bt_theme_mix(colors[0], bt_theme_interchange(), 0.5F);
        for (int i = 0; i < 3; i++) {
            bt_theme_color ink = bt_net_ink_for(colors[i]);
            BT_CHECK(fabsf(luminance(ink) - luminance(colors[i])) >= 0.35F);
        }
    }
}

BT_TEST(net, stroke_widths_track_quad_within_clamps) {
    BT_CHECK_EQ(bt_net_cell_stroke(16.0F), 1.5F);
    BT_CHECK_F64_NEAR(bt_net_cell_stroke(41.0F), 2.46, 1e-5);
    BT_CHECK_EQ(bt_net_cell_stroke(96.0F), 4.0F);
    BT_CHECK_EQ(bt_net_face_stroke(16.0F), 2.0F);
    BT_CHECK_F64_NEAR(bt_net_face_stroke(41.0F), 3.28, 1e-5);
    BT_CHECK_EQ(bt_net_face_stroke(96.0F), 5.0F);
}

BT_TEST(net, net_footprint_sums_cells_gaps_and_face_strokes) {
    bt_net_geometry geo = bt_net_for_region(563.2F, 690.0F);
    BT_CHECK_EQ(geo.quad, 38.0F);
    BT_CHECK_F64_NEAR(bt_net_width(&geo), 554.8, 1e-4);
    BT_CHECK_F64_NEAR(bt_net_height(&geo), 414.96, 1e-4);
}

BT_TEST(net, bands_seat_faces_over_the_f_slot) {
    const int expected[BT_NET_BAND_COUNT][BT_NET_BAND_SLOTS] = {
        {-1, 0, -1, -1},
        {1, 2, 3, 4},
        {-1, 5, -1, -1},
    };
    for (int band = 0; band < BT_NET_BAND_COUNT; band++) {
        for (int slot = 0; slot < BT_NET_BAND_SLOTS; slot++) {
            BT_CHECK_EQ(BT_NET_BANDS[band][slot], expected[band][slot]);
        }
    }
}

BT_TEST(net, geometry_fits_configured_window) {
    bt_net_geometry geo = bt_net_for_window(1280.0F, 800.0F);
    BT_CHECK(geo.quad >= 16.0F && geo.quad <= 96.0F);
    BT_CHECK(bt_net_width(&geo) <= 1280.0F);
    BT_CHECK(bt_net_height(&geo) <= 800.0F);
}

BT_TEST(net, geometry_fits_window_with_view_reserve) {
    const bt_theme *theme = bt_theme_table();
    float reserve = theme->root_rem * theme->net_view_reserve_rem;
    bt_net_geometry geo = bt_net_for_window(1280.0F, 800.0F - reserve);
    BT_CHECK(bt_net_height(&geo) + reserve <= 800.0F);
    BT_CHECK(bt_net_width(&geo) <= 1280.0F);
}

BT_TEST(net, geometry_clamps_tiny_windows) {
    bt_net_geometry geo = bt_net_for_window(100.0F, 60.0F);
    BT_CHECK_EQ(geo.quad, 16.0F);
}

BT_TEST(net, for_region_sizes_the_drill_left_region) {
    const bt_theme *theme = bt_theme_table();
    float width = 1280.0F * theme->drill_net_width_pct;
    float chrome = theme->root_rem * (theme->header_height_rem + 2.0F * theme->panel_padding_rem);
    bt_net_geometry geo = bt_net_for_region(width, 800.0F - chrome);
    BT_CHECK_EQ(geo.quad, 38.0F);
    BT_CHECK(bt_net_width(&geo) <= width);
    BT_CHECK(bt_net_height(&geo) <= 800.0F - chrome);
}

static bt_app_net_state targeted_net(void) {
    bt_app_net_state net;
    bt_app_net_state_default(&net);
    int first = bt_net_sticker_at_grid(1);
    int second = bt_net_sticker_at_grid(3);
    BT_CHECK(first >= 0 && second >= 0);
    net.has_target[0] = 1;
    net.targets[0] = (unsigned char)first;
    net.has_target[1] = 1;
    net.targets[1] = (unsigned char)second;
    return net;
}

static void run_pulse(const bt_app_net_state *net, float elapsed, int frozen,
                      bt_net_cell_ink out[BT_NET_GRID_CELLS]) {
    float pulse[BT_NET_GRID_CELLS] = {0};
    bt_net_update_cells(net, pulse, 0.016F, elapsed, frozen, out);
}

BT_TEST(net, frozen_pulse_renders_the_same_highlights_whatever_the_elapsed_time) {
    bt_app_net_state net = targeted_net();
    bt_net_cell_ink early[BT_NET_GRID_CELLS];
    bt_net_cell_ink late[BT_NET_GRID_CELLS];
    run_pulse(&net, 0.3F, 1, early);
    run_pulse(&net, 3.7F, 1, late);
    for (int grid = 0; grid < BT_NET_GRID_CELLS; grid++) {
        BT_CHECK(color_eq(early[grid].background, late[grid].background));
    }
}

BT_TEST(net, live_pulse_keeps_varying_with_the_elapsed_time) {
    bt_app_net_state net = targeted_net();
    bt_net_cell_ink early[BT_NET_GRID_CELLS];
    bt_net_cell_ink late[BT_NET_GRID_CELLS];
    run_pulse(&net, 0.3F, 0, early);
    run_pulse(&net, 3.7F, 0, late);
    int differs = 0;
    for (int grid = 0; grid < BT_NET_GRID_CELLS; grid++) {
        if (!color_eq(early[grid].background, late[grid].background)) {
            differs = 1;
        }
    }
    BT_CHECK(differs);
}

BT_TEST(net, update_cells_keeps_border_ink_matched) {
    bt_app_net_state net;
    bt_app_net_state_default(&net);
    bt_net_cell_ink ink[BT_NET_GRID_CELLS];
    run_pulse(&net, 0.05F, 0, ink);
    for (int grid = 0; grid < BT_NET_GRID_CELLS; grid++) {
        BT_CHECK(color_eq(ink[grid].border, bt_net_ink_for(ink[grid].background)));
    }
}
