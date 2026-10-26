#include "policy/lexer.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "policy/lib.h"

#define POLICY_LEXER_NONE ((size_t)-1)

static int lex_is_word(unsigned char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '_';
}

static int is_ident_start(unsigned char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || byte == '_';
}

static size_t utf8_width(unsigned char byte) {
    if (byte < 0x80) {
        return 1;
    }
    if (((unsigned int)byte >> 5U) == 0x6U) {
        return 2;
    }
    if (((unsigned int)byte >> 4U) == 0xEU) {
        return 3;
    }
    return 4;
}

const char *policy_comment_kind_str(policy_comment_kind kind) {
    switch (kind) {
    case POLICY_COMMENT_LINE:
        return "line";
    case POLICY_COMMENT_BLOCK:
        return "block";
    case POLICY_COMMENT_HASH:
        return "hash";
    }
    return "unknown";
}

policy_lang policy_lang_of_path(const char *path) {
    size_t len = strlen(path);
    if (len >= 3 && strcmp(path + len - 3, ".py") == 0) {
        return POLICY_LANG_PY;
    }
    return POLICY_LANG_C;
}

size_t policy_line_of(const char *text, size_t offset) {
    size_t line = 1;
    for (size_t at = 0; at < offset; at++) {
        if (text[at] == '\n') {
            line++;
        }
    }
    return line;
}

static void push_span(policy_masked *out, size_t start, size_t end, policy_comment_kind kind) {
    if (out->comment_count == out->comment_cap) {
        size_t cap = out->comment_cap == 0 ? 8 : out->comment_cap * 2;
        policy_comment_span *grown = realloc(out->comments, cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("policy.lexer", "comment span growth failed at %zu", cap);
            fputs("bt-policy: out of memory\n", stderr);
            exit(2);
        }
        out->comments = grown;
        out->comment_cap = cap;
    }
    out->comments[out->comment_count].start = start;
    out->comments[out->comment_count].end = end;
    out->comments[out->comment_count].kind = kind;
    out->comment_count++;
}

static size_t c_line_end(const char *text, size_t len, size_t from) {
    for (size_t at = from; at < len; at++) {
        if (text[at] == '\n') {
            return at;
        }
    }
    return len;
}

static size_t c_block_end(const char *text, size_t len, size_t start) {
    size_t j = start + 2;
    while (j + 1 < len) {
        if (text[j] == '*' && text[j + 1] == '/') {
            return j + 2;
        }
        j++;
    }
    return len;
}

static size_t c_string_close(const char *text, size_t len, size_t start) {
    size_t j = start + 1;
    while (j < len) {
        if (text[j] == '\\') {
            j += 2;
            continue;
        }
        if (text[j] == '"') {
            return j;
        }
        j++;
    }
    return POLICY_LEXER_NONE;
}

static int lex_is_hex_digit(unsigned char byte) {
    return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f') ||
           (byte >= 'A' && byte <= 'F');
}

static size_t c_char_close(const char *text, size_t len, size_t start) {
    size_t q = start + 1;
    if (q >= len || text[q] == '\'') {
        return POLICY_LEXER_NONE;
    }
    if (text[q] == '\\') {
        size_t j = q + 1;
        if (j >= len) {
            return POLICY_LEXER_NONE;
        }
        if (text[j] == 'x') {
            j++;
            while (j < len && lex_is_hex_digit((unsigned char)text[j])) {
                j++;
            }
        } else if (text[j] >= '0' && text[j] <= '7') {
            int digits = 0;
            while (digits < 3 && j < len && text[j] >= '0' && text[j] <= '7') {
                j++;
                digits++;
            }
        } else {
            j++;
        }
        return (j < len && text[j] == '\'') ? j + 1 : POLICY_LEXER_NONE;
    }
    size_t width = utf8_width((unsigned char)text[q]);
    return (q + width < len && text[q + width] == '\'') ? q + width + 1 : POLICY_LEXER_NONE;
}

static int is_c_encoding_prefix(const char *text, size_t start, size_t end) {
    size_t len = end - start;
    if (len == 1) {
        return text[start] == 'u' || text[start] == 'U' || text[start] == 'L';
    }
    if (len == 2) {
        return text[start] == 'u' && text[start + 1] == '8';
    }
    return 0;
}

static int is_py_string_prefix(const char *text, size_t start, size_t end) {
    for (size_t at = start; at < end; at++) {
        unsigned char byte = (unsigned char)text[at];
        int ok = byte == 'r' || byte == 'R' || byte == 'b' || byte == 'B' || byte == 'u' ||
                 byte == 'U' || byte == 'f' || byte == 'F';
        if (!ok) {
            return 0;
        }
    }
    return end > start;
}

