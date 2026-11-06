#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *READ_PIPE = "\\\\.\\pipe\\bt-probe-read/cases.json";
static const char *WRITE_PIPE = "\\\\.\\pipe\\bt-probe-write/report.json";
static char g_bytes[1048576];

static DWORD WINAPI read_server(void *user) {
    (void)user;
    HANDLE pipe =
        CreateNamedPipeA(READ_PIPE, PIPE_ACCESS_OUTBOUND,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    printf("read_server pipe=%p err=%lu\n", pipe, GetLastError());
    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    printf("read_server connected=%d err=%lu\n", connected, GetLastError());
    Sleep(2000);
    CloseHandle(pipe);
    return 0;
}

static DWORD WINAPI write_server(void *user) {
    (void)user;
    HANDLE pipe =
        CreateNamedPipeA(WRITE_PIPE, PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    printf("write_server pipe=%p err=%lu\n", pipe, GetLastError());
    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }
    BOOL connected = ConnectNamedPipe(pipe, NULL);
    printf("write_server connected=%d err=%lu\n", connected, GetLastError());
    CloseHandle(pipe);
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("stage 1 read pipe\n");
    HANDLE reader = CreateThread(NULL, 0, read_server, NULL, 0, NULL);
    Sleep(500);
    printf("stage 2 client fopen\n");
    FILE *file = fopen(READ_PIPE, "rb");
    printf("stage 3 read fopen=%p err=%d\n", (void *)file, errno);
    if (file != NULL) {
        printf("stage 4 fseek\n");
        int seek_rc = fseek(file, 0, SEEK_END);
        printf("stage 5 ftell\n");
        long size = ftell(file);
        printf("stage 6 read fseek_rc=%d ftell=%ld errno=%d\n", seek_rc, size, errno);
        fclose(file);
        printf("stage 7 read closed\n");
    }
    DWORD wait = WaitForSingleObject(reader, 10000);
    printf("stage 8 reader wait=%lu\n", wait);

    printf("stage 9 write pipe\n");
    HANDLE writer = CreateThread(NULL, 0, write_server, NULL, 0, NULL);
    Sleep(500);
    printf("stage 10 client fopen write\n");
    FILE *out = fopen(WRITE_PIPE, "wb");
    printf("stage 11 write fopen=%p err=%d\n", (void *)out, errno);
    if (out != NULL) {
        memset(g_bytes, 'x', sizeof g_bytes);
        printf("stage 12 fwrite\n");
        size_t wrote = fwrite(g_bytes, 1, sizeof g_bytes, out);
        printf("stage 13 fclose\n");
        int closed = fclose(out);
        printf("stage 14 write asked=%zu wrote=%zu fclose=%d errno=%d\n", (size_t)sizeof g_bytes,
               wrote, closed, errno);
    }
    wait = WaitForSingleObject(writer, 10000);
    printf("stage 15 writer wait=%lu\n", wait);
    return 0;
}
