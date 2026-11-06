#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TINY_PIPE = "\\\\.\\pipe\\bt-probe-tiny/report.md";
static char g_bytes[1200];

static DWORD WINAPI tiny_server(void *user) {
    (void)user;
    HANDLE pipe =
        CreateNamedPipeA(TINY_PIPE, PIPE_ACCESS_INBOUND,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 16, 0, NULL);
    printf("tiny_server pipe=%p err=%lu\n", pipe, GetLastError());
    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    printf("tiny_server connected=%d err=%lu\n", connected, GetLastError());
    Sleep(300);
    CloseHandle(pipe);
    printf("tiny_server closed\n");
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(g_bytes, 'x', sizeof g_bytes);
    HANDLE server = CreateThread(NULL, 0, tiny_server, NULL, 0, NULL);
    Sleep(200);
    FILE *out = fopen(TINY_PIPE, "wb");
    printf("client fopen=%p err=%d\n", (void *)out, errno);
    if (out == NULL) {
        return 1;
    }
    size_t wrote = fwrite(g_bytes, 1, sizeof g_bytes, out);
    int closed = fclose(out);
    printf("client asked=%zu wrote=%zu fclose=%d errno=%d\n", (size_t)sizeof g_bytes, wrote, closed,
           errno);
    WaitForSingleObject(server, 10000);
    return 0;
}
