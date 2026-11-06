#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

static int make_junction(const char *link, const char *target) {
    if (!CreateDirectoryA(link, NULL)) {
        printf("mkdir link failed %lu\n", GetLastError());
        return 0;
    }
    HANDLE handle = CreateFileA(link, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        printf("open link failed %lu\n", GetLastError());
        return 0;
    }
    char native[512];
    char full[512];
    GetFullPathNameA(target, sizeof full, full, NULL);
    snprintf(native, sizeof native, "\\??\\%s", full);
    for (char *cursor = native; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    wchar_t name[512];
    int name_len = MultiByteToWideChar(CP_UTF8, 0, native, -1, name, 512);
    size_t bytes = (size_t)(name_len - 1) * sizeof(wchar_t);
    unsigned short data_len = (unsigned short)(12 + bytes);
    unsigned char buffer[2048];
    memset(buffer, 0, sizeof buffer);
    *(unsigned long *)(buffer + 0) = 0xA0000003u;
    *(unsigned short *)(buffer + 4) = data_len;
    *(unsigned short *)(buffer + 8) = 0;
    *(unsigned short *)(buffer + 10) = (unsigned short)bytes;
    *(unsigned short *)(buffer + 12) = (unsigned short)(bytes + 2);
    *(unsigned short *)(buffer + 14) = 0;
    memcpy(buffer + 16, name, bytes);
    DWORD returned = 0;
    int ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer, (DWORD)(8 + data_len), NULL,
                             0, &returned, NULL)
                 ? 1
                 : 0;
    printf("ioctl ok=%d err=%lu native=%s\n", ok, GetLastError(), native);
    CloseHandle(handle);
    if (!ok) {
        RemoveDirectoryA(link);
    }
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: junction_stat_probe <link-fwd> <target-fwd>\n");
        return 2;
    }
    if (!make_junction(argv[1], argv[2])) {
        return 1;
    }
    struct stat info;
    int rc = stat(argv[1], &info);
    printf("stat fwd rc=%d errno=%d isdir=%d\n", rc, errno,
           rc == 0 && (info.st_mode & S_IFMT) == S_IFDIR);
    char back[512];
    snprintf(back, sizeof back, "%s", argv[1]);
    for (char *cursor = back; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    rc = stat(back, &info);
    printf("stat back rc=%d errno=%d isdir=%d\n", rc, errno,
           rc == 0 && (info.st_mode & S_IFMT) == S_IFDIR);
    return 0;
}
