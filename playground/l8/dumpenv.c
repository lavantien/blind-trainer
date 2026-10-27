#include <stdio.h>
#include <wchar.h>
#include <windows.h>

int main(void) {
    wchar_t *raw = GetEnvironmentStringsW();
    int index = 0;
    for (wchar_t *entry = raw; *entry != L'\0'; entry += wcslen(entry) + 1) {
        if (index < 80) {
            printf("%03d [%ls]\n", index, entry);
        }
        index++;
    }
    printf("total %d\n", index);
    FreeEnvironmentStringsW(raw);
    return 0;
}
