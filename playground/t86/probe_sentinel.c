#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>

int main(void) {
    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    printf("OpenInputDesktop=%p err=%lu\n", desk, desk ? 0ul : GetLastError());
    if (desk) CloseDesktop(desk);
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = L"t86_probe_sentinel";
    ATOM atom = RegisterClassW(&clazz);
    printf("RegisterClassW=%d err=%lu\n", atom, atom ? 0ul : GetLastError());
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, L"t86_probe_sentinel", L"t86",
                                  WS_POPUP, -2000, -2000, 8, 8, NULL, NULL,
                                  GetModuleHandleW(NULL), NULL);
    printf("CreateWindowExW=%p err=%lu\n", window, window ? 0ul : GetLastError());
    HWND baseline = GetForegroundWindow();
    wchar_t title[128] = L"";
    GetWindowTextW(baseline, title, 128);
    printf("foreground=%p title='%ls'\n", baseline, title);
    for (int attempt = 0; attempt < 3; attempt++) {
        ShowWindow(window, SW_SHOW);
        baseline = GetForegroundWindow();
        DWORD owner_thread = GetWindowThreadProcessId(baseline, NULL);
        DWORD self_thread = GetCurrentThreadId();
        int attached = owner_thread != 0 && owner_thread != self_thread &&
                       AttachThreadInput(self_thread, owner_thread, TRUE) != 0;
        int forced = SetForegroundWindow(window) != 0;
        if (attached) {
            AttachThreadInput(self_thread, owner_thread, FALSE);
        }
        printf("attempt %d: attach=%d forced=%d now=%p err=%lu\n", attempt, attached,
               forced, GetForegroundWindow(), GetLastError());
        Sleep(30);
    }
    return 0;
}
