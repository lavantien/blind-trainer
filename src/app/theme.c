#include "app/theme.h"

#include "common/bt_trace.h"

#include <math.h>

static const bt_theme BT_THEME_TABLE = {
    .root_rem = 20.0F,
    .font_focal = 3.4F,
    .font_title = 2.2F,
    .font_alg = 1.7F,
    .font_heading = 1.3F,
    .font_body = 1.0F,
    .font_small = 0.8F,
    .font_micro = 0.75F,
    .spacing_rem = 0.5F,
    .panel_padding_rem = 1.0F,
    .header_height_rem = 3.5F,
    .hairline_rem = 0.05F,
    .drill_net_width_pct = 0.44F,
    .pulse_hz = 2.0F,
    .net_view_reserve_rem = 8.0F,
    .dynamic_band_rem = 16.5F,
    .pulse_lerp_per_s = 8.0F,
};

static const float BT_THEME_FACE_SHADES[BT_THEME_PALETTE_COUNT][3] = {
    {0.95F, 0.95F, 0.95F}, {0.95F, 0.60F, 0.15F}, {0.20F, 0.75F, 0.35F}, {0.85F, 0.20F, 0.20F},
    {0.20F, 0.45F, 0.90F}, {0.95F, 0.85F, 0.15F}, {0.15F, 0.85F, 0.90F}, {0.90F, 0.35F, 0.80F},
    {0.55F, 0.30F, 0.85F}, {0.60F, 0.40F, 0.20F}, {0.55F, 0.58F, 0.62F}, {0.10F, 0.55F, 0.55F},
};

static const char *const BT_THEME_FACE_NAMES[BT_THEME_FACE_COUNT] = {"U", "L", "F", "R", "B", "D"};

const bt_theme *bt_theme_table(void) {
    return &BT_THEME_TABLE;
}

static bt_theme_color shade_color(const float shade[3]) {
    bt_theme_color color = {shade[0], shade[1], shade[2], 1.0F};
    return color;
}

bt_theme_color bt_theme_palette_color(size_t index) {
    BT_TRACE_ENTER("app.theme_palette_color");
    bt_theme_color color = shade_color(BT_THEME_FACE_SHADES[index % BT_THEME_PALETTE_COUNT]);
    BT_TRACE_EXIT("app.theme_palette_color");
    return color;
}

bt_theme_color bt_theme_face_color(size_t face) {
    return bt_theme_palette_color(face);
}

const char *bt_theme_face_name(size_t face) {
    BT_TRACE_ENTER("app.theme_face_name");
    const char *name = BT_THEME_FACE_NAMES[face % BT_THEME_FACE_COUNT];
    BT_TRACE_EXIT("app.theme_face_name");
    return name;
}

float bt_theme_srgb_to_linear(float value) {
    if (value <= 0.0F) {
        return value;
    }
    if (value <= 0.04045F) {
        return value / 12.92F;
    }
    return powf((value + 0.055F) / 1.055F, 2.4F);
}

bt_theme_color bt_theme_to_linear(bt_theme_color color) {
    bt_theme_color linear;
    linear.red = bt_theme_srgb_to_linear(color.red);
    linear.green = bt_theme_srgb_to_linear(color.green);
    linear.blue = bt_theme_srgb_to_linear(color.blue);
    linear.alpha = color.alpha;
    return linear;
}

bt_theme_color bt_theme_mix(bt_theme_color a, bt_theme_color b, float factor) {
    float rest = 1.0F - factor;
    bt_theme_color mixed;
    mixed.red = a.red * rest + b.red * factor;
    mixed.green = a.green * rest + b.green * factor;
    mixed.blue = a.blue * rest + b.blue * factor;
    mixed.alpha = a.alpha * rest + b.alpha * factor;
    return mixed;
}

#define BT_THEME_COLOR_FN(name, r, g, b)                                                           \
    bt_theme_color bt_theme_##name(void) {                                                         \
        BT_TRACE_ENTER("app.theme_" #name);                                                        \
        bt_theme_color color = {r, g, b, 1.0F};                                                    \
        BT_TRACE_EXIT("app.theme_" #name);                                                         \
        return color;                                                                              \
    }

BT_THEME_COLOR_FN(background, 0.09F, 0.10F, 0.12F)
BT_THEME_COLOR_FN(panel, 0.14F, 0.15F, 0.18F)
BT_THEME_COLOR_FN(panel_raised, 0.19F, 0.20F, 0.24F)
BT_THEME_COLOR_FN(text_primary, 0.92F, 0.93F, 0.95F)
BT_THEME_COLOR_FN(text_muted, 0.62F, 0.64F, 0.68F)
BT_THEME_COLOR_FN(accent, 0.35F, 0.66F, 0.95F)
BT_THEME_COLOR_FN(danger, 0.92F, 0.35F, 0.35F)
BT_THEME_COLOR_FN(teal, 0.30F, 0.90F, 0.85F)
BT_THEME_COLOR_FN(interchange, 0.98F, 0.55F, 0.25F)
BT_THEME_COLOR_FN(setup, 0.55F, 0.62F, 0.75F)
BT_THEME_COLOR_FN(ok, 0.36F, 0.78F, 0.44F)
BT_THEME_COLOR_FN(hairline, 0.26F, 0.28F, 0.33F)
