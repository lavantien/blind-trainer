#include "app/driver_keys.h"

#include "common/bt_trace.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static int extended_vk(unsigned int vk) {
    switch (vk) {
    case VK_RIGHT:
    case VK_LEFT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_RWIN:
    case VK_LWIN:
    case VK_APPS:
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_SNAPSHOT:
    case VK_CANCEL:
        return 1;
    default:
        return 0;
    }
}

static int post_key(void *hwnd, unsigned int msg, unsigned int wparam, unsigned int vk, int up) {
    unsigned long scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    unsigned long lparam = 1UL | (scan << 16U);
    if (extended_vk(vk)) {
        lparam |= (unsigned long)KF_EXTENDED << 16U;
    }
    if (up) {
        lparam |= 1U << 30U;
        lparam |= 1U << 31U;
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
    unsigned int vk = (unsigned int)VkKeyScanW((wchar_t)ch) & 0xFFU;
    int ok = post_key(hwnd, WM_CHAR, ch, vk, 0);
    BT_TRACE_EXIT("app.driver_key_char");
    return ok;
}
