#include "app/window.h"

#include "app/config.h"
#include "app/fonts.h"
#include "common/bt_trace.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

int bt_app_window_open(unsigned int width, unsigned int height, const char *title,
                       const bt_app_render_cfg *render) {
    BT_TRACE_ENTER("app.window_open");
    SetTraceLogLevel(LOG_WARNING);
    if (render != NULL && render->vsync) {
        SetConfigFlags(FLAG_VSYNC_HINT);
    }
    InitWindow((int)width, (int)height, title);
    if (render != NULL && render->target_fps > 0) {
        SetTargetFPS((int)render->target_fps);
    }
    int ready = IsWindowReady() ? 1 : 0;
    if (!ready) {
        BT_TRACE_FAIL("app.window_open", "window %ux%u not ready", width, height);
    }
    BT_TRACE_EXIT("app.window_open");
    return ready;
}

void bt_app_window_close(void) {
    BT_TRACE_ENTER("app.window_close");
    if (IsWindowReady()) {
        CloseWindow();
    }
    BT_TRACE_EXIT("app.window_close");
}

int bt_app_window_ready(void) {
    return IsWindowReady() ? 1 : 0;
}

void bt_app_window_frame_begin(void) {
    BeginDrawing();
}

void bt_app_window_frame_end(void) {
    EndDrawing();
}

int bt_app_window_capture(const char *path) {
    BT_TRACE_ENTER("app.window_capture");
    TakeScreenshot(path);
    int exists = FileExists(path) ? 1 : 0;
    if (!exists) {
        BT_TRACE_FAIL("app.window_capture", "no shot at %s", path);
    }
    BT_TRACE_EXIT("app.window_capture");
    return exists;
}

void bt_app_poll_keys(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.poll_keys");
    int key = GetKeyPressed();
    while (key != 0) {
        bt_app_key_event event = {BT_APP_KEY_OTHER, 0, 1};
        if (key == KEY_ESCAPE) {
            event.kind = BT_APP_KEY_ESCAPE;
        } else if (key >= KEY_ONE && key <= KEY_SIX) {
            event.kind = BT_APP_KEY_DIGIT;
            event.digit = (char)key;
        }
        bt_app_push_key(ctx, event);
        key = GetKeyPressed();
    }
    BT_TRACE_EXIT("app.poll_keys");
}

void bt_app_poll_input(bt_ui_input *input) {
    BT_TRACE_ENTER("app.poll_input");
    Vector2 mouse = GetMousePosition();
    input->x = mouse.x;
    input->y = mouse.y;
    input->click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? 1 : 0;
    input->down = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0;
    float wheel = GetMouseWheelMove();
    input->wheel = 0;
    if (wheel > 0.0F) {
        input->wheel = 1;
    } else if (wheel < 0.0F) {
        input->wheel = -1;
    }
    input->escape = IsKeyPressed(KEY_ESCAPE) ? 1 : 0;
    input->enter = IsKeyPressed(KEY_ENTER) ? 1 : 0;
    input->backspace = IsKeyPressed(KEY_BACKSPACE) ? 1 : 0;
    input->char_count = 0;
    int code = GetCharPressed();
    while (code != 0 && input->char_count < BT_UI_INPUT_CHARS) {
        input->chars[input->char_count++] = code;
        code = GetCharPressed();
    }
    BT_TRACE_EXIT("app.poll_input");
}

Color bt_app_color(bt_theme_color color) {
    Color out;
    out.r = (unsigned char)(color.red * 255.0F + 0.5F);
    out.g = (unsigned char)(color.green * 255.0F + 0.5F);
    out.b = (unsigned char)(color.blue * 255.0F + 0.5F);
    out.a = (unsigned char)(color.alpha * 255.0F + 0.5F);
    return out;
}

void bt_app_draw_text(const void *font, const char *text, float x, float y, float size,
                      bt_theme_color color) {
    Font cast = *(const Font *)font;
    DrawTextEx(cast, text, (Vector2){x, y}, size, 1.0F, bt_app_color(color));
}

