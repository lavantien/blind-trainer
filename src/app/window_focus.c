#include "app/window_focus.h"

#include "common/alloc.h"
#include "common/bt_trace.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdlib.h>
#include <string.h>

static HWND g_marked_window;
static HWND g_sentinel_window;
static HWND g_sentinel_baseline;

int bt_app_focus_no_activate_install(void *hwnd) {
    BT_TRACE_ENTER("app.focus_no_activate_install");
    if (hwnd == NULL) {
        BT_TRACE_FAIL("app.focus_no_activate_install", "no window handle");
        BT_TRACE_EXIT("app.focus_no_activate_install");
        return 0;
    }
    HWND window = (HWND)hwnd;
    ULONG_PTR style = (ULONG_PTR)GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetWindowLongPtrW(window, GWL_EXSTYLE, (LONG_PTR)(style | (ULONG_PTR)WS_EX_NOACTIVATE));
    g_marked_window = window;
    BT_TRACE_EXIT("app.focus_no_activate_install");
    return 1;
}

int bt_app_focus_no_activate_restore(void) {
    BT_TRACE_ENTER("app.focus_no_activate_restore");
    if (g_marked_window == NULL) {
        BT_TRACE_EXIT("app.focus_no_activate_restore");
        return 0;
    }
    ULONG_PTR style = (ULONG_PTR)GetWindowLongPtrW(g_marked_window, GWL_EXSTYLE);
    SetWindowLongPtrW(g_marked_window, GWL_EXSTYLE,
                      (LONG_PTR)(style & ~(ULONG_PTR)WS_EX_NOACTIVATE));
    g_marked_window = NULL;
    BT_TRACE_EXIT("app.focus_no_activate_restore");
    return 1;
}

int bt_app_focus_style_no_activate(const void *hwnd) {
    BT_TRACE_ENTER("app.focus_style_no_activate");
    int marked = 0;
    if (hwnd != NULL) {
        ULONG_PTR style = (ULONG_PTR)GetWindowLongPtrW((HWND)hwnd, GWL_EXSTYLE);
        marked = (style & (ULONG_PTR)WS_EX_NOACTIVATE) != 0 ? 1 : 0;
    }
    BT_TRACE_EXIT("app.focus_style_no_activate");
    return marked;
}

void *bt_app_focus_foreground_window(void) {
    BT_TRACE_ENTER("app.focus_foreground_window");
    void *foreground = GetForegroundWindow();
    BT_TRACE_EXIT("app.focus_foreground_window");
    return foreground;
}

static int focus_env_fail(const char *name) {
    const char *forced = getenv(name);
    return forced != NULL && forced[0] != '\0' ? 1 : 0;
}

int bt_app_focus_cursor_pos(long *x, long *y) {
    BT_TRACE_ENTER("app.focus_cursor_pos");
    if (focus_env_fail("BT_APP_FOCUS_CURSOR_FAIL")) {
        BT_TRACE_FAIL("app.focus_cursor_pos", "forced by env");
        BT_TRACE_EXIT("app.focus_cursor_pos");
        return 0;
    }
    POINT point;
    int ok = GetCursorPos(&point) != 0;
    if (ok && x != NULL && y != NULL) {
        *x = (long)point.x;
        *y = (long)point.y;
    } else if (!ok) {
        BT_TRACE_FAIL("app.focus_cursor_pos", "GetCursorPos failed");
    }
    BT_TRACE_EXIT("app.focus_cursor_pos");
    return ok;
}

int bt_app_focus_client_to_screen(const void *hwnd, long x, long y, long *screen_x,
                                  long *screen_y) {
    BT_TRACE_ENTER("app.focus_client_to_screen");
    int ok = 0;
    if (hwnd != NULL && screen_x != NULL && screen_y != NULL) {
        POINT point;
        point.x = (LONG)x;
        point.y = (LONG)y;
        if (ClientToScreen((HWND)hwnd, &point) != 0) {
            *screen_x = (long)point.x;
            *screen_y = (long)point.y;
            ok = 1;
        } else {
            BT_TRACE_FAIL("app.focus_client_to_screen", "ClientToScreen failed");
        }
    } else {
        BT_TRACE_FAIL("app.focus_client_to_screen", "null window or output");
    }
    BT_TRACE_EXIT("app.focus_client_to_screen");
    return ok;
}

