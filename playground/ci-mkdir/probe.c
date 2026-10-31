#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void probe_one(const char *prefix) {
    DWORD attrs = GetFileAttributesA(prefix);
    SetLastError(1234);
    BOOL ok = CreateDirectoryA(prefix, NULL);
    DWORD err = GetLastError();
    printf("prefix=%-60s attrs=%lu create=%d err=%lu\n", prefix, attrs, (int)ok, err);
}

int main(void) {
    char cwd[MAX_PATH];
    GetCurrentDirectoryA(sizeof cwd, cwd);
    char drivec[64];
    DWORD nc = GetEnvironmentVariableA("=C:", drivec, sizeof drivec);
    char base[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, base);
    printf("cwd=%s temp=%.*s =C:=[%.*s]\n", cwd, (int)n, base, (int)nc, drivec);
    char copy[MAX_PATH];
    snprintf(copy, sizeof copy, "%sbt-rg-probe-%ld\\config", base, (long)GetCurrentProcessId());
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '\\' || *cursor == '/') {
            char saved = *cursor;
            *cursor = '\0';
            probe_one(copy);
            *cursor = saved;
        }
    }
    probe_one(copy);
    probe_one("C:\\");
    probe_one("C:");
    BOOL cleared = SetEnvironmentVariableA("=C:", NULL);
    printf("clear =C: -> %d err=%lu\n", (int)cleared, GetLastError());
    probe_one("C:");
    return 0;
}
