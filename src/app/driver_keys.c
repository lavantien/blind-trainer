#include "app/driver_keys.h"

#include "common/bt_trace.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static int post_key(void *hwnd, unsigned int msg, unsigned int wparam, unsigned int vk,
                    int up) {
    unsigned long scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    unsigned long lparam = 1 | (scan << 16);
    if (up) {
        lparam |= 1u << 30;
        lparam |= 1u << 31;
    }
    return PostMessageW((HWND)hwnd, msg, wparam, lparam) != 0;
}

int bt_app_driver_key_down(void *hwnd, unsigned int vk) {
    BT_TRACE_ENTER("app.driver_key_down");
    int ok = post_key(hwnd, WM_KEYDOWN, vk, vk, 0);
    BT_TRACE_EXIT("app.driver_key_down");
    return ok;
}

int bt_app_driver_key_up(void *hwnd, unsigned int vk) {
    BT_TRACE_ENTER("app.driver_key_up");
    int ok = post_key(hwnd, WM_KEYUP, vk, vk, 1);
    BT_TRACE_EXIT("app.driver_key_up");
    return ok;
}

int bt_app_driver_key_char(void *hwnd, unsigned int ch) {
    BT_TRACE_ENTER("app.driver_key_char");
    unsigned int vk = VkKeyScanW((wchar_t)ch) & 0xFFu;
    int ok = post_key(hwnd, WM_CHAR, ch, vk, 0);
    BT_TRACE_EXIT("app.driver_key_char");
    return ok;
}
