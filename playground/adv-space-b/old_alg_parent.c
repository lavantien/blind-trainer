#include "core/alg.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "core/cube.h"

void bt_alg_init(bt_alg *alg) {
    alg->items = NULL;
    alg->len = 0;
    alg->cap = 0;
}

void bt_alg_free(bt_alg *alg) {
    free(alg->items);
    alg->items = NULL;
    alg->len = 0;
    alg->cap = 0;
}

bool bt_alg_push(bt_alg *alg, bt_move mv) {
    if (alg->len == alg->cap) {
        size_t cap = alg->cap == 0 ? 16 : alg->cap * 2U;
        bt_move *items = realloc(alg->items, cap * sizeof *items);
        if (items == NULL) {
            return false;
        }
        alg->items = items;
        alg->cap = cap;
    }
    alg->items[alg->len++] = mv;
    return true;
}

size_t bt_alg_len(const bt_alg *alg) {
    return alg->len;
}

bool bt_alg_is_empty(const bt_alg *alg) {
    return alg->len == 0;
}

bool bt_alg_eq(const bt_alg *a, const bt_alg *b) {
    BT_TRACE_ENTER("bt_alg_eq");
    bool equal = a->len == b->len;
    for (size_t i = 0; equal && i < a->len; i++) {
        equal = bt_move_eq(a->items[i], b->items[i]);
    }
    BT_TRACE_EXIT("bt_alg_eq");
    return equal;
}

static int face_index_of(char ch) {
    switch (ch) {
    case 'U':
        return BT_FACE_U;
    case 'L':
        return BT_FACE_L;
    case 'F':
        return BT_FACE_F;
    case 'R':
        return BT_FACE_R;
    case 'B':
        return BT_FACE_B;
    case 'D':
        return BT_FACE_D;
    default:
        return -1;
    }
}

static char face_char(uint8_t face) {
    static const char letters[6] = {'U', 'L', 'F', 'R', 'B', 'D'};
    if (face >= 6) {
        BT_TRACE_FAIL("face_char", "face %u out of range", (unsigned)face);
        bt_trace_dump();
        abort();
    }
    return letters[face];
}

static char slice_char(uint8_t slice) {
    static const char letters[3] = {'M', 'E', 'S'};
    if (slice > 2) {
        BT_TRACE_FAIL("slice_char", "slice %u out of range", (unsigned)slice);
        bt_trace_dump();
        abort();
    }
    return letters[slice];
}

static char axis_char(uint8_t axis) {
    static const char letters[3] = {'x', 'y', 'z'};
    if (axis > 2) {
        BT_TRACE_FAIL("axis_char", "axis %u out of range", (unsigned)axis);
        bt_trace_dump();
        abort();
    }
    return letters[axis];
}

static char lower_char(char ch) {
    return (char)tolower((unsigned char)ch);
}

static char upper_char(char ch) {
    return (char)toupper((unsigned char)ch);
}

static bool parse_quarter_suffix(const char *token, size_t len, size_t idx, uint8_t *out) {
    size_t rest = len - idx;
    if (rest == 0) {
        *out = 1;
        return true;
    }
    if (rest == 1 && token[idx] == '2') {
        *out = 2;
        return true;
    }
    if (rest == 1 && token[idx] == '\'') {
        *out = 3;
        return true;
    }
    if (rest == 2 && token[idx] == '2' && token[idx + 1] == '\'') {
        *out = 2;
        return true;
    }
    return false;
}

bool bt_alg_parse_token(const char *token, bt_move *out) {
    BT_TRACE_ENTER("bt_alg_parse_token");
    size_t len = strlen(token);
    size_t idx = 0;
    bool inner = false;
    if (len > 0 && token[0] == '2') {
        idx = 1;
        inner = true;
    }
    if (idx >= len) {
        BT_TRACE_EXIT("bt_alg_parse_token");
        return false;
    }
    char base = token[idx];
    idx += 1;
    uint8_t quarter;
    if (!parse_quarter_suffix(token, len, idx, &quarter)) {
        BT_TRACE_EXIT("bt_alg_parse_token");
        return false;
    }
    int face = face_index_of(base);
    if (face >= 0) {
        if (inner) {
            *out = bt_move_of_inner((uint8_t)face, quarter);
        } else {
            *out = bt_move_of_face((uint8_t)face, quarter);
        }
        BT_TRACE_EXIT("bt_alg_parse_token");
        return true;
    }
    int lower_face = face_index_of(upper_char(base));
    if (lower_face >= 0) {
        if (inner) {
            BT_TRACE_EXIT("bt_alg_parse_token");
            return false;
        }
        *out = bt_move_of_wide((uint8_t)lower_face, quarter);
        BT_TRACE_EXIT("bt_alg_parse_token");
        return true;
    }
    if (base == 'M' || base == 'E' || base == 'S') {
        if (inner) {
            BT_TRACE_EXIT("bt_alg_parse_token");
            return false;
        }
        uint8_t slice;
        switch (base) {
        case 'M':
            slice = BT_SLICE_M;
            break;
        case 'E':
            slice = BT_SLICE_E;
            break;
        default:
            slice = BT_SLICE_S;
            break;
        }
        *out = bt_move_of_slice(slice, quarter);
        BT_TRACE_EXIT("bt_alg_parse_token");
        return true;
    }
    if (base == 'x' || base == 'y' || base == 'z') {
        if (inner) {
            BT_TRACE_EXIT("bt_alg_parse_token");
            return false;
        }
        uint8_t axis;
        switch (base) {
        case 'x':
            axis = BT_AXIS_X;
            break;
        case 'y':
            axis = BT_AXIS_Y;
            break;
        default:
            axis = BT_AXIS_Z;
            break;
        }
        *out = bt_move_of_rotation(axis, quarter);
        BT_TRACE_EXIT("bt_alg_parse_token");
        return true;
    }
    BT_TRACE_EXIT("bt_alg_parse_token");
    return false;
}

