#ifndef BT_STRBUF_H
#define BT_STRBUF_H

#include <stddef.h>

#define BT_TSV_OVERFLOW ((size_t)-1)

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} bt_strbuf;

int bt_strbuf_init(bt_strbuf *sb, size_t cap);
void bt_strbuf_free(bt_strbuf *sb);
int bt_strbuf_reserve(bt_strbuf *sb, size_t extra);
int bt_strbuf_append(bt_strbuf *sb, const char *text, size_t len);
int bt_strbuf_append_str(bt_strbuf *sb, const char *text);
int bt_strbuf_appendf(bt_strbuf *sb, const char *fmt, ...);
void bt_strbuf_trim(bt_strbuf *sb);
int bt_strbuf_join(bt_strbuf *sb, const char *sep, const char *const *parts, size_t count);
size_t bt_tsv_split(char *line, char **fields, size_t max_fields);

#endif
