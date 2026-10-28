#include "raylib.h"

#include <stdio.h>

__declspec(dllimport) int __stdcall PostMessageW(void *hwnd, unsigned int msg, unsigned int wparam,
                                                 unsigned long lparam);
__declspec(dllimport) unsigned long __stdcall MapVirtualKeyW(unsigned int vk, unsigned int map);

static int post_key(void *hwnd, unsigned int msg, unsigned int wparam, unsigned int vk, int up) {
    unsigned long scan = MapVirtualKeyW(vk, 0);
    unsigned long lparam = 1UL | (scan << 16U);
    if (up) {
        lparam |= 1UL << 30U;
        lparam |= 1UL << 31U;
    }
    return PostMessageW(hwnd, msg, wparam, lparam) != 0;
}

int main(void) {
    InitWindow(640, 480, "l49 input probe");
    SetTargetFPS(60);
    SetTraceLogLevel(LOG_WARNING);
    void *hwnd = GetWindowHandle();
    int saw_slash = 0;
    int saw_char = 0;
    int slash_frame = -1;
    int char_frame = -1;
    for (int frame = 0; frame < 240; frame++) {
        if (frame == 10) {
            post_key(hwnd, 0x0100, 0xBF, 0xBF, 0);
        }
        if (frame == 11) {
            post_key(hwnd, 0x0101, 0xBF, 0xBF, 1);
        }
        if (frame == 40) {
            post_key(hwnd, 0x0100, 'B', 'B', 0);
            post_key(hwnd, 0x0101, 'B', 'B', 1);
        }
        if (frame == 80) {
            post_key(hwnd, 0x0102, 'b', 'B', 0);
        }
        if (frame == 120) {
            post_key(hwnd, 0x0100, 'B', 'B', 0);
            post_key(hwnd, 0x0102, 'b', 'B', 0);
            post_key(hwnd, 0x0101, 'B', 'B', 1);
        }
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
        if (IsKeyPressed(KEY_SLASH) && !saw_slash) {
            saw_slash = 1;
            slash_frame = frame;
        }
        int ch = GetCharPressed();
        while (ch != 0) {
            printf("frame %d char 0x%02x\n", frame, ch);
            if (ch == 'b' && !saw_char) {
                saw_char = 1;
                char_frame = frame;
            }
            ch = GetCharPressed();
        }
    }
    printf("slash=%d frame=%d char=%d frame=%d\n", saw_slash, slash_frame, saw_char, char_frame);
    CloseWindow();
    return 0;
}