enum {
    BT_FOCUS_SENTINEL_ATTEMPTS = 3,
    BT_FOCUS_SENTINEL_RETRY_MS = 30,
    BT_FOCUS_SENTINEL_OFFSCREEN_POS = -2000,
};

static int sentinel_class_ready(void) {
    static int ready = 0;
    if (focus_env_fail("BT_APP_FOCUS_CLASS_FAIL")) {
        BT_TRACE_FAIL("app.focus_sentinel_acquire", "class registration forced off");
        return 0;
    }
    if (ready) {
        return 1;
    }
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = L"bt_app_focus_sentinel";
    ATOM atom = RegisterClassW(&clazz);
    ready = atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    if (!ready) {
        BT_TRACE_FAIL("app.focus_sentinel_acquire", "RegisterClassW failed");
    }
    return ready;
}

static int sentinel_foreground_unseizable(void) {
    BT_TRACE_ENTER("app.focus_sentinel_unseizable");
    HWND probe =
        CreateWindowExW(0, L"bt_app_focus_sentinel", L"blind trainer foreground probe", WS_POPUP,
                        BT_FOCUS_SENTINEL_OFFSCREEN_POS, BT_FOCUS_SENTINEL_OFFSCREEN_POS, 8, 8,
                        NULL, NULL, GetModuleHandleW(NULL), NULL);
    int seizable = 1;
    if (probe == NULL) {
        BT_TRACE_FAIL("app.focus_sentinel_unseizable", "probe window was not created");
        BT_TRACE_EXIT("app.focus_sentinel_unseizable");
        return 0;
    }
    ShowWindow(probe, SW_SHOW);
    HWND baseline = GetForegroundWindow();
    DWORD owner_thread = GetWindowThreadProcessId(baseline, NULL);
    DWORD self_thread = GetCurrentThreadId();
    int attached = owner_thread != 0 && owner_thread != self_thread &&
                   AttachThreadInput(self_thread, owner_thread, TRUE) != 0;
    if (SetForegroundWindow(probe) == 0 || GetForegroundWindow() != probe) {
        seizable = 0;
    }
    if (attached) {
        AttachThreadInput(self_thread, owner_thread, FALSE);
    }
    SetForegroundWindow(baseline);
    DestroyWindow(probe);
    BT_TRACE_EXIT("app.focus_sentinel_unseizable");
    return !seizable;
}

static int sentinel_force_foreground(void) {
    for (int attempt = 0; attempt < BT_FOCUS_SENTINEL_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            Sleep(BT_FOCUS_SENTINEL_RETRY_MS);
        }
        ShowWindow(g_sentinel_window, SW_SHOW);
        HWND baseline = GetForegroundWindow();
        if (baseline == g_sentinel_window) {
            return 1;
        }
        DWORD owner_thread = GetWindowThreadProcessId(baseline, NULL);
        DWORD self_thread = GetCurrentThreadId();
        int attached = owner_thread != 0 && owner_thread != self_thread &&
                       AttachThreadInput(self_thread, owner_thread, TRUE) != 0;
        int forced = SetForegroundWindow(g_sentinel_window) != 0;
        if (attached) {
            AttachThreadInput(self_thread, owner_thread, FALSE);
        }
        if (forced && GetForegroundWindow() == g_sentinel_window) {
            return 1;
        }
    }
    return sentinel_foreground_unseizable();
}

static void give_foreground_back(HWND target) {
    if (target == NULL || IsWindow(target) == FALSE) {
        return;
    }
    DWORD owner_thread = GetWindowThreadProcessId(target, NULL);
    DWORD self_thread = GetCurrentThreadId();
    int attached = owner_thread != 0 && owner_thread != self_thread &&
                   AttachThreadInput(self_thread, owner_thread, TRUE) != 0;
    SetForegroundWindow(target);
    if (attached) {
        AttachThreadInput(self_thread, owner_thread, FALSE);
    }
}

