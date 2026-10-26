#include "core/expand.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

typedef enum {
    TOK_MV = 0,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_COLON,
    TOK_PLUS,
    TOK_COUNT,
} tok_kind;

typedef struct {
    uint8_t kind;
    bt_move mv;
    size_t count;
} expand_tok;

typedef enum {
    STOP_END = 0,
    STOP_PAREN,
    STOP_BRACKET,
} expand_stop;

typedef struct {
    expand_tok *items;
    size_t len;
    size_t cap;
    size_t index;
} expand_tokens;

static bool tok_push(expand_tokens *tokens, expand_tok tok) {
    if (tokens->len == tokens->cap) {
        size_t cap = tokens->cap == 0 ? 16 : tokens->cap * 2U;
        expand_tok *items = realloc(tokens->items, cap * sizeof *items);
        if (items == NULL) {
            return false;
        }
        tokens->items = items;
        tokens->cap = cap;
    }
    tokens->items[tokens->len++] = tok;
    return true;
}

static bool tok_last_is_rparen(const expand_tokens *tokens) {
    if (tokens->len == 0) {
        return false;
    }
    return tokens->items[tokens->len - 1].kind == TOK_RPAREN;
}

static bool flush_word(expand_tokens *tokens, char *word, size_t *word_len) {
    if (*word_len == 0) {
        return true;
    }
    word[*word_len] = '\0';
    bt_move mv;
    if (!bt_alg_parse_token(word, &mv)) {
        return false;
    }
    *word_len = 0;
    return tok_push(tokens, (expand_tok){TOK_MV, mv, 0});
}

