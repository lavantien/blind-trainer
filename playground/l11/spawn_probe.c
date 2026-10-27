#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <process.h>
#include <windows.h>

int main(void) {
    const char *exe = "build/bin/visual-diff.exe";
    wchar_t wide_exe[1024];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wide_exe, 1024);
    const char *args[] = {"build/test-tmp/vd-probe/ảnh chụp", "build/test-tmp/vd-probe/nền"};
    wchar_t *wide[4];
    wide[0] = _wcsdup(wide_exe);
    wide[1] = malloc(2048);
    wide[2] = malloc(2048);
    MultiByteToWideChar(CP_UTF8, 0, args[0], -1, wide[1], 1024);
    MultiByteToWideChar(CP_UTF8, 0, args[1], -1, wide[2], 1024);
    wide[3] = NULL;
    int code = (int)_wspawnv(_P_WAIT, wide_exe, (const wchar_t *const *)wide);
    printf("probe exit=%d\n", code);
    return 0;
}