int bt_app_focus_sentinel_acquire(void) {
    BT_TRACE_ENTER("app.focus_sentinel_acquire");
    if (!sentinel_class_ready()) {
        BT_TRACE_EXIT("app.focus_sentinel_acquire");
        return 0;
    }
    if (g_sentinel_window != NULL) {
        bt_app_focus_sentinel_release();
    }
    g_sentinel_window =
        CreateWindowExW(WS_EX_TOOLWINDOW, L"bt_app_focus_sentinel", L"blind trainer focus sentinel",
                        WS_POPUP, BT_FOCUS_SENTINEL_OFFSCREEN_POS, BT_FOCUS_SENTINEL_OFFSCREEN_POS,
                        8, 8, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (g_sentinel_window == NULL) {
        BT_TRACE_FAIL("app.focus_sentinel_acquire", "CreateWindowExW failed");
        BT_TRACE_EXIT("app.focus_sentinel_acquire");
        return 0;
    }
    g_sentinel_baseline = GetForegroundWindow();
    int held = sentinel_force_foreground();
    if (!held) {
        BT_TRACE_FAIL("app.focus_sentinel_acquire", "sentinel did not reach the foreground");
        DestroyWindow(g_sentinel_window);
        g_sentinel_window = NULL;
        g_sentinel_baseline = NULL;
    }
    BT_TRACE_EXIT("app.focus_sentinel_acquire");
    return held;
}

void bt_app_focus_sentinel_release(void) {
    BT_TRACE_ENTER("app.focus_sentinel_release");
    if (g_sentinel_window != NULL) {
        if (GetForegroundWindow() == g_sentinel_window || focus_env_fail("BT_APP_FOCUS_GIVEBACK")) {
            give_foreground_back(g_sentinel_baseline);
        }
        DestroyWindow(g_sentinel_window);
        g_sentinel_window = NULL;
    }
    g_sentinel_baseline = NULL;
    BT_TRACE_EXIT("app.focus_sentinel_release");
}

enum {
    BT_FOCUS_WATCH_SAMPLE_MS = 40,
    BT_FOCUS_WATCH_TITLE_MAX = 64,
    BT_FOCUS_WATCH_CURSOR_JUMP_PX = 300,
    BT_FOCUS_TITLE_TIMEOUT_MS = 25,
};

typedef struct {
    volatile long stop;
    volatile long app_foreground_count;
    volatile long cursor_jump_count;
    volatile long jump_from_x;
    volatile long jump_from_y;
    volatile long jump_to_x;
    volatile long jump_to_y;
    wchar_t title[BT_FOCUS_WATCH_TITLE_MAX];
    unsigned long pid;
    int have_last;
    LONG last_x;
    LONG last_y;
    HANDLE thread;
} bt_focus_watch;

static int foreground_is_watched_window(const bt_focus_watch *watch, HWND foreground) {
    if (foreground == NULL) {
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if ((unsigned long)pid != watch->pid) {
        return 0;
    }
    if (watch->title[0] == L'\0') {
        return 1;
    }
    wchar_t title[BT_FOCUS_WATCH_TITLE_MAX];
    DWORD_PTR copied = 0;
    int sent =
        SendMessageTimeoutW(foreground, WM_GETTEXT, (WPARAM)BT_FOCUS_WATCH_TITLE_MAX, (LPARAM)title,
                            SMTO_ABORTIFHUNG, BT_FOCUS_TITLE_TIMEOUT_MS, &copied) != 0;
    return sent && copied > 0 && wcscmp(title, watch->title) == 0;
}

static int cursor_lands_on_watched_window(const bt_focus_watch *watch, LONG x, LONG y) {
    POINT point;
    point.x = x;
    point.y = y;
    HWND landed = WindowFromPoint(point);
    if (landed == NULL) {
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(landed, &pid);
    return (unsigned long)pid == watch->pid;
}

static DWORD WINAPI focus_watch_thread(LPVOID raw) {
    bt_focus_watch *watch = raw;
    while (InterlockedCompareExchange(&watch->stop, 0, 0) == 0) {
        if (foreground_is_watched_window(watch, GetForegroundWindow())) {
            InterlockedIncrement(&watch->app_foreground_count);
        }
        POINT point;
        if (GetCursorPos(&point)) {
            if (watch->have_last &&
                (labs((long)(point.x - watch->last_x)) > BT_FOCUS_WATCH_CURSOR_JUMP_PX ||
                 labs((long)(point.y - watch->last_y)) > BT_FOCUS_WATCH_CURSOR_JUMP_PX) &&
                cursor_lands_on_watched_window(watch, point.x, point.y)) {
                InterlockedIncrement(&watch->cursor_jump_count);
                InterlockedExchange(&watch->jump_from_x, (long)watch->last_x);
                InterlockedExchange(&watch->jump_from_y, (long)watch->last_y);
                InterlockedExchange(&watch->jump_to_x, (long)point.x);
                InterlockedExchange(&watch->jump_to_y, (long)point.y);
            }
            watch->last_x = point.x;
            watch->last_y = point.y;
            watch->have_last = 1;
        }
        Sleep(BT_FOCUS_WATCH_SAMPLE_MS);
    }
    return 0;
}

unsigned long bt_app_focus_current_pid(void) {
    BT_TRACE_ENTER("app.focus_current_pid");
    unsigned long pid = (unsigned long)GetCurrentProcessId();
    BT_TRACE_EXIT("app.focus_current_pid");
    return pid;
}

void *bt_app_focus_watch_start(const char *title, unsigned long pid) {
    BT_TRACE_ENTER("app.focus_watch_start");
    bt_focus_watch *watch = bt_malloc(sizeof *watch);
    if (watch == NULL) {
        BT_TRACE_FAIL("app.focus_watch_start", "allocation failed");
        BT_TRACE_EXIT("app.focus_watch_start");
        return NULL;
    }
    memset(watch, 0, sizeof *watch);
    int written = 1;
    if (title != NULL && title[0] != '\0') {
        written =
            MultiByteToWideChar(CP_UTF8, 0, title, -1, watch->title, BT_FOCUS_WATCH_TITLE_MAX);
    }
    watch->pid = pid;
    watch->thread = NULL;
    if (written <= 0) {
        BT_TRACE_FAIL("app.focus_watch_start", "title conversion failed");
        free(watch);
        BT_TRACE_EXIT("app.focus_watch_start");
        return NULL;
    }
    watch->thread = CreateThread(NULL, 0, focus_watch_thread, watch, 0, NULL);
    if (watch->thread == NULL) {
        BT_TRACE_FAIL("app.focus_watch_start", "CreateThread failed");
        free(watch);
        BT_TRACE_EXIT("app.focus_watch_start");
        return NULL;
    }
    BT_TRACE_EXIT("app.focus_watch_start");
    return watch;
}

long bt_app_focus_watch_app_foreground_count(const void *watch) {
    BT_TRACE_ENTER("app.focus_watch_app_foreground_count");
    const bt_focus_watch *focus = watch;
    long count =
        focus != NULL
            ? InterlockedCompareExchange((volatile long *)&focus->app_foreground_count, 0, 0)
            : 0;
    BT_TRACE_EXIT("app.focus_watch_app_foreground_count");
    return count;
}

long bt_app_focus_watch_cursor_jump_count(const void *watch) {
    BT_TRACE_ENTER("app.focus_watch_cursor_jump_count");
    const bt_focus_watch *focus = watch;
    long count = focus != NULL
                     ? InterlockedCompareExchange((volatile long *)&focus->cursor_jump_count, 0, 0)
                     : 0;
    BT_TRACE_EXIT("app.focus_watch_cursor_jump_count");
    return count;
}

void bt_app_focus_watch_stop(void *watch) {
    BT_TRACE_ENTER("app.focus_watch_stop");
    bt_focus_watch *focus = watch;
    if (focus != NULL) {
        InterlockedExchange(&focus->stop, 1);
        WaitForSingleObject(focus->thread, INFINITE);
        CloseHandle(focus->thread);
        free(focus);
    }
    BT_TRACE_EXIT("app.focus_watch_stop");
}
