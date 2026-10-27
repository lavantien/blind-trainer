#ifndef BT_APP_THEME_H
#define BT_APP_THEME_H

#include <stddef.h>

#define BT_THEME_FACE_COUNT 6
#define BT_THEME_PALETTE_COUNT 12

typedef struct {
    float red;
    float green;
    float blue;
    float alpha;
} bt_theme_color;

typedef struct {
    float root_rem;
    float font_focal;
    float font_title;
    float font_alg;
    float font_heading;
    float font_body;
    float font_small;
    float font_micro;
    float spacing_rem;
    float panel_padding_rem;
    float header_height_rem;
    float hairline_rem;
    float drill_net_width_pct;
    float pulse_hz;
    float net_view_reserve_rem;
    float dynamic_band_rem;
    float pulse_lerp_per_s;
} bt_theme;

const bt_theme *bt_theme_table(void);
bt_theme_color bt_theme_background(void);
bt_theme_color bt_theme_panel(void);
bt_theme_color bt_theme_panel_raised(void);
bt_theme_color bt_theme_text_primary(void);
bt_theme_color bt_theme_text_muted(void);
bt_theme_color bt_theme_accent(void);
bt_theme_color bt_theme_danger(void);
bt_theme_color bt_theme_teal(void);
bt_theme_color bt_theme_interchange(void);
bt_theme_color bt_theme_setup(void);
bt_theme_color bt_theme_ok(void);
bt_theme_color bt_theme_hairline(void);
bt_theme_color bt_theme_palette_color(size_t index);
bt_theme_color bt_theme_face_color(size_t face);
const char *bt_theme_face_name(size_t face);
float bt_theme_srgb_to_linear(float value);
bt_theme_color bt_theme_to_linear(bt_theme_color color);
bt_theme_color bt_theme_mix(bt_theme_color a, bt_theme_color b, float factor);

#endif
