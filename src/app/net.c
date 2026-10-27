#include "app/net.h"

#include "common/bt_trace.h"
#include "scheme/scheme.h"

#include <math.h>

typedef struct {
    unsigned char letter;
    int is_row;
    unsigned char pos;
} bt_net_side_rule;

static const bt_net_side_rule BT_NET_SIDE_RULES[BT_THEME_FACE_COUNT][4] = {
    {{'B', 1, 0}, {'F', 1, 2}, {'L', 0, 0}, {'R', 0, 2}},
    {{'U', 1, 0}, {'D', 1, 2}, {'B', 0, 0}, {'F', 0, 2}},
    {{'U', 1, 0}, {'D', 1, 2}, {'L', 0, 0}, {'R', 0, 2}},
    {{'U', 1, 0}, {'D', 1, 2}, {'F', 0, 0}, {'B', 0, 2}},
    {{'U', 1, 0}, {'D', 1, 2}, {'R', 0, 0}, {'L', 0, 2}},
    {{'F', 1, 0}, {'B', 1, 2}, {'L', 0, 0}, {'R', 0, 2}},
};

const int BT_NET_BANDS[BT_NET_BAND_COUNT][BT_NET_BAND_SLOTS] = {
    {-1, 0, -1, -1},
    {1, 2, 3, 4},
    {-1, 5, -1, -1},
};

static int grid_to_sticker_ready = 0;
static int grid_to_sticker[BT_NET_GRID_CELLS];

static void build_grid_to_sticker(void) {
    for (int grid = 0; grid < BT_NET_GRID_CELLS; grid++) {
        grid_to_sticker[grid] = -1;
    }
    for (int sticker = 0; sticker < 48; sticker++) {
        grid_to_sticker[bt_net_grid_index((unsigned char)sticker)] = sticker;
    }
    grid_to_sticker_ready = 1;
}

int bt_net_sticker_at_grid(size_t grid) {
    BT_TRACE_ENTER("app.net_sticker_at_grid");
    if (!grid_to_sticker_ready) {
        build_grid_to_sticker();
    }
    if (grid >= BT_NET_GRID_CELLS) {
        BT_TRACE_FAIL("app.net_sticker_at_grid", "grid %zu out of range", grid);
        BT_TRACE_EXIT("app.net_sticker_at_grid");
        return -1;
    }
    BT_TRACE_EXIT("app.net_sticker_at_grid");
    return grid_to_sticker[grid];
}

int bt_net_face_of(unsigned char sticker) {
    BT_TRACE_ENTER("app.net_face_of");
    const char *name = bt_scheme_name_of(sticker);
    int face = -1;
    switch (name[0]) {
    case 'U':
        face = 0;
        break;
    case 'L':
        face = 1;
        break;
    case 'F':
        face = 2;
        break;
    case 'R':
        face = 3;
        break;
    case 'B':
        face = 4;
        break;
    case 'D':
        face = 5;
        break;
    default:
        BT_TRACE_FAIL("app.net_face_of", "sticker face byte unexpected: %c", name[0]);
        break;
    }
    BT_TRACE_EXIT("app.net_face_of");
    return face;
}

bt_theme_color bt_net_ink_for(bt_theme_color color) {
    BT_TRACE_ENTER("app.net_ink_for");
    bt_theme_color linear = bt_theme_to_linear(color);
    float luminance =
        0.2126F * linear.red + 0.7152F * linear.green + 0.0722F * linear.blue;
    bt_theme_color ink;
    if (luminance >= 0.5F) {
        ink = (bt_theme_color){0.08F, 0.08F, 0.08F, 1.0F};
    } else {
        ink = (bt_theme_color){0.96F, 0.96F, 0.96F, 1.0F};
    }
    BT_TRACE_EXIT("app.net_ink_for");
    return ink;
}

bt_theme_color bt_net_letter_color(size_t face) {
    BT_TRACE_ENTER("app.net_letter_color");
    bt_theme_color color = bt_net_ink_for(bt_theme_face_color(face));
    BT_TRACE_EXIT("app.net_letter_color");
    return color;
}

