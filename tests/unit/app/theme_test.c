#include "app/theme.h"

#include "common/bt_harness.h"

#include <math.h>

static bt_theme_color srgb(float r, float g, float b) {
    bt_theme_color color = {r, g, b, 1.0F};
    return color;
}

static int color_eq(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

static float linear_distance(bt_theme_color a, bt_theme_color b) {
    bt_theme_color la = bt_theme_to_linear(a);
    bt_theme_color lb = bt_theme_to_linear(b);
    float dr = la.red - lb.red;
    float dg = la.green - lb.green;
    float db = la.blue - lb.blue;
    return sqrtf(dr * dr + dg * dg + db * db);
}

BT_TEST(theme, face_colors_follow_cube_convention) {
    BT_CHECK_EQ(strcmp(bt_theme_face_name(0), "U"), 0);
    BT_CHECK_EQ(strcmp(bt_theme_face_name(1), "L"), 0);
    BT_CHECK_EQ(strcmp(bt_theme_face_name(2), "F"), 0);
    BT_CHECK_EQ(strcmp(bt_theme_face_name(3), "R"), 0);
    BT_CHECK_EQ(strcmp(bt_theme_face_name(4), "B"), 0);
    BT_CHECK_EQ(strcmp(bt_theme_face_name(5), "D"), 0);
}

BT_TEST(theme, font_scale_is_pinned) {
    const bt_theme *theme = bt_theme_table();
    BT_CHECK_EQ(theme->font_micro, 0.75F);
    BT_CHECK_EQ(theme->font_small, 0.8F);
    BT_CHECK_EQ(theme->font_body, 1.0F);
    BT_CHECK_EQ(theme->font_heading, 1.3F);
    BT_CHECK_EQ(theme->font_alg, 1.7F);
    BT_CHECK_EQ(theme->font_title, 2.2F);
    BT_CHECK_EQ(theme->font_focal, 3.4F);
}

BT_TEST(theme, layout_scale_is_pinned) {
    const bt_theme *theme = bt_theme_table();
    BT_CHECK_EQ(theme->root_rem, 20.0F);
    BT_CHECK_EQ(theme->spacing_rem, 0.5F);
    BT_CHECK_EQ(theme->panel_padding_rem, 1.0F);
    BT_CHECK_EQ(theme->header_height_rem, 3.5F);
    BT_CHECK_EQ(theme->hairline_rem, 0.05F);
    BT_CHECK_EQ(theme->drill_net_width_pct, 0.44F);
    BT_CHECK_EQ(theme->net_view_reserve_rem, 8.0F);
    BT_CHECK_EQ(theme->dynamic_band_rem, 16.5F);
}

BT_TEST(theme, mechanic_palette_is_distinct) {
    bt_theme_color colors[4] = {
        bt_theme_teal(),
        bt_theme_interchange(),
        bt_theme_setup(),
        bt_theme_ok(),
    };
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            bt_theme_color a = bt_theme_to_linear(colors[i]);
            bt_theme_color b = bt_theme_to_linear(colors[j]);
            BT_CHECK(!color_eq(a, b));
        }
    }
}

BT_TEST(theme, palette_pins_twelve_shades) {
    bt_theme_color pinned[BT_THEME_PALETTE_COUNT] = {
        srgb(0.95F, 0.95F, 0.95F), srgb(0.95F, 0.60F, 0.15F), srgb(0.20F, 0.75F, 0.35F),
        srgb(0.85F, 0.20F, 0.20F), srgb(0.20F, 0.45F, 0.90F), srgb(0.95F, 0.85F, 0.15F),
        srgb(0.15F, 0.85F, 0.90F), srgb(0.90F, 0.35F, 0.80F), srgb(0.55F, 0.30F, 0.85F),
        srgb(0.60F, 0.40F, 0.20F), srgb(0.55F, 0.58F, 0.62F), srgb(0.10F, 0.55F, 0.55F),
    };
    for (int index = 0; index < BT_THEME_PALETTE_COUNT; index++) {
        BT_CHECK(color_eq(bt_theme_palette_color((size_t)index), pinned[index]));
    }
    BT_CHECK(color_eq(bt_theme_palette_color(BT_THEME_PALETTE_COUNT), bt_theme_palette_color(0)));
}

