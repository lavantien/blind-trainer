#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t **argv) {
    if (argc >= 2) {
        wprintf(L"arg1 [%ls] len %zu\n", argv[1], wcslen(argv[1]));
    }
    wprintf(L"wmain ok\n");
    return 0;
}
