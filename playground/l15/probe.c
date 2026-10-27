#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_RANGES 5
#define PROBE_TEXT "Thử nghiệm tiếng Việt: ấềộữễ ơ ư đ Ă Đ Ơ Ư"

static int probe_ranges[PROBE_RANGES][2] = {
    {0x20, 0x7E},
    {0xA0, 0xFF},
    {0x102, 0x103},
    {0x1A0, 0x1B0},
    {0x1EA0, 0x1EF9},
};

static int probe_fail(const char *unit, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[fail] %s ", unit);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return 1;
}

static int build_codepoints(int *out, int cap) {
    int count = 0;
    for (int r = 0; r < PROBE_RANGES; r++) {
        for (int cp = probe_ranges[r][0]; cp <= probe_ranges[r][1]; cp++) {
            int present = 0;
            for (int i = 0; i < count; i++) {
                if (out[i] == cp) {
                    present = 1;
                    break;
                }
            }
            if (!present && count < cap) {
                out[count++] = cp;
            }
        }
    }
    for (int i = 1; i < count; i++) {
        int key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j] > key) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return count;
}

int main(int argc, char **argv) {
    const char *unit = "l15-probe";
    if (argc < 6) {
        return probe_fail(unit, "usage: %s width height title font shot", argv[0]);
    }
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    const char *title = argv[3];
    const char *font_path = argv[4];
    const char *shot_path = argv[5];
    printf("[entry] %s width=%d height=%d font=%s shot=%s\n", unit, width, height,
           font_path, shot_path);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(width, height, title);
    if (!IsWindowReady()) {
        return probe_fail(unit, "window not ready");
    }
    if (GetScreenWidth() != width || GetScreenHeight() != height) {
        return probe_fail(unit, "window dims %dx%d want %dx%d", GetScreenWidth(),
                          GetScreenHeight(), width, height);
    }
    printf("[pass] window %dx%d\n", GetScreenWidth(), GetScreenHeight());
    SetTargetFPS(60);
    int codepoints[1024];
    int count = build_codepoints(codepoints, 1024);
    Font font = LoadFontEx(font_path, 64, codepoints, count);
    if (font.texture.id == 0) {
        return probe_fail(unit, "font texture missing for %s", font_path);
    }
    if (font.glyphCount != count) {
        return probe_fail(unit, "glyphCount %d want %d", font.glyphCount, count);
    }
    for (int i = 0; i < count; i++) {
        int found = 0;
        for (int g = 0; g < font.glyphCount; g++) {
            if (font.glyphs[g].value == codepoints[i]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return probe_fail(unit, "codepoint U+%04X missing", codepoints[i]);
        }
    }
    Vector2 measured = MeasureTextEx(font, PROBE_TEXT, 32.0f, 1.0f);
    if (measured.x <= 0.0f || measured.y <= 0.0f) {
        return probe_fail(unit, "measure %f x %f", measured.x, measured.y);
    }
    printf("[pass] font %d glyphs, vietnamese measure %.1f x %.1f\n", font.glyphCount,
           measured.x, measured.y);
    for (int frame = 0; frame < 12; frame++) {
        BeginDrawing();
        ClearBackground((Color){23, 26, 31, 255});
        DrawTextEx(font, PROBE_TEXT, (Vector2){40.0f, 40.0f}, 32.0f, 1.0f,
                   (Color){235, 237, 242, 255});
        DrawRectangle(40, 120, 200, 60, (Color){89, 169, 242, 255});
        DrawRectangleLines(40, 120, 200, 60, (Color){235, 237, 242, 255});
        EndDrawing();
    }
    int key_seen = IsKeyPressed(KEY_ESCAPE);
    int mouse_seen = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mouse = GetMousePosition();
    int queued = GetCharPressed();
    while (queued != 0) {
        queued = GetCharPressed();
    }
    printf("[pass] input poll keys=%d mouse=%d pos=%.0f,%.0f\n", key_seen, mouse_seen,
           mouse.x, mouse.y);
    TakeScreenshot(shot_path);
    if (!FileExists(shot_path)) {
        return probe_fail(unit, "screenshot missing at %s", shot_path);
    }
    UnloadFont(font);
    CloseWindow();
    printf("[exit] %s code=0\n", unit);
    return 0;
}