static int load_face(const char *config_path, const char *key, const char *file_name,
                     const bt_corpus *corpus, const char *const *words, size_t word_count,
                     int codepoints[], int count, Font *out, char *error, size_t error_len) {
    bt_json_value *config = NULL;
    char load_error[BT_APP_ERROR_MAX];
    char name[128];
    snprintf(name, sizeof name, "%s", file_name);
    if (bt_app_load_config_value(config_path, &config, load_error, sizeof load_error) == 0) {
        const bt_json_value *fonts = bt_json_get(bt_json_get(config, "app"), "fonts");
        if (fonts != NULL && fonts->type == BT_JSON_OBJECT) {
            const bt_json_value *file = bt_json_get(fonts, key);
            if (file != NULL && file->type == BT_JSON_STRING) {
                snprintf(name, sizeof name, "%s", file->as.string);
            }
        }
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char fonts_dir[BT_PATH_MAX];
    snprintf(fonts_dir, sizeof fonts_dir, "%s/assets/fonts", root);
    if (config != NULL) {
        const bt_json_value *paths_fonts = bt_json_get(bt_json_get(config, "paths"), "fonts");
        if (paths_fonts != NULL && paths_fonts->type == BT_JSON_STRING) {
            snprintf(fonts_dir, sizeof fonts_dir, "%s/%s", root, paths_fonts->as.string);
        }
    }
    int built = bt_app_fonts_codepoints(config, corpus, words, word_count, codepoints, count);
    bt_json_free(config);
    if (built < 0) {
        snprintf(error, error_len, "codepoint set exceeds %d", count);
        BT_TRACE_FAIL("app.fonts_load", "%s", error);
        return -1;
    }
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", fonts_dir, name);
    *out = LoadFontEx(path, 64, codepoints, built);
    if (out->texture.id == 0) {
        snprintf(error, error_len, "font load failed at %s", path);
        BT_TRACE_FAIL("app.fonts_load", "%s", error);
        return -1;
    }
    if (out->glyphCount != built) {
        snprintf(error, error_len, "font %s holds %d glyphs, want %d", path, out->glyphCount,
                 built);
        UnloadFont(*out);
        BT_TRACE_FAIL("app.fonts_load", "%s", error);
        return -1;
    }
    for (int i = 0; i < built; i++) {
        int found = 0;
        for (int g = 0; g < out->glyphCount; g++) {
            if (out->glyphs[g].value == codepoints[i]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(error, error_len, "font %s misses U+%04X", path, codepoints[i]);
            UnloadFont(*out);
            BT_TRACE_FAIL("app.fonts_load", "%s", error);
            return -1;
        }
    }
    return 0;
}

static bt_app_font_book current_book;

int bt_app_fonts_load(const char *config_path, const bt_corpus *corpus, const char *const *words,
                      size_t word_count, bt_app_font_book *out, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.fonts_load");
    static int codepoints[BT_APP_FONTS_MAX];
    Font regular;
    if (load_face(config_path, "regular", "NotoSans-Regular.ttf", corpus, words, word_count,
                  codepoints, BT_APP_FONTS_MAX, &regular, error, error_len) != 0) {
        BT_TRACE_EXIT("app.fonts_load");
        return -1;
    }
    Font bold;
    if (load_face(config_path, "bold", "NotoSans-Bold.ttf", corpus, words, word_count, codepoints,
                  BT_APP_FONTS_MAX, &bold, error, error_len) != 0) {
        UnloadFont(regular);
        BT_TRACE_EXIT("app.fonts_load");
        return -1;
    }
    static Font held_regular;
    static Font held_bold;
    held_regular = regular;
    held_bold = bold;
    out->regular = &held_regular;
    out->bold = &held_bold;
    out->glyph_count = regular.glyphCount;
    current_book = *out;
    BT_TRACE_EXIT("app.fonts_load");
    return 0;
}

void bt_app_fonts_unload(bt_app_font_book *book) {
    BT_TRACE_ENTER("app.fonts_unload");
    if (book->regular != NULL) {
        UnloadFont(*(Font *)book->regular);
    }
    if (book->bold != NULL) {
        UnloadFont(*(Font *)book->bold);
    }
    book->regular = NULL;
    book->bold = NULL;
    current_book.regular = NULL;
    current_book.bold = NULL;
    current_book.glyph_count = 0;
    BT_TRACE_EXIT("app.fonts_unload");
}

const bt_app_font_book *bt_app_fonts_current(void) {
    BT_TRACE_ENTER("app.fonts_current");
    const bt_app_font_book *book = current_book.regular != NULL ? &current_book : NULL;
    BT_TRACE_EXIT("app.fonts_current");
    return book;
}
