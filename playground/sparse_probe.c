#include <windows.h>

#include <stdint.h>
#include <stdio.h>

static const char *PATH_ = "sparse_probe.bin";

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    DeleteFileA(PATH_);
    HANDLE h =
        CreateFileA(PATH_, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        printf("create failed %lu\n", GetLastError());
        return 1;
    }
    DWORD bytes = 0;
    BOOL sparse = DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytes, NULL);
    printf("set_sparse=%d err=%lu\n", sparse, GetLastError());
    LARGE_INTEGER size;
    size.QuadPart = 2147483649LL;
    BOOL moved = SetFilePointerEx(h, size, NULL, FILE_BEGIN);
    printf("moved=%d err=%lu\n", moved, GetLastError());
    BOOL extended = SetEndOfFile(h);
    printf("extended=%d err=%lu\n", extended, GetLastError());
    CloseHandle(h);

    FILE *file = fopen(PATH_, "rb");
    printf("fopen=%p\n", (void *)file);
    if (file == NULL) {
        return 1;
    }
    int seek_rc = fseek(file, 0, SEEK_END);
    long pos = ftell(file);
    printf("fseek_rc=%d ftell=%ld errno=%d\n", seek_rc, pos, errno);
    fclose(file);
    DeleteFileA(PATH_);
    return 0;
}