static size_t whitespace_len(const char *s) {
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

bool bt_alg_parse(const char *text, bt_alg *out, char *err_token, size_t err_token_size) {
    BT_TRACE_ENTER("bt_alg_parse");
    bt_alg_free(out);
    const char *cursor = text;
    while (*cursor != '\0') {
        size_t ws = whitespace_len(cursor);
        while (ws > 0) {
            cursor += ws;
            ws = whitespace_len(cursor);
        }
        if (*cursor == '\0') {
            break;
        }
        const char *start = cursor;
        while (*cursor != '\0' && whitespace_len(cursor) == 0) {
            cursor++;
        }
        size_t len = (size_t)(cursor - start);
        char token[64];
        if (len >= sizeof token) {
            if (err_token != NULL) {
                snprintf(err_token, err_token_size, "%.*s",
                         (int)(len < err_token_size - 1 ? len : err_token_size - 1), start);
            }
            BT_TRACE_FAIL("bt_alg_parse", "unknown token: %.*s", (int)len, start);
            bt_alg_free(out);
            BT_TRACE_EXIT("bt_alg_parse");
            return false;
        }
        memcpy(token, start, len);
        token[len] = '\0';
        bt_move mv;
        if (!bt_alg_parse_token(token, &mv)) {
            if (err_token != NULL) {
                snprintf(err_token, err_token_size, "%s", token);
            }
            BT_TRACE_FAIL("bt_alg_parse", "unknown token: %s", token);
            bt_alg_free(out);
            BT_TRACE_EXIT("bt_alg_parse");
            return false;
        }
        if (!bt_alg_push(out, mv)) {
            BT_TRACE_FAIL("bt_alg_parse", "allocation failed");
            bt_alg_free(out);
            BT_TRACE_EXIT("bt_alg_parse");
            return false;
        }
    }
    BT_TRACE_EXIT("bt_alg_parse");
    return true;
}

void bt_alg_format(const bt_alg *alg, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_alg_format");
    for (size_t i = 0; i < alg->len; i++) {
        if (i > 0) {
            bt_strbuf_append_str(out, " ");
        }
        bt_move mv = alg->items[i];
        char prefix[3];
        size_t prefix_len = 0;
        switch (mv.kind) {
        case BT_MOVE_FACE:
            prefix[0] = face_char(mv.face);
            prefix_len = 1;
            break;
        case BT_MOVE_WIDE:
            prefix[0] = lower_char(face_char(mv.face));
            prefix_len = 1;
            break;
        case BT_MOVE_INNER:
            prefix[0] = '2';
            prefix[1] = face_char(mv.face);
            prefix_len = 2;
            break;
        case BT_MOVE_SLICE:
            prefix[0] = slice_char(mv.slice);
            prefix_len = 1;
            break;
        default:
            prefix[0] = axis_char(mv.axis);
            prefix_len = 1;
            break;
        }
        const char *suffix = "";
        if (mv.quarter == 2) {
            suffix = "2";
        } else if (mv.quarter == 3) {
            suffix = "'";
        }
        bt_strbuf_append(out, prefix, prefix_len);
        bt_strbuf_append_str(out, suffix);
    }
    BT_TRACE_EXIT("bt_alg_format");
}

bool bt_alg_try_parse(const char *text, bt_alg *out) {
    BT_TRACE_ENTER("bt_alg_try_parse");
    bool ok = bt_alg_parse(text, out, NULL, 0);
    BT_TRACE_EXIT("bt_alg_try_parse");
    return ok;
}

bt_alg bt_alg_inverse(const bt_alg *alg) {
    BT_TRACE_ENTER("bt_alg_inverse");
    bt_alg out;
    bt_alg_init(&out);
    for (size_t i = alg->len; i > 0; i--) {
        if (!bt_alg_push(&out, bt_move_invert(alg->items[i - 1]))) {
            BT_TRACE_FAIL("bt_alg_inverse", "allocation failed");
            bt_trace_dump();
            abort();
        }
    }
    BT_TRACE_EXIT("bt_alg_inverse");
    return out;
}

uint32_t bt_alg_qtm(const bt_alg *alg) {
    BT_TRACE_ENTER("bt_alg_qtm");
    uint32_t total = 0;
    for (size_t i = 0; i < alg->len; i++) {
        if (alg->items[i].kind == BT_MOVE_ROTATION) {
            continue;
        }
        total += alg->items[i].quarter == 2 ? 2 : 1;
    }
    BT_TRACE_EXIT("bt_alg_qtm");
    return total;
}

uint32_t bt_alg_htm(const bt_alg *alg) {
    BT_TRACE_ENTER("bt_alg_htm");
    uint32_t total = 0;
    for (size_t i = 0; i < alg->len; i++) {
        if (alg->items[i].kind == BT_MOVE_ROTATION) {
            continue;
        }
        total += 1;
    }
    BT_TRACE_EXIT("bt_alg_htm");
    return total;
}
