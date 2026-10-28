#include "e2e/lib.h"

#include <stdio.h>
#include <string.h>

static void fill(unsigned char *text, size_t cap, int lines, size_t *len_out) {
    size_t len = 0;
    for (int line = 1; line <= lines; line++) {
        len += (size_t)snprintf((char *)text + len, cap - len, "line %d\n", line);
    }
    *len_out = len;
}

int main(void) {
    unsigned char text[16384];
    char tailed[2048];
    size_t len;

    fill(text, sizeof text, 50, &len);
    bt_e2e_tail(text, len, tailed, sizeof tailed);
    printf("case50 first7=[%.7s] last7=[%s] len=%zu\n", tailed,
           tailed + strlen(tailed) - 7, strlen(tailed));

    fill(text, sizeof text, 600, &len);
    bt_e2e_tail(text, len, tailed, sizeof tailed);
    printf("case600 first=[%.16s] last7=[%s] len=%zu\n", tailed,
           tailed + strlen(tailed) - 7, strlen(tailed));

    const char *crlf = "alpha\r\nbeta\r\ngamma\r\n";
    bt_e2e_tail((const unsigned char *)crlf, strlen(crlf), tailed, sizeof tailed);
    printf("casecrlf=[%s]\n", tailed);

    const char *nonl = "one\ntwo\nthree no newline";
    bt_e2e_tail((const unsigned char *)nonl, strlen(nonl), tailed, sizeof tailed);
    printf("casenonl=[%s]\n", tailed);

    const char *blanks = "a\n\n\nb\n";
    bt_e2e_tail((const unsigned char *)blanks, strlen(blanks), tailed, sizeof tailed);
    printf("caseblanks=[%s]\n", tailed);
    return 0;
}
