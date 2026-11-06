#include "common/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("usage: env_probe <exe> <config> <ok-count>\n");
        return 2;
    }
    char budget[32];
    snprintf(budget, sizeof budget, "%s", argv[3]);
    _putenv_s("BT_MEMOFREQ_ALLOC_FAIL_OK", budget);
    _putenv_s("BT_CONFIG", argv[2]);
    STARTUPINFOW si = {0};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {0};
    wchar_t cmd[BT_PATH_MAX];
    size_t wide = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, cmd, BT_PATH_MAX);
    if (wide == 0) {
        printf("path to wide conversion failed %lu\n", GetLastError());
        return 1;
    }
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("create failed %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    printf("child exit %lu\n", code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
