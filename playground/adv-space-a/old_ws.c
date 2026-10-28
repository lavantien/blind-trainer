#include <stddef.h>

size_t old_whitespace_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == 0x0BU || *p == 0x0CU) {
        return 1;
    }
    if (p[0] == 0xC2U && (p[1] == 0x85U || p[1] == 0xA0U)) {
        return 2;
    }
    if (p[0] == 0xE1U && p[1] == 0x9AU && p[2] == 0x80U) {
        return 3;
    }
    if (p[0] == 0xE2U && p[1] == 0x80U) {
        if (p[2] >= 0x80U && p[2] <= 0x8AU) {
            return 3;
        }
        if (p[2] == 0xA8U || p[2] == 0xA9U || p[2] == 0xAFU) {
            return 3;
        }
    }
    if (p[0] == 0xE2U && p[1] == 0x81U && p[2] == 0x9FU) {
        return 3;
    }
    if (p[0] == 0xE3U && p[1] == 0x80U && p[2] == 0x80U) {
        return 3;
    }
    return 0;
}
