#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

int main(void) {
    const char *exe = "build/bin/visual-diff.exe";
    wchar_t wide_exe[1024];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wide_exe, 1024);
    const char *args[] = {"build/test-tmp/vd-probe/ảnh chụp", "build/test-tmp/vd-probe/nền"};
    wchar_t wide[4];
    wchar_t line[4096];
    size_t at = 0;
    MultiByteToWideChar(CP_UTF8, 0, args[0], -1, &wide[1], 1024);
    MultiByteToWideChar(CP_UTF8, 0, args[1], -1, &wide[2], 1024);
    const wchar_t *parts[] = {wide_exe, &wide[1], &wide[2]};
    for (int idx = 0; idx < 3; idx++) {
        line[at++] = L'"';
        for (const wchar_t *scan = parts[idx]; *scan; scan++) {
            line[at++] = *scan;
        }
        line[at++] = L'"';
        if (idx < 2) {
            line[at++] = L' ';
        }
    }
    line[at] = L'\0';
    STARTUPINFOW startup;
    PROCESS_INFORMATION info;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    ZeroMemory(&info, sizeof info);
    if (CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &info) == 0) {
        printf("parent create error=%lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD raw = 0;
    GetExitCodeProcess(info.hProcess, &raw);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    printf("parent exit=%u\n", (unsigned)raw);
    return 0;
}
