#include "mutate/mutate.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

static const char *const KEYWORDS[] = {
    "alignas",       "alignof",     "auto",          "bool",
    "break",         "case",        "char",          "constexpr",
    "const",         "continue",    "default",       "do",
    "double",        "else",        "enum",          "extern",
    "float",         "for",         "goto",          "if",
    "inline",        "int",         "long",          "nullptr",
    "register",      "restrict",    "return",        "short",
    "signed",        "sizeof",      "static_assert", "static",
    "struct",        "switch",      "thread_local",  "typedef",
    "typeof_unqual", "typeof",      "union",         "unsigned",
    "void",          "volatile",    "while",         "_Alignas",
    "_Alignof",      "_Atomic",     "_BitInt",       "_Bool",
    "_Complex",      "_Decimal128", "_Decimal32",    "_Decimal64",
    "_Generic",      "_Imaginary",  "_Noreturn",     "_Static_assert",
    "_Thread_local",
};

static const char *const OPERATORS[] = {
    "<<=", ">>=", "...", "%:%:", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&",
    "||",  "+=",  "-=",  "*=",   "/=", "%=", "&=", "^=", "|=", "##", "<:", ":>", "<%", "%:",
    "+",   "-",   "*",   "/",    "%",  "&",  "|",  "^",  "~",  "!",  "<",  ">",  "=",  "?",
    ":",   ";",   ",",   ".",    "(",  ")",  "[",  "]",  "{",  "}",  "#",
};