static bool parse_count_digits(const char *text, size_t start, size_t end, size_t *out) {
    size_t value = 0;
    for (size_t i = start; i < end; i++) {
        unsigned digit = (unsigned)(text[i] - '0');
        if (value > (SIZE_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    *out = value;
    return true;
}

static bool expand_tokenize(const char *text, expand_tokens *tokens) {
    size_t len = strlen(text);
    char word[256];
    size_t word_len = 0;
    size_t index = 0;
    while (index < len) {
        char ch = text[index];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (!flush_word(tokens, word, &word_len)) {
                return false;
            }
            index += 1;
        } else if (ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == ',' || ch == ':' ||
                   ch == '+') {
            if (!flush_word(tokens, word, &word_len)) {
                return false;
            }
            uint8_t kind;
            switch (ch) {
            case '[':
                kind = TOK_LBRACKET;
                break;
            case ']':
                kind = TOK_RBRACKET;
                break;
            case '(':
                kind = TOK_LPAREN;
                break;
            case ')':
                kind = TOK_RPAREN;
                break;
            case ',':
                kind = TOK_COMMA;
                break;
            case ':':
                kind = TOK_COLON;
                break;
            default:
                kind = TOK_PLUS;
                break;
            }
            if (!tok_push(tokens, (expand_tok){kind, {0}, 0})) {
                return false;
            }
            index += 1;
        } else if (isdigit((unsigned char)ch) && word_len == 0 && tok_last_is_rparen(tokens)) {
            size_t start = index;
            while (index < len && isdigit((unsigned char)text[index])) {
                index += 1;
            }
            size_t count;
            if (!parse_count_digits(text, start, index, &count)) {
                return false;
            }
            if (!tok_push(tokens, (expand_tok){TOK_COUNT, {0}, count})) {
                return false;
            }
        } else {
            if (word_len + 1 >= sizeof word) {
                return false;
            }
            word[word_len++] = ch;
            index += 1;
        }
    }
    return flush_word(tokens, word, &word_len);
}

static bool expand_peek_is(const expand_tokens *tokens, uint8_t kind) {
    if (tokens->index >= tokens->len) {
        return false;
    }
    return tokens->items[tokens->index].kind == kind;
}

static bool expand_seq(expand_tokens *tokens, size_t depth, expand_stop stop, bt_alg *out);

static bool alg_append(bt_alg *dst, const bt_alg *src) {
    for (size_t i = 0; i < src->len; i++) {
        if (!bt_alg_push(dst, src->items[i])) {
            return false;
        }
    }
    return true;
}

static bool alg_append_inverse(bt_alg *dst, const bt_alg *src) {
    for (size_t i = src->len; i > 0; i--) {
        if (!bt_alg_push(dst, bt_move_invert(src->items[i - 1]))) {
            return false;
        }
    }
    return true;
}

static bool expand_unit(expand_tokens *tokens, size_t depth, expand_stop stop, bt_alg *out) {
    bt_alg setup;
    bt_alg_init(&setup);
    if (!expand_seq(tokens, depth, stop, &setup)) {
        bt_alg_free(&setup);
        return false;
    }
    if (expand_peek_is(tokens, TOK_COLON)) {
        if (depth >= BT_EXPAND_MAX_DEPTH) {
            bt_alg_free(&setup);
            return false;
        }
        tokens->index += 1;
        bt_alg core;
        bt_alg_init(&core);
        if (!expand_unit(tokens, depth + 1, stop, &core)) {
            bt_alg_free(&setup);
            bt_alg_free(&core);
            return false;
        }
        bool ok = alg_append(out, &setup);
        if (ok) {
            ok = alg_append(out, &core);
        }
        if (ok) {
            ok = alg_append_inverse(out, &setup);
        }
        bt_alg_free(&setup);
        bt_alg_free(&core);
        return ok;
    }
    bool ok = alg_append(out, &setup);
    bt_alg_free(&setup);
    return ok;
}

static bool expand_sum(expand_tokens *tokens, size_t depth, expand_stop stop, bt_alg *out) {
    if (!expand_unit(tokens, depth, stop, out)) {
        return false;
    }
    while (expand_peek_is(tokens, TOK_PLUS)) {
        tokens->index += 1;
        if (!expand_unit(tokens, depth, stop, out)) {
            return false;
        }
    }
    return true;
}

static bool expand_seq(expand_tokens *tokens, size_t depth, expand_stop stop, bt_alg *out) {
    while (tokens->index < tokens->len) {
        expand_tok tok = tokens->items[tokens->index];
        if (tok.kind == TOK_PLUS || tok.kind == TOK_COLON) {
            break;
        }
        if ((tok.kind == TOK_COMMA || tok.kind == TOK_RBRACKET) && stop == STOP_BRACKET) {
            break;
        }
        if (tok.kind == TOK_RPAREN && stop == STOP_PAREN) {
            break;
        }
        if (tok.kind == TOK_COMMA || tok.kind == TOK_RBRACKET || tok.kind == TOK_RPAREN ||
            tok.kind == TOK_COUNT) {
            return false;
        }
        if (tok.kind == TOK_LPAREN) {
            if (depth >= BT_EXPAND_MAX_DEPTH) {
                return false;
            }
            tokens->index += 1;
            bt_alg inner;
            bt_alg_init(&inner);
            if (!expand_sum(tokens, depth + 1, STOP_PAREN, &inner)) {
                bt_alg_free(&inner);
                return false;
            }
            if (!expand_peek_is(tokens, TOK_RPAREN)) {
                bt_alg_free(&inner);
                return false;
            }
            tokens->index += 1;
            size_t repeat = 1;
            if (expand_peek_is(tokens, TOK_COUNT)) {
                repeat = tokens->items[tokens->index].count;
                tokens->index += 1;
            }
            if (repeat == 0 || repeat > BT_EXPAND_REPEAT_LIMIT) {
                bt_alg_free(&inner);
                return false;
            }
            if (inner.len > BT_EXPAND_EXPAND_LIMIT / repeat) {
                bt_alg_free(&inner);
                return false;
            }
            bool ok = true;
            for (size_t r = 0; ok && r < repeat; r++) {
                ok = alg_append(out, &inner);
            }
            bt_alg_free(&inner);
            if (!ok) {
                return false;
            }
        } else if (tok.kind == TOK_LBRACKET) {
            if (depth >= BT_EXPAND_MAX_DEPTH) {
                return false;
            }
            tokens->index += 1;
            bt_alg first;
            bt_alg_init(&first);
            if (!expand_sum(tokens, depth + 1, STOP_BRACKET, &first)) {
                bt_alg_free(&first);
                return false;
            }
            if (expand_peek_is(tokens, TOK_COMMA)) {
                tokens->index += 1;
                bt_alg second;
                bt_alg_init(&second);
                if (!expand_sum(tokens, depth + 1, STOP_BRACKET, &second)) {
                    bt_alg_free(&first);
                    bt_alg_free(&second);
                    return false;
                }
                if (!expand_peek_is(tokens, TOK_RBRACKET)) {
                    bt_alg_free(&first);
                    bt_alg_free(&second);
                    return false;
                }
                tokens->index += 1;
                bool ok = alg_append(out, &first);
                if (ok) {
                    ok = alg_append(out, &second);
                }
                if (ok) {
                    ok = alg_append_inverse(out, &first);
                }
                if (ok) {
                    ok = alg_append_inverse(out, &second);
                }
                bt_alg_free(&first);
                bt_alg_free(&second);
                if (!ok) {
                    return false;
                }
            } else if (expand_peek_is(tokens, TOK_RBRACKET)) {
                tokens->index += 1;
                bool ok = alg_append(out, &first);
                bt_alg_free(&first);
                if (!ok) {
                    return false;
                }
            } else {
                bt_alg_free(&first);
                return false;
            }
        } else if (tok.kind == TOK_MV) {
            if (!bt_alg_push(out, tok.mv)) {
                return false;
            }
            tokens->index += 1;
        }
    }
    return out->len > 0;
}

void bt_expand_cancel(const bt_alg *in, bt_alg *out) {
    BT_TRACE_ENTER("bt_expand_cancel");
    bt_alg_free(out);
    for (size_t i = 0; i < in->len; i++) {
        bt_move mv = in->items[i];
        bool merged = false;
        if (out->len > 0) {
            bt_move top = out->items[out->len - 1];
            if (bt_move_eq(bt_move_with_quarter(top, 1), bt_move_with_quarter(mv, 1))) {
                uint8_t quarter = (uint8_t)((top.quarter + mv.quarter) % 4U);
                if (quarter == 0) {
                    out->len -= 1;
                } else {
                    out->items[out->len - 1] = bt_move_with_quarter(top, quarter);
                }
                merged = true;
            }
        }
        if (!merged && !bt_alg_push(out, mv)) {
            BT_TRACE_FAIL("bt_expand_cancel", "allocation failed");
            bt_trace_dump();
            abort();
        }
    }
    BT_TRACE_EXIT("bt_expand_cancel");
}

bool bt_expand(const char *text, bt_alg *out) {
    BT_TRACE_ENTER("bt_expand");
    bt_alg_free(out);
    expand_tokens tokens = {0};
    if (!expand_tokenize(text, &tokens)) {
        free(tokens.items);
        BT_TRACE_EXIT("bt_expand");
        return false;
    }
    tokens.index = 0;
    bt_alg moves;
    bt_alg_init(&moves);
    bool ok = expand_sum(&tokens, 0, STOP_END, &moves);
    free(tokens.items);
    if (!ok || moves.len > BT_EXPAND_EXPAND_LIMIT) {
        bt_alg_free(&moves);
        BT_TRACE_EXIT("bt_expand");
        return false;
    }
    bt_alg cancelled;
    bt_alg_init(&cancelled);
    bt_expand_cancel(&moves, &cancelled);
    bt_alg_free(&moves);
    if (cancelled.len == 0) {
        bt_alg_free(&cancelled);
        BT_TRACE_EXIT("bt_expand");
        return false;
    }
    *out = cancelled;
    BT_TRACE_EXIT("bt_expand");
    return true;
}