void bt_net_grid_of(unsigned char sticker, bt_net_cell *out) {
    BT_TRACE_ENTER("app.net_grid_of");
    int face = bt_net_face_of(sticker);
    const char *name = bt_scheme_name_of(sticker);
    size_t row = 1;
    size_t col = 1;
    for (const char *cursor = name + 1; *cursor != '\0'; cursor++) {
        for (int rule = 0; rule < 4; rule++) {
            const bt_net_side_rule *side = &BT_NET_SIDE_RULES[face][rule];
            if (side->letter == (unsigned char)*cursor) {
                if (side->is_row) {
                    row = side->pos;
                } else {
                    col = side->pos;
                }
            }
        }
    }
    out->face = (size_t)face;
    out->row = row;
    out->col = col;
    BT_TRACE_EXIT("app.net_grid_of");
}

size_t bt_net_grid_index(unsigned char sticker) {
    BT_TRACE_ENTER("app.net_grid_index");
    bt_net_cell cell;
    bt_net_grid_of(sticker, &cell);
    BT_TRACE_EXIT("app.net_grid_index");
    return cell.face * 9 + cell.row * 3 + cell.col;
}

static float clampf(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

float bt_net_cell_stroke(float quad) {
    return clampf(quad * 0.06F, 1.5F, 4.0F);
}

float bt_net_face_stroke(float quad) {
    return clampf(quad * 0.08F, 2.0F, 5.0F);
}

float bt_net_face_plate(bt_net_geometry geo) {
    BT_TRACE_ENTER("app.net_face_plate");
    float plate = geo.quad * 3.0F + geo.gap * 2.0F + bt_net_face_stroke(geo.quad) * 4.0F;
    BT_TRACE_EXIT("app.net_face_plate");
    return plate;
}

bt_net_geometry bt_net_for_region(float width, float height) {
    BT_TRACE_ENTER("app.net_for_region");
    bt_net_geometry geo;
    float quad = floorf(fminf(width / 14.7F, height / 11.0F));
    geo.quad = clampf(quad, 16.0F, 96.0F);
    geo.gap = fmaxf(geo.quad * 0.12F, 2.0F);
    BT_TRACE_EXIT("app.net_for_region");
    return geo;
}

bt_net_geometry bt_net_for_window(float width, float height) {
    return bt_net_for_region(width, height);
}

float bt_net_width(const bt_net_geometry *geo) {
    BT_TRACE_ENTER("app.net_width");
    float width = bt_net_face_plate(*geo) * 4.0F + geo->gap * 3.0F;
    BT_TRACE_EXIT("app.net_width");
    return width;
}

float bt_net_height(const bt_net_geometry *geo) {
    BT_TRACE_ENTER("app.net_height");
    float height = bt_net_face_plate(*geo) * 3.0F + geo->gap * 2.0F;
    BT_TRACE_EXIT("app.net_height");
    return height;
}

void bt_net_update_cells(const bt_app_net_state *net, float pulse[BT_NET_GRID_CELLS], float dt,
                         float elapsed, int frozen, bt_net_cell_ink out[BT_NET_GRID_CELLS]) {
    BT_TRACE_ENTER("app.net_update_cells");
    const bt_theme *theme = bt_theme_table();
    const float tau = 6.283185307F;
    float wave = 0.6F + 0.4F * sinf(elapsed * tau * theme->pulse_hz);
    float step = dt * theme->pulse_lerp_per_s;
    for (int grid = 0; grid < BT_NET_GRID_CELLS; grid++) {
        size_t face = (size_t)grid / 9;
        bt_theme_color base = bt_theme_face_color(face);
        bt_theme_color highlight = {0, 0, 0, 0};
        int has_highlight = 0;
        float want_pulse = 0.0F;
        int sticker = bt_net_sticker_at_grid((size_t)grid);
        if (sticker >= 0) {
            unsigned char occupant = bt_cube_sticker_at(&net->cube, (unsigned char)sticker);
            base = bt_theme_face_color((size_t)bt_net_face_of(occupant));
            bt_app_highlight_kind kind;
            if (bt_app_net_highlight_kind(net, occupant, &kind)) {
                want_pulse = 1.0F;
                has_highlight = 1;
                highlight = kind == BT_APP_HIGHLIGHT_BUFFER ? bt_theme_teal()
                                                           : bt_theme_interchange();
            }
        }
        float delta = clampf(want_pulse - pulse[grid], -step, step);
        pulse[grid] += delta;
        float factor = frozen ? 1.0F : pulse[grid] * wave;
        bt_theme_color background =
            has_highlight ? bt_theme_mix(base, highlight, factor) : base;
        out[grid].background = background;
        out[grid].border = bt_net_ink_for(background);
    }
    BT_TRACE_EXIT("app.net_update_cells");
}
