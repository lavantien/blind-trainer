#include "policy/lexer.h"

#include <stdlib.h>
#include <string.h>

#include "policy/lib.h"

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
    (void)text;
    (void)offset;
    return 1;
}

void policy_mask_source(const char *text, size_t len, policy_lang lang, policy_masked *out) {
    (void)text;
    (void)len;
    (void)lang;
    out->text = policy_xalloc(1);
    out->text[0] = '\0';
    out->comments = NULL;
    out->comment_count = 0;
    out->comment_cap = 0;
}

void policy_masked_free(policy_masked *masked) {
    free(masked->text);
    free(masked->comments);
    masked->text = NULL;
    masked->comments = NULL;
    masked->comment_count = 0;
    masked->comment_cap = 0;
}