BT_TEST(theme, palette_is_pairwise_distinct) {
    for (int left = 0; left < BT_THEME_PALETTE_COUNT; left++) {
        for (int right = left + 1; right < BT_THEME_PALETTE_COUNT; right++) {
            float distance = linear_distance(bt_theme_palette_color((size_t)left),
                                             bt_theme_palette_color((size_t)right));
            BT_CHECK(distance >= 0.2F);
        }
    }
}

BT_TEST(theme, palette_front_six_match_the_cube_faces) {
    for (int face = 0; face < BT_THEME_FACE_COUNT; face++) {
        BT_CHECK(color_eq(bt_theme_palette_color((size_t)face), bt_theme_face_color((size_t)face)));
    }
}

BT_TEST(theme, ui_colors_are_pinned) {
    BT_CHECK(color_eq(bt_theme_background(), srgb(0.09F, 0.10F, 0.12F)));
    BT_CHECK(color_eq(bt_theme_panel(), srgb(0.14F, 0.15F, 0.18F)));
    BT_CHECK(color_eq(bt_theme_panel_raised(), srgb(0.19F, 0.20F, 0.24F)));
    BT_CHECK(color_eq(bt_theme_text_primary(), srgb(0.92F, 0.93F, 0.95F)));
    BT_CHECK(color_eq(bt_theme_text_muted(), srgb(0.62F, 0.64F, 0.68F)));
    BT_CHECK(color_eq(bt_theme_accent(), srgb(0.35F, 0.66F, 0.95F)));
    BT_CHECK(color_eq(bt_theme_danger(), srgb(0.92F, 0.35F, 0.35F)));
    BT_CHECK(color_eq(bt_theme_teal(), srgb(0.30F, 0.90F, 0.85F)));
    BT_CHECK(color_eq(bt_theme_interchange(), srgb(0.98F, 0.55F, 0.25F)));
    BT_CHECK(color_eq(bt_theme_setup(), srgb(0.55F, 0.62F, 0.75F)));
    BT_CHECK(color_eq(bt_theme_ok(), srgb(0.36F, 0.78F, 0.44F)));
    BT_CHECK(color_eq(bt_theme_hairline(), srgb(0.26F, 0.28F, 0.33F)));
}

BT_TEST(theme, srgb_to_linear_follows_the_srgb_transfer_function) {
    BT_CHECK_EQ(bt_theme_srgb_to_linear(0.0F), 0.0F);
    BT_CHECK_F64_NEAR(bt_theme_srgb_to_linear(0.02F), 0.02F / 12.92, 1e-9);
    BT_CHECK_F64_NEAR(bt_theme_srgb_to_linear(0.95F), pow((0.95 + 0.055) / 1.055, 2.4), 1e-6);
    BT_CHECK_EQ(bt_theme_srgb_to_linear(1.0F), 1.0F);
}

BT_TEST(theme, mix_interpolates_in_srgb_space) {
    bt_theme_color a = srgb(0.10F, 0.20F, 0.30F);
    bt_theme_color b = srgb(0.90F, 0.40F, 0.50F);
    bt_theme_color mid = bt_theme_mix(a, b, 0.5F);
    BT_CHECK_F64_NEAR(mid.red, 0.5, 1e-6);
    BT_CHECK_F64_NEAR(mid.green, 0.3, 1e-6);
    BT_CHECK_F64_NEAR(mid.blue, 0.4, 1e-6);
    BT_CHECK_EQ(mid.alpha, 1.0F);
    bt_theme_color quarter = bt_theme_mix(a, b, 0.25F);
    BT_CHECK_F64_NEAR(quarter.red, 0.3, 1e-6);
}
