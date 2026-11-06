#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdio.h>

static const wchar_t *PROBE_CLASS = L"bt_focus_stall_probe";

static void step(const char *name) {
    printf("begin %s\n", name);
    fflush(stdout);
}

static void done(const char *name) {
    printf("done  %s\n", name);
    fflush(stdout);
}

static DWORD WINAPI sample_thread(LPVOID raw) {
    volatile long *stop = raw;
    while (*stop == 0) {
        GetForegroundWindow();
        POINT point;
        GetCursorPos(&point);
        WindowFromPoint(point);
        Sleep(40);
    }
    return 0;
}

int main(void) {
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = PROBE_CLASS;
    step("register class");
    RegisterClassW(&clazz);
    done("register class");

    step("spawn target window");
    HWND window = CreateWindowExW(WS_EX_TOPMOST, PROBE_CLASS, L"focus stall probe target", WS_POPUP,
                                  200, 200, 300, 300, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(window, SW_SHOW);
    done("spawn target window");

    step("spawn offscreen sentinel window");
    HWND sentinel =
        CreateWindowExW(WS_EX_TOOLWINDOW, PROBE_CLASS, L"focus stall probe sentinel", WS_POPUP,
                        -2000, -2000, 8, 8, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(sentinel, SW_SHOW);
    done("spawn offscreen sentinel window");

    step("force sentinel foreground");
    for (int attempt = 0; attempt < 3; attempt++) {
        Sleep(30);
        HWND baseline = GetForegroundWindow();
        DWORD owner_thread = GetWindowThreadProcessId(baseline, NULL);
        DWORD self_thread = GetCurrentThreadId();
        int attached = owner_thread != 0 && owner_thread != self_thread &&
                       AttachThreadInput(self_thread, owner_thread, TRUE) != 0;
        int forced = SetForegroundWindow(sentinel) != 0;
        if (attached) {
            AttachThreadInput(self_thread, owner_thread, FALSE);
        }
        printf("  attempt %d forced %d held %d\n", attempt, forced,
               GetForegroundWindow() == sentinel);
        fflush(stdout);
        if (forced && GetForegroundWindow() == sentinel) {
            break;
        }
    }
    done("force sentinel foreground");

    step("start sample thread");
    volatile long stop = 0;
    HANDLE thread = CreateThread(NULL, 0, sample_thread, (LPVOID)&stop, 0, NULL);
    done("start sample thread");

    step("sleep 120");
    Sleep(120);
    done("sleep 120");

    step("set target foreground");
    SetForegroundWindow(window);
    done("set target foreground");

    step("sleep 160");
    Sleep(160);
    done("sleep 160");

    step("stop sample thread");
    stop = 1;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    done("stop sample thread");

    step("give foreground back");
    SetForegroundWindow(GetConsoleWindow());
    done("give foreground back");

    step("destroy windows");
    DestroyWindow(window);
    DestroyWindow(sentinel);
    done("destroy windows");
    return 0;
}
