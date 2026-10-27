#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/sha256.h"

const char *bt_mut_op_name(bt_mut_op op) {
    switch (op) {
    case BT_MUT_NUMERIC_TWEAK:
        return "NumericTweak";
    case BT_MUT_COMPARISON_SWAP:
        return "ComparisonSwap";
    case BT_MUT_CONDITION_NEGATION:
        return "ConditionNegation";
    case BT_MUT_UNARY_DROP:
        return "UnaryDrop";
    case BT_MUT_STATEMENT_DELETE:
        return "StatementDelete";
    }
    return "Unknown";
}

static char *text_dup(const char *text, size_t len) {
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static char *token_dup(const bt_mut_tokens *tokens, const bt_mut_token *token) {
    return text_dup(bt_mut_token_text(tokens, token), token->len);
}

static void free_mutant_fields(bt_mut_mutant *mutant) {
    free(mutant->file);
    free(mutant->from);
    free(mutant->to);
    for (size_t i = 0; i < mutant->edit_count; i++) {
        free(mutant->edits[i].text);
    }
}

void bt_mut_mutants_free(bt_mut_mutant_list *list) {
    for (size_t i = 0; i < list->count; i++) {
        free_mutant_fields(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static int list_push(bt_mut_mutant_list *list, const bt_mut_mutant *mutant) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].id, mutant->id) == 0) {
            return 1;
        }
    }
    if (list->count == list->cap) {
        size_t want = list->cap == 0 ? 16 : list->cap * 2;
        bt_mut_mutant *grown = realloc(list->items, want * sizeof(bt_mut_mutant));
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->cap = want;
    }
    bt_mut_mutant *slot = &list->items[list->count];
    *slot = *mutant;
    list->count++;
    return 0;
}

static void mutant_id(bt_mut_mutant *mutant) {
    char payload[BT_MUT_LINE_MAX];
    snprintf(payload, sizeof payload, "%s\t%zu\t%zu\t%s\t%s\t%s", mutant->file, mutant->line,
             mutant->col, bt_mut_op_name(mutant->op), mutant->from, mutant->to);
    unsigned char digest[32];
    bt_sha256(payload, strlen(payload), digest);
    char hex[65];
    bt_sha256_hex(digest, hex);
    memcpy(mutant->id, hex, BT_MUT_ID_LEN);
    mutant->id[BT_MUT_ID_LEN] = '\0';
}

static int init_mutant(bt_mut_mutant *mutant, const char *file, size_t line, size_t col,
                       bt_mut_op op, const char *from, const char *to) {
    memset(mutant, 0, sizeof *mutant);
    mutant->file = text_dup(file, strlen(file));
    mutant->from = text_dup(from, strlen(from));
    mutant->to = text_dup(to, strlen(to));
    mutant->op = op;
    mutant->line = line;
    mutant->col = col;
    if (mutant->file == NULL || mutant->from == NULL || mutant->to == NULL) {
        free_mutant_fields(mutant);
        return -1;
    }
    mutant_id(mutant);
    return 0;
}

