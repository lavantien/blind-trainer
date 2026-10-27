#include <stdio.h>
#include <windows.h>

#define CREATE_UNICODE_ENVIRONMENT_PROBE 0x00000400

static int try_spawn(const char *label, void *env, DWORD flags) {
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    wchar_t line[] =
        L"\"C:\\Users\\lavantien\\dev\\github\\blind-trainer\\build\\bin\\sleeper.exe\" 0";
    if (CreateProcessW(NULL, line, NULL, NULL, FALSE, flags, env, NULL, &startup, &info) == 0) {
        printf("%s: CreateProcessW failed error %lu\n", label, GetLastError());
        return 0;
    }
    WaitForSingleObject(info.hProcess, 30000);
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    printf("%s: spawned ok, exit %lu\n", label, code);
    return 1;
}

int main(void) {
    wchar_t wide_env[] = L"PROBE=1\0";
    char ansi_env[] = "PROBE=1\0";
    int wide_flag = try_spawn("wide+flag", wide_env, CREATE_UNICODE_ENVIRONMENT_PROBE);
    int wide_plain = try_spawn("wide+noflag", wide_env, 0);
    int ansi_plain = try_spawn("ansi+noflag", ansi_env, 0);
    int inherit = try_spawn("null-env", NULL, 0);
    printf("summary wide+flag=%d wide+noflag=%d ansi+noflag=%d null=%d\n", wide_flag, wide_plain,
           ansi_plain, inherit);
    return wide_flag && !wide_plain && ansi_plain && inherit ? 0 : 2;
}
