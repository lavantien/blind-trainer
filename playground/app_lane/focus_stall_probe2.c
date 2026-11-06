#include "app/window_focus.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdio.h>

static const wchar_t *PROBE_CLASS = L"bt_focus_stall_probe2";

static void step(const char *name) {
    printf("begin %s\n", name);
    fflush(stdout);
}

static void done(const char *name) {
    printf("done  %s\n", name);
    fflush(stdout);
}

int main(void) {
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = PROBE_CLASS;
    RegisterClassW(&clazz);
    step("spawn window");
    HWND window =
        CreateWindowExW(WS_EX_TOPMOST, PROBE_CLASS, L"blind trainer focus probe target", WS_POPUP,
                        200, 200, 300, 300, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(window, SW_SHOW);
    done("spawn window");

    step("sentinel acquire");
    int held = bt_app_focus_sentinel_acquire();
    printf("  acquire held %d\n", held);
    fflush(stdout);
    done("sentinel acquire");

    step("watch start");
    void *watch =
        bt_app_focus_watch_start("blind trainer focus probe target", bt_app_focus_current_pid());
    printf("  watch %p\n", watch);
    fflush(stdout);
    done("watch start");

    step("sleep 120");
    Sleep(120);
    printf("  fg count %ld\n", bt_app_focus_watch_app_foreground_count(watch));
    fflush(stdout);
    done("sleep 120");

    step("set foreground");
    SetForegroundWindow(window);
    done("set foreground");

    step("sleep 160");
    Sleep(160);
    printf("  fg count %ld\n", bt_app_focus_watch_app_foreground_count(watch));
    fflush(stdout);
    done("sleep 160");

    step("watch stop");
    bt_app_focus_watch_stop(watch);
    done("watch stop");

    step("foreground back");
    SetForegroundWindow(GetConsoleWindow());
    done("foreground back");

    step("sentinel release");
    bt_app_focus_sentinel_release();
    done("sentinel release");

    step("destroy");
    DestroyWindow(window);
    done("destroy");
    return 0;
}
