#include "common/strbuf.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

int bt_strbuf_init(bt_strbuf *sb, size_t cap) {
    BT_TRACE_ENTER("strbuf.init");
    if (cap < 16) {
        cap = 16;
    }
    sb->data = malloc(cap + 1);
    if (sb->data == NULL) {
        BT_TRACE_FAIL("strbuf.init", "malloc of %zu failed", cap + 1);
        BT_TRACE_EXIT("strbuf.init");
        return -1;
    }
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = cap;
    BT_TRACE_EXIT("strbuf.init");
    return 0;
}

void bt_strbuf_free(bt_strbuf *sb) {
    BT_TRACE_ENTER("strbuf.free");
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    BT_TRACE_EXIT("strbuf.free");
}

int bt_strbuf_reserve(bt_strbuf *sb, size_t extra) {
    if (sb->len + extra + 1 <= sb->cap) {
        return 0;
    }
    size_t want = sb->cap * 2;
    while (want < sb->len + extra + 1) {
        want *= 2;
    }
    char *grown = realloc(sb->data, want + 1);
    if (grown == NULL) {
        BT_TRACE_FAIL("strbuf.reserve", "realloc to %zu failed", want + 1);
        return -1;
    }
    sb->data = grown;
    sb->cap = want;
    return 0;
}

int bt_strbuf_append(bt_strbuf *sb, const char *text, size_t len) {
    BT_TRACE_ENTER("strbuf.append");
    if (bt_strbuf_reserve(sb, len) != 0) {
        BT_TRACE_EXIT("strbuf.append");
        return -1;
    }
    memcpy(sb->data + sb->len, text, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
    BT_TRACE_EXIT("strbuf.append");
    return 0;
}

int bt_strbuf_append_str(bt_strbuf *sb, const char *text) {
    return bt_strbuf_append(sb, text, strlen(text));
}

int bt_strbuf_appendf(bt_strbuf *sb, const char *fmt, ...) {
    BT_TRACE_ENTER("strbuf.appendf");
    va_list ap;
    va_start(ap, fmt);
    va_list probe;
    va_copy(probe, ap);
    int needed = vsnprintf(NULL, 0, fmt, probe);
    va_end(probe);
    if (needed < 0) {
        va_end(ap);
        BT_TRACE_FAIL("strbuf.appendf", "format rejected");
        BT_TRACE_EXIT("strbuf.appendf");
        return -1;
    }
    if (bt_strbuf_reserve(sb, (size_t)needed) != 0) {
        va_end(ap);
        BT_TRACE_EXIT("strbuf.appendf");
        return -1;
    }
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    sb->len += (size_t)needed;
    BT_TRACE_EXIT("strbuf.appendf");
    return 0;
}

void bt_strbuf_trim(bt_strbuf *sb) {
    BT_TRACE_ENTER("strbuf.trim");
    size_t start = 0;
    while (start < sb->len && isspace((unsigned char)sb->data[start])) {
        start++;
    }
    size_t end = sb->len;
    while (end > start && isspace((unsigned char)sb->data[end - 1])) {
        end--;
    }
    sb->len = end - start;
    memmove(sb->data, sb->data + start, sb->len);
    sb->data[sb->len] = '\0';
    BT_TRACE_EXIT("strbuf.trim");
}

int bt_strbuf_join(bt_strbuf *sb, const char *sep, const char *const *parts, size_t count) {
    BT_TRACE_ENTER("strbuf.join");
    size_t sep_len = strlen(sep);
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && bt_strbuf_append(sb, sep, sep_len) != 0) {
            BT_TRACE_FAIL("strbuf.join", "separator append failed at %zu", i);
            BT_TRACE_EXIT("strbuf.join");
            return -1;
        }
        if (bt_strbuf_append_str(sb, parts[i]) != 0) {
            BT_TRACE_FAIL("strbuf.join", "part append failed at %zu", i);
            BT_TRACE_EXIT("strbuf.join");
            return -1;
        }
    }
    BT_TRACE_EXIT("strbuf.join");
    return 0;
}

size_t bt_tsv_split(char *line, char **fields, size_t max_fields) {
    BT_TRACE_ENTER("tsv.split");
    size_t count = 1;
    for (const char *cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor == '\t') {
            count++;
        }
    }
    if (count > max_fields) {
        BT_TRACE_FAIL("tsv.split", "%zu fields exceed max %zu", count, max_fields);
        BT_TRACE_EXIT("tsv.split");
        return BT_TSV_OVERFLOW;
    }
    size_t index = 0;
    fields[index++] = line;
    for (char *cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor == '\t') {
            *cursor = '\0';
            fields[index++] = cursor + 1;
        }
    }
    BT_TRACE_EXIT("tsv.split");
    return count;
}