static int push_replace(bt_mut_mutant_list *list, const char *file, const bt_mut_token *token,
                        bt_mut_op op, const char *from, const char *to) {
    bt_mut_mutant mutant;
    if (init_mutant(&mutant, file, token->line, token->col, op, from, to) != 0) {
        return -1;
    }
    mutant.end_line = token->end_line;
    mutant.end_col = token->end_col;
    mutant.edits[0].offset = token->offset;
    mutant.edits[0].len = token->len;
    mutant.edits[0].text = text_dup(to, strlen(to));
    mutant.edit_count = 1;
    if (mutant.edits[0].text == NULL) {
        free_mutant_fields(&mutant);
        return -1;
    }
    int rc = list_push(list, &mutant);
    if (rc != 0) {
        free_mutant_fields(&mutant);
    }
    return rc < 0 ? -1 : 0;
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int numeric_core(const char *token, size_t len, char *core, char *suffix) {
    int hex = len > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X');
    int has_dot = 0;
    int has_exp = 0;
    for (size_t i = 0; i < len; i++) {
        if (token[i] == '.') {
            has_dot = 1;
        }
        if (hex && (token[i] == 'p' || token[i] == 'P')) {
            has_exp = 1;
        }
        if (!hex && (token[i] == 'e' || token[i] == 'E') && i > 0 && is_digit(token[i - 1])) {
            has_exp = 1;
        }
    }
    int is_float = has_dot || has_exp;
    size_t end = len;
    while (end > 0) {
        char c = token[end - 1];
        if (c == '\'') {
            end--;
            continue;
        }
        if (c == 'u' || c == 'U' || c == 'l' || c == 'L') {
            end--;
            continue;
        }
        if ((c == 'f' || c == 'F') && !hex) {
            end--;
            continue;
        }
        break;
    }
    size_t out = 0;
    for (size_t i = 0; i < end; i++) {
        if (token[i] != '\'') {
            core[out++] = token[i];
        }
    }
    core[out] = '\0';
    out = 0;
    for (size_t i = end; i < len; i++) {
        if (token[i] != '\'') {
            suffix[out++] = token[i];
        }
    }
    suffix[out] = '\0';
    return is_float;
}

static void format_float(char *out, size_t cap, double value) {
    snprintf(out, cap, "%.17g", value);
    if (strpbrk(out, ".eE") == NULL) {
        strncat(out, ".0", cap - strlen(out) - 1);
    }
}

static unsigned long long parse_int_core(const char *core) {
    int base = 10;
    const char *digits = core;
    if (core[0] == '0' && (core[1] == 'x' || core[1] == 'X')) {
        base = 16;
        digits = core + 2;
    } else if (core[0] == '0' && (core[1] == 'b' || core[1] == 'B')) {
        base = 2;
        digits = core + 2;
    } else if (core[0] == '0' && core[1] != '\0') {
        base = 8;
        digits = core + 1;
    }
    return strtoull(digits, NULL, base);
}

static int enumerate_numeric(bt_mut_mutant_list *list, const char *file, const char *token_text,
                             const bt_mut_token *token) {
    char core[128];
    char suffix[16];
    int is_float = numeric_core(token_text, token->len, core, suffix);
    char variants[4][64];
    if (is_float) {
        double value = strtod(core, NULL);
        format_float(variants[0], sizeof variants[0], value + 1.0);
        format_float(variants[1], sizeof variants[1], value - 1.0);
        format_float(variants[2], sizeof variants[2], 0.0);
        format_float(variants[3], sizeof variants[3], 1.0);
    } else {
        unsigned long long value = parse_int_core(core);
        snprintf(variants[0], sizeof variants[0], "%llu", value + 1ULL);
        snprintf(variants[1], sizeof variants[1], "%s", value == 0 ? "-1" : "");
        if (value != 0) {
            snprintf(variants[1], sizeof variants[1], "%llu", value - 1ULL);
        }
        snprintf(variants[2], sizeof variants[2], "0");
        snprintf(variants[3], sizeof variants[3], "1");
    }
    for (int i = 0; i < 4; i++) {
        char to[96];
        snprintf(to, sizeof to, "%s%s", variants[i], suffix);
        if (strcmp(to, token_text) == 0) {
            continue;
        }
        int rc = push_replace(list, file, token, BT_MUT_NUMERIC_TWEAK, token_text, to);
        if (rc != 0) {
            return -1;
        }
    }
    return 0;
}

static const char *comparison_swap(const char *op, size_t len) {
    if (len == 1 && op[0] == '<') {
        return "<=";
    }
    if (len == 2 && op[0] == '<' && op[1] == '=') {
        return "<";
    }
    if (len == 1 && op[0] == '>') {
        return ">=";
    }
    if (len == 2 && op[0] == '>' && op[1] == '=') {
        return ">";
    }
    if (len == 2 && op[0] == '=' && op[1] == '=') {
        return "!=";
    }
    if (len == 2 && op[0] == '!' && op[1] == '=') {
        return "==";
    }
    return NULL;
}

static int op_is_single(const bt_mut_tokens *tokens, const bt_mut_token *token, char c) {
    return token->kind == BT_MUT_TOK_OP && token->len == 1 &&
           bt_mut_token_text(tokens, token)[0] == c;
}

static int op_text_equals(const bt_mut_tokens *tokens, const bt_mut_token *token, const char *op) {
    size_t len = strlen(op);
    return token->kind == BT_MUT_TOK_OP && token->len == len &&
           memcmp(bt_mut_token_text(tokens, token), op, len) == 0;
}

static int match_paren(const bt_mut_tokens *tokens, size_t open_index, size_t *close_index) {
    size_t depth = 1;
    for (size_t i = open_index + 1; i < tokens->count; i++) {
        const bt_mut_token *token = &tokens->items[i];
        if (!op_is_single(tokens, token, '(') && !op_is_single(tokens, token, ')')) {
            continue;
        }
        if (op_is_single(tokens, token, '(')) {
            depth++;
        } else {
            depth--;
            if (depth == 0) {
                *close_index = i;
                return 0;
            }
        }
    }
    return -1;
}

static int enumerate_negation(bt_mut_mutant_list *list, const char *file,
                              const bt_mut_tokens *tokens, size_t keyword_index) {
    const bt_mut_token *keyword = &tokens->items[keyword_index];
    if (keyword_index + 1 >= tokens->count) {
        return 0;
    }
    const bt_mut_token *open = &tokens->items[keyword_index + 1];
    if (!op_is_single(tokens, open, '(')) {
        return 0;
    }
    size_t close_index = 0;
    if (match_paren(tokens, keyword_index + 1, &close_index) != 0) {
        return 0;
    }
    for (size_t i = keyword_index + 1; i <= close_index; i++) {
        if (tokens->items[i].kind == BT_MUT_TOK_PP) {
            return 0;
        }
    }
    const bt_mut_token *close = &tokens->items[close_index];
    char *keyword_text = token_dup(tokens, keyword);
    if (keyword_text == NULL) {
        return -1;
    }
    bt_mut_mutant mutant;
    if (init_mutant(&mutant, file, keyword->line, keyword->col, BT_MUT_CONDITION_NEGATION,
                    keyword_text, "!(...)") != 0) {
        free(keyword_text);
        return -1;
    }
    free(keyword_text);
    mutant.end_line = close->end_line;
    mutant.end_col = close->end_col;
    mutant.edits[0].offset = open->offset + open->len;
    mutant.edits[0].len = 0;
    mutant.edits[0].text = text_dup("!(", 2);
    mutant.edits[1].offset = close->offset;
    mutant.edits[1].len = close->len;
    mutant.edits[1].text = text_dup("))", 2);
    mutant.edit_count = 2;
    if (mutant.edits[0].text == NULL || mutant.edits[1].text == NULL) {
        free_mutant_fields(&mutant);
        return -1;
    }
    int rc = list_push(list, &mutant);
    if (rc != 0) {
        free_mutant_fields(&mutant);
    }
    return rc < 0 ? -1 : 0;
}

static int statement_deletable(const bt_mut_tokens *tokens, size_t start, size_t end) {
    if (start > end) {
        return 0;
    }
    const bt_mut_token *first = &tokens->items[start];
    if (first->kind != BT_MUT_TOK_IDENT) {
        return 0;
    }
    if (end > start && op_text_equals(tokens, &tokens->items[start + 1], ":")) {
        return 0;
    }
    for (size_t i = start; i <= end; i++) {
        const bt_mut_token *token = &tokens->items[i];
        if (token->kind == BT_MUT_TOK_PP || token->kind == BT_MUT_TOK_KEYWORD) {
            return 0;
        }
        if (op_is_single(tokens, token, '{') || op_is_single(tokens, token, '}')) {
            return 0;
        }
    }
    return 1;
}

static int enumerate_statement_delete(bt_mut_mutant_list *list, const char *file,
                                      const bt_mut_tokens *tokens, size_t start, size_t end) {
    if (!statement_deletable(tokens, start, end)) {
        return 0;
    }
    const bt_mut_token *first = &tokens->items[start];
    const bt_mut_token *last = &tokens->items[end];
    const char *from = bt_mut_token_text(tokens, first);
    char from_text[128];
    size_t from_len = first->len < sizeof from_text ? first->len : sizeof from_text - 1;
    memcpy(from_text, from, from_len);
    from_text[from_len] = '\0';
    bt_mut_mutant mutant;
    if (init_mutant(&mutant, file, first->line, first->col, BT_MUT_STATEMENT_DELETE, from_text,
                    "") != 0) {
        return -1;
    }
    mutant.end_line = last->end_line;
    mutant.end_col = last->end_col;
    mutant.edits[0].offset = first->offset;
    mutant.edits[0].len = last->offset + last->len - first->offset;
    mutant.edits[0].text = text_dup(" ", 1);
    mutant.edit_count = 1;
    if (mutant.edits[0].text == NULL) {
        free_mutant_fields(&mutant);
        return -1;
    }
    int rc = list_push(list, &mutant);
    if (rc != 0) {
        free_mutant_fields(&mutant);
    }
    return rc < 0 ? -1 : 0;
}

int bt_mut_enumerate(const char *file_rel, const char *text, size_t len, bt_mut_mutant_list *out) {
    BT_TRACE_ENTER("mutate.enumerate");
    out->items = NULL;
    out->count = 0;
    out->cap = 0;
    bt_mut_tokens tokens;
    if (bt_mut_tokenize(text, len, &tokens) != 0) {
        BT_TRACE_FAIL("mutate.enumerate", "tokenize failed for %s", file_rel);
        BT_TRACE_EXIT("mutate.enumerate");
        return -1;
    }
    int rc = 0;
    size_t statement_start = 0;
    size_t brace_depth = 0;
    size_t paren_depth = 0;
    for (size_t i = 0; i < tokens.count && rc == 0; i++) {
        const bt_mut_token *token = &tokens.items[i];
        if (token->kind == BT_MUT_TOK_NUMBER) {
            char *number_text = token_dup(&tokens, token);
            if (number_text == NULL) {
                rc = -1;
                break;
            }
            rc = enumerate_numeric(out, file_rel, number_text, token);
            free(number_text);
            continue;
        }
        if (token->kind == BT_MUT_TOK_KEYWORD) {
            char *word = token_dup(&tokens, token);
            if (word == NULL) {
                rc = -1;
                break;
            }
            int guard = strcmp(word, "if") == 0 || strcmp(word, "while") == 0;
            free(word);
            if (guard) {
                rc = enumerate_negation(out, file_rel, &tokens, i);
            }
            continue;
        }
        if (token->kind != BT_MUT_TOK_OP) {
            continue;
        }
        const char *op_raw = bt_mut_token_text(&tokens, token);
        const char *swapped = comparison_swap(op_raw, token->len);
        if (swapped != NULL) {
            char *op_text = token_dup(&tokens, token);
            if (op_text == NULL) {
                rc = -1;
                break;
            }
            rc = push_replace(out, file_rel, token, BT_MUT_COMPARISON_SWAP, op_text, swapped);
            free(op_text);
            continue;
        }
        if (op_is_single(&tokens, token, '{') || op_is_single(&tokens, token, '}')) {
            if (op_raw[0] == '{') {
                brace_depth++;
            } else {
                brace_depth = brace_depth > 0 ? brace_depth - 1 : 0;
            }
            statement_start = i + 1;
            continue;
        }
        if (op_is_single(&tokens, token, '(')) {
            paren_depth++;
            continue;
        }
        if (op_is_single(&tokens, token, ')')) {
            paren_depth = paren_depth > 0 ? paren_depth - 1 : 0;
            continue;
        }
        if (op_is_single(&tokens, token, ';')) {
            if (brace_depth >= 1 && paren_depth == 0) {
                rc = enumerate_statement_delete(out, file_rel, &tokens, statement_start, i);
            }
            statement_start = i + 1;
            continue;
        }
        if ((token->len == 1 && op_raw[0] == '!') ||
            (token->len == 2 && op_raw[0] == '+' && op_raw[1] == '+') ||
            (token->len == 2 && op_raw[0] == '-' && op_raw[1] == '-')) {
            char *op_text = token_dup(&tokens, token);
            if (op_text == NULL) {
                rc = -1;
                break;
            }
            rc = push_replace(out, file_rel, token, BT_MUT_UNARY_DROP, op_text, "");
            free(op_text);
            continue;
        }
    }
    bt_mut_tokens_free(&tokens);
    if (rc != 0) {
        bt_mut_mutants_free(out);
        BT_TRACE_FAIL("mutate.enumerate", "failed for %s", file_rel);
    }
    BT_TRACE_EXIT("mutate.enumerate");
    return rc;
}

int bt_mut_apply(const char *text, size_t len, const bt_mut_mutant *mutant, char **out,
                 size_t *out_len) {
    BT_TRACE_ENTER("mutate.apply");
    size_t removed = 0;
    size_t inserted = 0;
    for (size_t i = 0; i < mutant->edit_count; i++) {
        removed += mutant->edits[i].len;
        inserted += strlen(mutant->edits[i].text);
    }
    size_t new_len = len - removed + inserted;
    char *result = malloc(new_len + 1);
    if (result == NULL) {
        BT_TRACE_FAIL("mutate.apply", "oom");
        BT_TRACE_EXIT("mutate.apply");
        return -1;
    }
    size_t src = 0;
    size_t dst = 0;
    for (size_t i = 0; i < mutant->edit_count; i++) {
        const bt_mut_edit *edit = &mutant->edits[i];
        if (edit->offset > len || src > edit->offset) {
            free(result);
            BT_TRACE_FAIL("mutate.apply", "edit out of range");
            BT_TRACE_EXIT("mutate.apply");
            return -1;
        }
        memcpy(result + dst, text + src, edit->offset - src);
        dst += edit->offset - src;
        size_t edit_len = strlen(edit->text);
        memcpy(result + dst, edit->text, edit_len);
        dst += edit_len;
        src = edit->offset + edit->len;
    }
    memcpy(result + dst, text + src, len - src);
    result[new_len] = '\0';
    *out = result;
    *out_len = new_len;
    BT_TRACE_EXIT("mutate.apply");
    return 0;
}

const bt_mut_mutant *bt_mut_find(const bt_mut_mutant_list *list, bt_mut_op op, const char *from,
                                 const char *to, size_t line) {
    for (size_t i = 0; i < list->count; i++) {
        const bt_mut_mutant *mutant = &list->items[i];
        if (mutant->op == op && mutant->line == line && strcmp(mutant->from, from) == 0 &&
            strcmp(mutant->to, to) == 0) {
            return mutant;
        }
    }
    return NULL;
}
