#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

int main(void) {
    wprintf(L"nbsp=%d u2000=%d ascii=%d\n", iswspace(0x00A0), iswspace(0x2000), iswspace(L' '));
    return 0;
}