static size_t py_string_body(char *out, const char *text, size_t len, size_t quote) {
    char delim = text[quote];
    int triple = quote + 2 < len && text[quote + 1] == delim && text[quote + 2] == delim;
    size_t start = triple ? quote + 3 : quote + 1;
    size_t j = start;
    while (j < len) {
        if (text[j] == '\\') {
            j += 2;
            continue;
        }
        if (text[j] == '\n' && !triple) {
            memset(out + start, ' ', j - start);
            return j;
        }
        if (triple) {
            if (text[j] == delim && j + 2 < len && text[j + 1] == delim && text[j + 2] == delim) {
                memset(out + start, ' ', j - start);
                return j + 3;
            }
        } else if (text[j] == delim) {
            memset(out + start, ' ', j - start);
            return j + 1;
        }
        j++;
    }
    memset(out + start, ' ', len - start);
    return len;
}

void policy_mask_source(const char *text, size_t len, policy_lang lang, policy_masked *out) {
    BT_TRACE_ENTER("policy.mask_source");
    out->text = policy_xalloc(len + 1);
    memcpy(out->text, text, len);
    out->text[len] = '\0';
    out->comments = NULL;
    out->comment_count = 0;
    out->comment_cap = 0;
    char *masked = out->text;
    size_t i = 0;
    while (i < len) {
        unsigned char byte = (unsigned char)text[i];
        if (lang == POLICY_LANG_C) {
            if (byte == '/' && i + 1 < len && text[i + 1] == '/') {
                size_t end = c_line_end(text, len, i + 2);
                push_span(out, i, end, POLICY_COMMENT_LINE);
                memset(masked + i, ' ', end - i);
                i = end;
                continue;
            }
            if (byte == '/' && i + 1 < len && text[i + 1] == '*') {
                size_t end = c_block_end(text, len, i);
                push_span(out, i, end, POLICY_COMMENT_BLOCK);
                memset(masked + i, ' ', end - i);
                i = end;
                continue;
            }
            if (byte == '"') {
                size_t close = c_string_close(text, len, i);
                if (close != POLICY_LEXER_NONE) {
                    memset(masked + i + 1, ' ', close - i - 1);
                    i = close + 1;
                } else {
                    memset(masked + i + 1, ' ', len - i - 1);
                    i = len;
                }
                continue;
            }
            if (byte == '\'') {
                size_t end = c_char_close(text, len, i);
                if (end != POLICY_LEXER_NONE) {
                    memset(masked + i + 1, ' ', end - 1 - (i + 1));
                    i = end;
                } else {
                    i++;
                }
                continue;
            }
        } else {
            if (byte == '#') {
                size_t end = c_line_end(text, len, i + 1);
                push_span(out, i, end, POLICY_COMMENT_HASH);
                memset(masked + i, ' ', end - i);
                i = end;
                continue;
            }
            if (byte == '"' || byte == '\'') {
                i = py_string_body(masked, text, len, i);
                continue;
            }
        }
        if (is_ident_start(byte)) {
            size_t start = i;
            while (i < len && lex_is_word((unsigned char)text[i])) {
                i++;
            }
            if (lang == POLICY_LANG_C) {
                if (i < len && text[i] == '"' && is_c_encoding_prefix(text, start, i)) {
                    size_t close = c_string_close(text, len, i);
                    if (close != POLICY_LEXER_NONE) {
                        memset(masked + i + 1, ' ', close - i - 1);
                        i = close + 1;
                    } else {
                        memset(masked + i + 1, ' ', len - i - 1);
                        i = len;
                    }
                    continue;
                }
                if (i < len && text[i] == '\'' && is_c_encoding_prefix(text, start, i)) {
                    size_t end = c_char_close(text, len, i);
                    if (end != POLICY_LEXER_NONE) {
                        memset(masked + i + 1, ' ', end - 1 - (i + 1));
                        i = end;
                        continue;
                    }
                }
            } else if (i < len && (text[i] == '"' || text[i] == '\'') &&
                       is_py_string_prefix(text, start, i)) {
                i = py_string_body(masked, text, len, i);
                continue;
            }
            continue;
        }
        i++;
    }
    BT_TRACE_EXIT("policy.mask_source");
}

void policy_masked_free(policy_masked *masked) {
    free(masked->text);
    free(masked->comments);
    masked->text = NULL;
    masked->comments = NULL;
    masked->comment_count = 0;
    masked->comment_cap = 0;
}
