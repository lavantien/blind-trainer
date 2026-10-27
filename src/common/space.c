#include "common/space.h"

#include "common/bt_trace.h"

size_t bt_space_len(const char *text, size_t at, size_t len) {
    BT_TRACE_ENTER("space.len");
    size_t space = 0;
    if (at < len) {
        const unsigned char *p = (const unsigned char *)text + at;
        size_t avail = len - at;
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == 0x0BU || *p == 0x0CU) {
            space = 1;
        } else if (avail >= 2 && p[0] == 0xC2U && (p[1] == 0x85U || p[1] == 0xA0U)) {
            space = 2;
        } else if (avail >= 3 && ((p[0] == 0xE1U && p[1] == 0x9AU && p[2] == 0x80U) ||
                                  (p[0] == 0xE2U && p[1] == 0x80U &&
                                   ((p[2] >= 0x80U && p[2] <= 0x8AU) || p[2] == 0xA8U ||
                                    p[2] == 0xA9U || p[2] == 0xAFU)) ||
                                  (p[0] == 0xE2U && p[1] == 0x81U && p[2] == 0x9FU) ||
                                  (p[0] == 0xE3U && p[1] == 0x80U && p[2] == 0x80U))) {
            space = 3;
        }
    }
    BT_TRACE_EXIT("space.len");
    return space;
}
