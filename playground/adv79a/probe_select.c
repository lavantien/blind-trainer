#include "app/boot.h"
#include "app/logic.h"
#include "app/ui.h"
#include "app/views/settings.h"
#include "app/window.h"
#include "data/paths.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

static void report_layout(float w, float h) {
    bt_view_settings_layout layout = bt_view_settings_layout_for(w, h);
    printf("window %.0fx%.0f\n", w, h);
    for (int i = 0; i < BT_SETTINGS_ROW_COUNT; i++) {
        printf("  row %d x %.1f y %.1f w %.1f h %.1f\n", i, layout.rows[i].x, layout.rows[i].y,
               layout.rows[i].width, layout.rows[i].height);
        printf("  ctl %d x %.1f y %.1f w %.1f h %.1f\n", i, layout.controls[i].x,
               layout.controls[i].y, layout.controls[i].width, layout.controls[i].height);
    }
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        float slot_y = layout.controls[1].y + layout.controls[1].height * (float)(i + 1);
        printf("  option %d y %.1f to %.1f%s\n", i, slot_y, slot_y + layout.controls[1].height,
               slot_y + layout.controls[1].height > h ? " PAST WINDOW" : "");
    }
}

static void frame(const bt_app_font_book *book, bt_settings_draft *draft, int *changed,
                  int *focus, bt_settings_widgets *widgets, const bt_ui_input *input) {
    bt_app_window_frame_begin();
    ClearBackground(bt_app_color(bt_theme_background()));
    bt_view_settings_draw(draft, changed, focus, widgets, book->regular, book->bold, 1280.0F,
                          800.0F, input);
    bt_app_window_frame_end();
}

int main(void) {
    report_layout(1280.0F, 800.0F);
    report_layout(640.0F, 480.0F);
    if (!bt_app_window_open(1280, 800, "adv79a select probe", NULL)) {
        printf("window open failed\n");
        return 1;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char config_path[BT_PATH_MAX];
    bt_data_config_path(config_path);
    if (bt_app_fonts_load(config_path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        printf("fonts: %s\n", error);
        bt_app_window_close();
        return 1;
    }
    bt_view_settings_layout layout = bt_view_settings_layout_for(1280.0F, 800.0F);
    bt_settings_draft draft;
    draft.memo_cadence_ms = 800;
    draft.memo_mode = BT_MEMO_SYLLABLES;
    draft.drill_reps = 6;
    bt_settings_widgets widgets;
    memset(&widgets, 0, sizeof widgets);
    int changed = 0;
    int focus = 0;
    bt_ui_input quiet;
    bt_ui_input_clear(&quiet);
    for (int i = 0; i < 3; i++) {
        frame(&book, &draft, &changed, &focus, &widgets, &quiet);
    }
    bt_ui_input open;
    bt_ui_input_clear(&open);
    open.x = layout.controls[1].x + layout.controls[1].width * 0.5F;
    open.y = layout.controls[1].y + layout.controls[1].height * 0.5F;
    open.click = 1;
    frame(&book, &draft, &changed, &focus, &widgets, &open);
    printf("after open click: select open %d cursor %d mode %d\n", widgets.mode_select.open,
           widgets.mode_select.cursor, (int)draft.memo_mode);
    bt_ui_input pick;
    bt_ui_input_clear(&pick);
    pick.x = layout.controls[1].x + layout.controls[1].width * 0.5F;
    pick.y = layout.controls[2].y + 5.0F;
    pick.click = 1;
    frame(&book, &draft, &changed, &focus, &widgets, &pick);
    printf("after pick click at y %.1f: select open %d mode %d reps focused %d cadence focused %d\n",
           pick.y, widgets.mode_select.open, (int)draft.memo_mode, widgets.reps.focused,
           widgets.cadence.focused);
    printf("overlap point inside option 0 slot: %.1f to %.1f\n",
           layout.controls[1].y + layout.controls[1].height,
           layout.controls[1].y + layout.controls[1].height * 2.0F);
    printf("reps control: %.1f to %.1f\n", layout.controls[2].y,
           layout.controls[2].y + layout.controls[2].height);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    return 0;
}
