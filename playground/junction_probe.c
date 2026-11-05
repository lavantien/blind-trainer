#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>

int main(int argc, char **argv) {
    const char *target = "jprobe-target";
    const char *link = "jprobe-link";
    if (argc == 3 && strcmp(argv[1], "stat") == 0) {
        struct stat info;
        int rc = stat(argv[2], &info);
        printf("stat rc=%d errno=%d isdir=%d mode=0x%X\n", rc, errno, rc == 0,
               rc == 0 ? (unsigned)info.st_mode : 0);
        if (rc == 0) {
            printf("S_IFDIR set: %d\n", (info.st_mode & S_IFMT) == S_IFDIR);
        }
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "dump") == 0) {
        HANDLE h = CreateFileA(argv[2], 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT |
                               FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            printf("dump open failed %lu\n", GetLastError());
            return 1;
        }
        unsigned char buf[2048];
        memset(buf, 0, sizeof buf);
        DWORD returned = 0;
        if (!DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof buf, &returned,
                             NULL)) {
            printf("get failed %lu\n", GetLastError());
            CloseHandle(h);
            return 1;
        }
        printf("returned=%lu tag=0x%08lX data_len=%u sub_off=%u sub_len=%u print_off=%u print_len=%u\n",
               returned, *(unsigned long *)(buf + 0), *(unsigned short *)(buf + 4),
               *(unsigned short *)(buf + 8), *(unsigned short *)(buf + 10),
               *(unsigned short *)(buf + 12), *(unsigned short *)(buf + 14));
        wchar_t sub[256];
        size_t sub_len = (*(unsigned short *)(buf + 10)) / 2;
        for (size_t i = 0; i < sub_len && i < 255; i++) {
            sub[i] = *(wchar_t *)(buf + 16 + 2 * i);
        }
        sub[sub_len < 255 ? sub_len : 255] = 0;
        printf("sub=<%ls>\n", sub);
        CloseHandle(h);
        return 0;
    }
    CreateDirectoryA(target, NULL);
    if (!CreateDirectoryA(link, NULL)) {
        printf("mkdir link failed %lu\n", GetLastError());
        return 1;
    }
    HANDLE handle = CreateFileA(link, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        printf("open link failed %lu\n", GetLastError());
        return 1;
    }
    const char *prefixed = "\\\\?\\C:\\nonexistent-marker";
    char native[256];
    char full[256];
    GetFullPathNameA(target, sizeof full, full, NULL);
    snprintf(native, sizeof native, "\\??\\%s", full);
    wchar_t name[256];
    int name_len = MultiByteToWideChar(CP_UTF8, 0, native, -1, name, 256);
    size_t bytes = (size_t)(name_len - 1) * sizeof(wchar_t);
    unsigned short data_len = (unsigned short)(12 + bytes);
    unsigned char buffer[1024];
    memset(buffer, 0, sizeof buffer);
    *(unsigned long *)(buffer + 0) = 0xA0000003u;
    *(unsigned short *)(buffer + 4) = data_len;
    *(unsigned short *)(buffer + 8) = 0;
    *(unsigned short *)(buffer + 10) = (unsigned short)bytes;
    *(unsigned short *)(buffer + 12) = (unsigned short)(bytes + 2);
    *(unsigned short *)(buffer + 14) = 0;
    memcpy(buffer + 16, name, bytes);
    printf("native=%s bytes=%zu data_len=%u\n", native, bytes, data_len);
    DWORD returned = 0;
    int ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer, (DWORD)(8 + data_len), NULL,
                             0, &returned, NULL);
    printf("ioctl ok=%d err=%lu\n", ok, GetLastError());
    CloseHandle(handle);
    if (!ok) {
        RemoveDirectoryA(link);
    }
    return ok ? 0 : 1;
}