static int is_keyword(const char *word, size_t len) {
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++) {
        if (strlen(KEYWORDS[i]) == len && memcmp(KEYWORDS[i], word, len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int push_token(bt_mut_tokens *tokens, bt_mut_token token) {
    BT_TRACE_ENTER("mutate.push_token");
    if (tokens->count == tokens->cap) {
        size_t want = tokens->cap == 0 ? 64 : tokens->cap * 2;
        bt_mut_token *grown = realloc(tokens->items, want * sizeof(bt_mut_token));
        if (grown == NULL) {
            BT_TRACE_FAIL("mutate.push_token", "oom at %zu", tokens->count);
            BT_TRACE_EXIT("mutate.push_token");
            return -1;
        }
        tokens->items = grown;
        tokens->cap = want;
    }
    tokens->items[tokens->count++] = token;
    BT_TRACE_EXIT("mutate.push_token");
    return 0;
}

static void advance(const char *text, size_t *pos, size_t *line, size_t *col) {
    if (text[*pos] == '\n') {
        (*line)++;
        *col = 1;
    } else {
        (*col)++;
    }
    (*pos)++;
}

static void advance_n(const char *text, size_t *pos, size_t *line, size_t *col, size_t count) {
    for (size_t i = 0; i < count; i++) {
        advance(text, pos, line, col);
    }
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int scan_string(bt_mut_tokens *tokens, const char *text, size_t len, size_t *pos,
                       size_t *line, size_t *col, uint8_t kind) {
    size_t start = *pos;
    size_t start_line = *line;
    size_t start_col = *col;
    char quote = text[*pos];
    advance(text, pos, line, col);
    while (*pos < len) {
        if (text[*pos] == '\\') {
            advance_n(text, pos, line, col, 2);
            continue;
        }
        if (text[*pos] == quote) {
            advance(text, pos, line, col);
            break;
        }
        if (text[*pos] == '\n') {
            break;
        }
        advance(text, pos, line, col);
    }
    bt_mut_token token = {
        .kind = kind,
        .offset = start,
        .len = *pos - start,
        .line = start_line,
        .col = start_col,
        .end_line = *line,
        .end_col = *col == 1 ? 1 : *col - 1,
    };
    return push_token(tokens, token);
}

static int scan_pp_line(bt_mut_tokens *tokens, const char *text, size_t len, size_t *pos,
                        size_t *line, size_t *col) {
    size_t start = *pos;
    size_t start_line = *line;
    size_t start_col = *col;
    while (*pos < len) {
        if (text[*pos] == '\\' && *pos + 1 < len && text[*pos + 1] == '\n') {
            advance_n(text, pos, line, col, 2);
            continue;
        }
        if (text[*pos] == '\n') {
            break;
        }
        advance(text, pos, line, col);
    }
    bt_mut_token token = {
        .kind = BT_MUT_TOK_PP,
        .offset = start,
        .len = *pos - start,
        .line = start_line,
        .col = start_col,
        .end_line = *line,
        .end_col = *col == 1 ? 1 : *col - 1,
    };
    return push_token(tokens, token);
}

static int is_pp_number_start(const char *text, size_t pos, size_t len) {
    if (is_digit(text[pos])) {
        return 1;
    }
    return text[pos] == '.' && pos + 1 < len && is_digit(text[pos + 1]);
}

static int scan_pp_number(bt_mut_tokens *tokens, const char *text, size_t len, size_t *pos,
                          size_t *line, size_t *col) {
    size_t start = *pos;
    size_t start_line = *line;
    size_t start_col = *col;
    advance(text, pos, line, col);
    while (*pos < len) {
        char c = text[*pos];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            c == '_' || c == '\'' || c == '.') {
            advance(text, pos, line, col);
            continue;
        }
        if ((c == '+' || c == '-') && *pos > start &&
            (text[*pos - 1] == 'e' || text[*pos - 1] == 'E' || text[*pos - 1] == 'p' ||
             text[*pos - 1] == 'P')) {
            advance(text, pos, line, col);
            continue;
        }
        break;
    }
    bt_mut_token token = {
        .kind = BT_MUT_TOK_NUMBER,
        .offset = start,
        .len = *pos - start,
        .line = start_line,
        .col = start_col,
        .end_line = *line,
        .end_col = *col == 1 ? 1 : *col - 1,
    };
    return push_token(tokens, token);
}

static int scan_identifier(bt_mut_tokens *tokens, const char *text, size_t len, size_t *pos,
                           size_t *line, size_t *col) {
    size_t start = *pos;
    size_t start_line = *line;
    size_t start_col = *col;
    while (*pos < len && is_ident_char(text[*pos])) {
        advance(text, pos, line, col);
    }
    uint8_t kind = is_keyword(text + start, *pos - start) ? BT_MUT_TOK_KEYWORD : BT_MUT_TOK_IDENT;
    bt_mut_token token = {
        .kind = kind,
        .offset = start,
        .len = *pos - start,
        .line = start_line,
        .col = start_col,
        .end_line = *line,
        .end_col = *col == 1 ? 1 : *col - 1,
    };
    return push_token(tokens, token);
}

static int scan_operator(bt_mut_tokens *tokens, const char *text, size_t len, size_t *pos,
                         size_t *line, size_t *col) {
    size_t start = *pos;
    size_t start_line = *line;
    size_t start_col = *col;
    size_t best = 0;
    for (size_t i = 0; i < sizeof OPERATORS / sizeof OPERATORS[0]; i++) {
        size_t op_len = strlen(OPERATORS[i]);
        if (op_len > best && *pos + op_len <= len &&
            memcmp(text + *pos, OPERATORS[i], op_len) == 0) {
            best = op_len;
        }
    }
    if (best == 0) {
        best = 1;
    }
    advance_n(text, pos, line, col, best);
    bt_mut_token token = {
        .kind = BT_MUT_TOK_OP,
        .offset = start,
        .len = best,
        .line = start_line,
        .col = start_col,
        .end_line = *line,
        .end_col = *col == 1 ? 1 : *col - 1,
    };
    return push_token(tokens, token);
}

int bt_mut_tokenize(const char *text, size_t len, bt_mut_tokens *out) {
    BT_TRACE_ENTER("mutate.tokenize");
    out->text = NULL;
    out->text_len = 0;
    out->items = NULL;
    out->count = 0;
    out->cap = 0;
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        BT_TRACE_FAIL("mutate.tokenize", "oom");
        BT_TRACE_EXIT("mutate.tokenize");
        return -1;
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
    out->text = copy;
    out->text_len = len;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;
    int rc = 0;
    while (pos < len && rc == 0) {
        char c = text[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            advance(text, &pos, &line, &col);
            continue;
        }
        if (c == '/' && pos + 1 < len && text[pos + 1] == '/') {
            while (pos < len && text[pos] != '\n') {
                advance(text, &pos, &line, &col);
            }
            continue;
        }
        if (c == '/' && pos + 1 < len && text[pos + 1] == '*') {
            advance_n(text, &pos, &line, &col, 2);
            while (pos + 1 < len && !(text[pos] == '*' && text[pos + 1] == '/')) {
                advance(text, &pos, &line, &col);
            }
            if (pos + 1 < len) {
                advance_n(text, &pos, &line, &col, 2);
            } else {
                pos = len;
            }
            continue;
        }
        if (c == '#') {
            rc = scan_pp_line(out, text, len, &pos, &line, &col);
            continue;
        }
        size_t prefix_len = 0;
        if (pos + 1 < len && text[pos + 1] >= '0' && text[pos + 1] <= '9') {
            prefix_len = 0;
        } else if (pos + 1 < len && (text[pos] == 'L' || text[pos] == 'u' || text[pos] == 'U') &&
                   (text[pos + 1] == '"' || text[pos + 1] == '\'')) {
            prefix_len = 1;
        } else if (pos + 2 < len && text[pos] == 'u' && text[pos + 1] == '8' &&
                   (text[pos + 2] == '"' || text[pos + 2] == '\'')) {
            prefix_len = 2;
        }
        if (prefix_len > 0) {
            advance_n(text, &pos, &line, &col, prefix_len);
            rc = scan_string(out, text, len, &pos, &line, &col,
                             text[pos] == '"' ? BT_MUT_TOK_STRING : BT_MUT_TOK_CHAR);
            continue;
        }
        if (c == '"' || c == '\'') {
            rc = scan_string(out, text, len, &pos, &line, &col,
                             c == '"' ? BT_MUT_TOK_STRING : BT_MUT_TOK_CHAR);
            continue;
        }
        if (is_pp_number_start(text, pos, len)) {
            rc = scan_pp_number(out, text, len, &pos, &line, &col);
            continue;
        }
        if (is_ident_start(c)) {
            rc = scan_identifier(out, text, len, &pos, &line, &col);
            continue;
        }
        rc = scan_operator(out, text, len, &pos, &line, &col);
    }
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.tokenize", "failed at offset %zu", pos);
    }
    BT_TRACE_EXIT("mutate.tokenize");
    return rc;
}

void bt_mut_tokens_free(bt_mut_tokens *tokens) {
    free(tokens->text);
    free(tokens->items);
    tokens->text = NULL;
    tokens->items = NULL;
    tokens->count = 0;
}

const char *bt_mut_token_text(const bt_mut_tokens *tokens, const bt_mut_token *token) {
    return tokens->text + token->offset;
}

size_t bt_mut_offset_at(const char *text, size_t line, size_t col) {
    size_t current_line = 1;
    size_t current_col = 1;
    for (size_t pos = 0; text[pos] != '\0'; pos++) {
        if (current_line == line && current_col == col) {
            return pos;
        }
        if (text[pos] == '\n') {
            current_line++;
            current_col = 1;
        } else {
            current_col++;
        }
    }
    return (size_t)-1;
}

static void function_name_for_open(const bt_mut_tokens *tokens, size_t open_index, char *out,
                                   size_t cap) {
    out[0] = '\0';
    if (open_index < 3) {
        return;
    }
    const bt_mut_token *close = &tokens->items[open_index - 1];
    const char *close_text = bt_mut_token_text(tokens, close);
    if (close->kind != BT_MUT_TOK_OP || close->len != 1 || close_text[0] != ')') {
        return;
    }
    size_t paren_depth = 1;
    size_t j = open_index - 1;
    while (j > 0 && paren_depth > 0) {
        j--;
        const bt_mut_token *scan = &tokens->items[j];
        if (scan->kind != BT_MUT_TOK_OP || scan->len != 1) {
            continue;
        }
        const char *scan_text = bt_mut_token_text(tokens, scan);
        if (scan_text[0] == ')') {
            paren_depth++;
        } else if (scan_text[0] == '(') {
            paren_depth--;
        }
    }
    if (paren_depth != 0 || tokens->items[j].kind != BT_MUT_TOK_OP) {
        return;
    }
    size_t k = j;
    while (k > 0 && tokens->items[k].kind != BT_MUT_TOK_IDENT) {
        k--;
    }
    if (tokens->items[k].kind != BT_MUT_TOK_IDENT) {
        return;
    }
    size_t len = tokens->items[k].len;
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out, tokens->text + tokens->items[k].offset, len);
    out[len] = '\0';
}

const char *bt_mut_function_at(const bt_mut_tokens *tokens, size_t offset) {
    static char name[128];
    static const char *empty = "";
    size_t depth = 0;
    int inside = 0;
    const char *current = empty;
    for (size_t i = 0; i < tokens->count; i++) {
        const bt_mut_token *token = &tokens->items[i];
        if (token->kind != BT_MUT_TOK_OP || token->len != 1) {
            continue;
        }
        const char *text = bt_mut_token_text(tokens, token);
        if (text[0] == '{') {
            if (depth == 0) {
                function_name_for_open(tokens, i, name, sizeof name);
                current = name;
                inside = offset >= token->offset;
            }
            depth++;
            continue;
        }
        if (text[0] == '}') {
            if (inside && depth == 1 && token->offset >= offset) {
                return current;
            }
            depth = depth > 0 ? depth - 1 : 0;
            if (depth == 0) {
                inside = 0;
                current = empty;
            }
            continue;
        }
        if (inside && depth >= 1 && token->offset > offset) {
            return current;
        }
    }
    if (depth >= 1 && inside) {
        return current;
    }
    return empty;
}
