#include "data/notation.h"

#include "common/bt_trace.h"

#include <string.h>

int bt_notation_is_rotation(const char *token) {
    BT_TRACE_ENTER("notation.is_rotation");
    if (token == NULL || token[0] == '\0' || (token[0] != 'x' && token[0] != 'y' &&
                                              token[0] != 'z')) {
        BT_TRACE_EXIT("notation.is_rotation");
        return 0;
    }
    for (const char *cursor = token + 1; *cursor != '\0'; cursor++) {
        if (*cursor != '\'' && *cursor != '2') {
            BT_TRACE_EXIT("notation.is_rotation");
            return 0;
        }
    }
    BT_TRACE_EXIT("notation.is_rotation");
    return 1;
}

int bt_notation_strip_rotations(const char *alg, char *out, size_t out_len) {
    BT_TRACE_ENTER("notation.strip_rotations");
    if (alg == NULL || out == NULL || out_len == 0) {
        BT_TRACE_FAIL("notation.strip_rotations", "invalid arguments");
        return -1;
    }
    size_t used = 0;
    int wrote_any = 0;
    const char *cursor = alg;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' &&
               *cursor != '\r') {
            cursor += 1;
        }
        size_t len = (size_t)(cursor - start);
        char token[64];
        if (len >= sizeof token) {
            BT_TRACE_FAIL("notation.strip_rotations", "token too long");
            return -1;
        }
        memcpy(token, start, len);
        token[len] = '\0';
        if (bt_notation_is_rotation(token)) {
            continue;
        }
        if (wrote_any) {
            if (used + 1 >= out_len) {
                BT_TRACE_FAIL("notation.strip_rotations", "output overflows");
                return -1;
            }
            out[used++] = ' ';
        }
        if (used + len >= out_len) {
            BT_TRACE_FAIL("notation.strip_rotations", "output overflows");
            return -1;
        }
        memcpy(out + used, token, len);
        used += len;
        wrote_any = 1;
    }
    out[used] = '\0';
    BT_TRACE_EXIT("notation.strip_rotations");
    return 0;
}
