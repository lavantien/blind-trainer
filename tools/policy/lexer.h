#ifndef POLICY_LEXER_H
#define POLICY_LEXER_H

#include <stddef.h>

typedef enum {
    POLICY_COMMENT_LINE,
    POLICY_COMMENT_BLOCK,
    POLICY_COMMENT_HASH,
} policy_comment_kind;

typedef enum {
    POLICY_LANG_C,
    POLICY_LANG_PY,
} policy_lang;

typedef struct {
    size_t start;
    size_t end;
    policy_comment_kind kind;
} policy_comment_span;

typedef struct {
    char *text;
    policy_comment_span *comments;
    size_t comment_count;
    size_t comment_cap;
} policy_masked;

const char *policy_comment_kind_str(policy_comment_kind kind);
policy_lang policy_lang_of_path(const char *path);
size_t policy_line_of(const char *text, size_t offset);
void policy_mask_source(const char *text, size_t len, policy_lang lang, policy_masked *out);
void policy_masked_free(policy_masked *masked);

#endif
