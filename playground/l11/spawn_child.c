#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t **argv) {
    printf("child argc=%d\n", argc);
    for (int at = 0; at < argc; at++) {
        printf("child argv[%d] len=%zu <", at, wcslen(argv[at]));
        for (size_t ch = 0; argv[at][ch] != 0 && ch < 12; ch++) {
            printf("%04x ", (unsigned)argv[at][ch]);
        }
        printf(">\n");
    }
    fflush(stdout);
    return 0;
}
