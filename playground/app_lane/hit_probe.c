#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <stdio.h>

int main(void) {
    WNDCLASSW clazz;
    memset(&clazz, 0, sizeof clazz);
    clazz.lpfnWndProc = DefWindowProcW;
    clazz.hInstance = GetModuleHandleW(NULL);
    clazz.lpszClassName = L"bt_hit_probe";
    RegisterClassW(&clazz);
    HWND window = CreateWindowExW(WS_EX_TOPMOST, L"bt_hit_probe", L"hit probe", WS_POPUP, 200, 200,
                                  300, 300, NULL, NULL, GetModuleHandleW(NULL), NULL);
    printf("window=%p err=%lu\n", (void *)window, GetLastError());
    ShowWindow(window, SW_SHOW);
    Sleep(200);
    int far_x = GetSystemMetrics(SM_CXSCREEN) - 20;
    int far_y = GetSystemMetrics(SM_CYSCREEN) - 20;
    printf("screen=%d,%d far=%d,%d\n", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
           far_x, far_y);
    BOOL a = SetCursorPos(far_x, far_y);
    Sleep(200);
    BOOL b = SetCursorPos(350, 350);
    POINT point;
    GetCursorPos(&point);
    printf("moves=%d,%d cursor=%ld,%ld\n", a, b, (long)point.x, (long)point.y);
    POINT probe = {350, 350};
    HWND hit = WindowFromPoint(probe);
    DWORD hit_pid = 0;
    GetWindowThreadProcessId(hit, &hit_pid);
    wchar_t title[128];
    GetWindowTextW(hit, title, 128);
    printf("hit=%p pid=%lu ours=%lu title=%ls is_ours=%d\n", (void *)hit, hit_pid,
           (unsigned long)GetCurrentProcessId(), title, hit_pid == (DWORD)GetCurrentProcessId());
    DestroyWindow(window);
    return 0;
}
