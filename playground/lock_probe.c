#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>

int main(void) {
    const char *path = "lock-probe-target.txt";
    FILE *seed = fopen(path, "wb");
    fputs("int x;\n", seed);
    fclose(seed);
    HANDLE lock = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (lock == INVALID_HANDLE_VALUE) {
        printf("lock open failed %lu\n", GetLastError());
        return 1;
    }
    struct stat info;
    int stat_rc = stat(path, &info);
    FILE *reader = fopen(path, "rb");
    printf("stat rc=%d isreg=%d fopen=%s\n", stat_rc,
           stat_rc == 0 ? (int)((info.st_mode & S_IFMT) == S_IFREG) : -1,
           reader != NULL ? "open" : "blocked");
    if (reader != NULL) {
        fclose(reader);
    }
    CloseHandle(lock);
    remove(path);
    return 0;
}
