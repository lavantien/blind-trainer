#include "app/window_focus.h"

#include "common.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdio.h>
#include <string.h>

static const wchar_t *FOCUS_CLASS = L"bt_app_focus_test_window";

static void ensure_class(void) {
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = FOCUS_CLASS;
    RegisterClassW(&clazz);
}

static HWND spawn_window(const wchar_t *title, int x, int y, int w, int h) {
    ensure_class();
    HWND window = CreateWindowExW(WS_EX_TOPMOST, FOCUS_CLASS, title, WS_POPUP, x, y, w, h, NULL,
                                  NULL, GetModuleHandleW(NULL), NULL);
    if (window != NULL) {
        ShowWindow(window, SW_SHOW);
    }
    return window;
}

BT_TEST(focus, null_handles_rejected_without_effect) {
    BT_CHECK_EQ(bt_app_focus_no_activate_install(NULL), 0);
    BT_CHECK_EQ(bt_app_focus_style_no_activate(NULL), 0);
    BT_CHECK_EQ(bt_app_focus_no_activate_restore(), 0);
    long screen_x = 0;
    long screen_y = 0;
    BT_CHECK_EQ(bt_app_focus_client_to_screen(NULL, 0, 0, &screen_x, &screen_y), 0);
    HWND window = spawn_window(L"null outputs", 10, 10, 60, 60);
    BT_CHECK(window != NULL);
    BT_CHECK_EQ(bt_app_focus_client_to_screen(window, 0, 0, NULL, NULL), 0);
    BT_CHECK_EQ(bt_app_focus_watch_app_foreground_count(NULL), 0);
    BT_CHECK_EQ(bt_app_focus_watch_cursor_jump_count(NULL), 0);
    bt_app_focus_watch_stop(NULL);
    DestroyWindow(window);
}

BT_TEST(focus, dead_window_client_to_screen_fails) {
    HWND window = spawn_window(L"doomed", 10, 10, 60, 60);
    BT_CHECK(window != NULL);
    DestroyWindow(window);
    long screen_x = 0;
    long screen_y = 0;
    BT_CHECK_EQ(bt_app_focus_client_to_screen(window, 0, 0, &screen_x, &screen_y), 0);
}

BT_TEST(focus, overlong_title_watch_start_fails) {
    char title[200];
    memset(title, 'a', sizeof title - 1);
    title[sizeof title - 1] = '\0';
    BT_CHECK(bt_app_focus_watch_start(title, bt_app_focus_current_pid()) == NULL);
}

BT_TEST(focus, cursor_jump_counts_only_a_landing_on_our_own_window) {
    HWND window = spawn_window(L"cursor landing zone", 200, 200, 300, 300);
    BT_CHECK(window != NULL);
    void *watch = bt_app_focus_watch_start(NULL, bt_app_focus_current_pid());
    BT_CHECK(watch != NULL);
    int far_x = GetSystemMetrics(SM_CXSCREEN) - 20;
    int far_y = GetSystemMetrics(SM_CYSCREEN) - 20;
    BT_CHECK_EQ(SetCursorPos(far_x, far_y), 1);
    Sleep(160);
    BT_CHECK_EQ(SetCursorPos(350, 350), 1);
    Sleep(160);
    POINT probe = {350, 350};
    HWND hit = WindowFromPoint(probe);
    DWORD hit_pid = 0;
    GetWindowThreadProcessId(hit, &hit_pid);
    int lands_on_ours = hit != NULL && hit_pid == (DWORD)GetCurrentProcessId();
    long jumps = bt_app_focus_watch_cursor_jump_count(watch);
    bt_app_focus_watch_stop(watch);
    SetCursorPos(far_x, far_y);
    DestroyWindow(window);
    BT_CHECK(lands_on_ours ? jumps >= 1 : jumps == 0);
}

BT_TEST(focus, foreground_watch_counts_our_titled_window) {
    HWND window = spawn_window(L"blind trainer focus probe target", 200, 200, 300, 300);
    BT_CHECK(window != NULL);
    BT_CHECK_EQ(bt_app_focus_sentinel_acquire(), 1);
    void *watch =
        bt_app_focus_watch_start("blind trainer focus probe target", bt_app_focus_current_pid());
    BT_CHECK(watch != NULL);
    Sleep(120);
    BT_CHECK_EQ(bt_app_focus_watch_app_foreground_count(watch), 0);
    SetForegroundWindow(window);
    int held = GetForegroundWindow() == window;
    Sleep(160);
    long seen = bt_app_focus_watch_app_foreground_count(watch);
    bt_app_focus_watch_stop(watch);
    SetForegroundWindow(GetConsoleWindow());
    bt_app_focus_sentinel_release();
    DestroyWindow(window);
    BT_CHECK(held ? seen >= 1 : seen == 0);
}

BT_TEST(focus, sentinel_acquire_and_release_hand_back) {
    BT_CHECK_EQ(bt_app_focus_sentinel_acquire(), 1);
    bt_app_focus_sentinel_release();
    BT_CHECK_EQ(bt_app_focus_sentinel_acquire(), 1);
    bt_app_focus_sentinel_release();
}
