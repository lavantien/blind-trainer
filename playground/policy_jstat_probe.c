#include "policy_test_util.h"
#include "unit/tools/spawn_util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "policy/fsutil.h"

static int make_junction_probe(const char *link, const char *target) {
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
    char prefixed[BT_PATH_MAX];
    snprintf(prefixed, sizeof prefixed, "\\??\\%s", target);
    for (char *cursor = prefixed; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    wchar_t name[BT_PATH_MAX];
    int name_len = MultiByteToWideChar(CP_UTF8, 0, prefixed, -1, name, BT_PATH_MAX);
    size_t bytes = (size_t)(name_len - 1) * sizeof(wchar_t);
    unsigned short data_len = (unsigned short)(12 + bytes);
    unsigned char buffer[8192];
    memset(buffer, 0, sizeof buffer);
    *(unsigned long *)(buffer + 0) = 0xA0000003u;
    *(unsigned short *)(buffer + 4) = data_len;
    *(unsigned short *)(buffer + 6) = 0;
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
    printf("ioctl ok=%d err=%lu bytes=%zu native=%s\n", ok, GetLastError(), bytes, prefixed);
    CloseHandle(handle);
    if (!ok) {
        RemoveDirectoryA(link);
    }
    return ok;
}

int main(void) {
    char root[POLICY_UTIL_PATH_MAX];
    policy_util_scratch("jstat-probe", root);
    printf("cwd root=%s\n", root);
    char target[POLICY_UTIL_PATH_MAX];
    policy_util_join(target, root, "real");
    policy_util_mkdir_p(target);
    char stray[POLICY_UTIL_PATH_MAX];
    policy_util_join(stray, target, "stray.cpp");
    policy_util_write(stray, "int x;\n");
    char src_dir[POLICY_UTIL_PATH_MAX];
    policy_util_join(src_dir, root, "src");
    policy_util_mkdir_p(src_dir);
    char link[POLICY_UTIL_PATH_MAX];
    policy_util_join(link, src_dir, "linked");
    if (!make_junction_probe(link, target)) {
        return 1;
    }
    printf("reparse=%d\n", policy_path_is_reparse_or_link(link));
    errno = 0;
    struct stat info;
    int rc = stat(link, &info);
    printf("stat rel rc=%d errno=%d (%s) isdir=%d\n", rc, errno, strerror(errno),
           rc == 0 && (info.st_mode & S_IFMT) == S_IFDIR);
    char absolute[BT_PATH_MAX];
    char root_abs[BT_PATH_MAX];
    GetFullPathNameA(root, sizeof root_abs, root_abs, NULL);
    snprintf(absolute, sizeof absolute, "%s\\src\\linked", root_abs);
    errno = 0;
    rc = stat(absolute, &info);
    printf("stat abs rc=%d errno=%d (%s) isdir=%d\n", rc, errno, strerror(errno),
           rc == 0 && (info.st_mode & S_IFMT) == S_IFDIR);
    printf("policy is_dir rel=%d abs=%d\n", policy_path_is_dir(link), policy_path_is_dir(absolute));
    policy_util_rm_rf(root);
    return 0;
}
