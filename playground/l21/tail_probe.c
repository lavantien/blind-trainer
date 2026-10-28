#include "e2e/lib.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned char text[512];
    size_t len = 0;
    for (int line = 1; line <= 50; line++) {
        len += (size_t)snprintf((char *)text + len, sizeof text - len, "line %d\n", line);
    }
    printf("len=%zu\n", len);
    printf("head=[%s]\n", text);
    char tailed[512];
    bt_e2e_tail(text, len, tailed, sizeof tailed);
    printf("tailed_len=%zu\n", strlen(tailed));
    printf("tailed=[%s]\n", tailed);
    printf("cmp=%d\n", strncmp(tailed, "line 31", 7));
    return 0;
}
