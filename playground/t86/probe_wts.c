#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>

int main(void) {
    DWORD session = WTSGetActiveConsoleSessionId();
    printf("console session=%lu\n", session);
    LPTSTR buffer = NULL;
    DWORD bytes = 0;
    if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, session, WTSSessionInfoEx,
                                   &buffer, &bytes) && buffer != NULL) {
        PWTSINFOEXW info = (PWTSINFOEXW)buffer;
        printf("level=%u sessionflags=%lu\n", info->Level,
               info->Data.WTSInfoExLevel1.SessionFlags);
        WTSFreeMemory(buffer);
    } else {
        printf("WTSQuerySessionInformation failed err=%lu\n", GetLastError());
    }
    return 0;
}
