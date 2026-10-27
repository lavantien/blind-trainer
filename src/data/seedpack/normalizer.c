#include "data/seedpack/normalizer.h"

#include "common/bt_trace.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const long long TURN_MIN = -5;
static const long long HALF_RING = 6;

void bt_normalize_error_string(const bt_normalize_error *err, char *out, size_t out_len) {
    BT_TRACE_ENTER("normalizer.error_string");
    switch (err->kind) {
    case BT_NORMALIZE_ERR_UNKNOWN_TOKEN:
        snprintf(out, out_len, "unknown token '%s' in alg '%s'", err->token, err->alg);
        break;
    case BT_NORMALIZE_ERR_UNBALANCED:
        snprintf(out, out_len, "unbalanced parens in alg '%s'", err->alg);
        break;
    case BT_NORMALIZE_ERR_BAD_REPEAT:
        snprintf(out, out_len, "malformed repeat marker in alg '%s'", err->alg);
        break;
    case BT_NORMALIZE_ERR_BAD_CHAIN:
        snprintf(out, out_len, "chain marker with an empty alg in '%s'", err->alg);
        break;
    case BT_NORMALIZE_ERR_EMPTY_ALG:
        snprintf(out, out_len, "alg is empty");
        break;
    }
    BT_TRACE_EXIT("normalizer.error_string");
}

void bt_normalized_free(bt_normalized *normalized) {
    free(normalized->alg);
    for (size_t i = 0; i < normalized->note_count; i++) {
        free(normalized->notes[i]);
    }
    free(normalized->notes);
    normalized->alg = NULL;
    normalized->notes = NULL;
    normalized->note_count = 0;
}

static void copy_alg(bt_normalize_error *err, const char *alg) {
    snprintf(err->alg, sizeof err->alg, "%.*s", (int)(sizeof err->alg - 1), alg);
}

static void set_error(bt_normalize_error *err, bt_normalize_error_kind kind, const char *alg,
                      const char *token) {
    err->kind = kind;
    copy_alg(err, alg);
    if (token != NULL) {
        snprintf(err->token, sizeof err->token, "%.*s", (int)(sizeof err->token - 1), token);
    } else {
        err->token[0] = '\0';
    }
}

static int notes_push(bt_normalized *out, const char *fmt, ...) {
    char buffer[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof buffer, fmt, ap);
    va_end(ap);
    char **grown = realloc(out->notes, (out->note_count + 1) * sizeof *grown);
    if (grown == NULL) {
        return -1;
    }
    out->notes = grown;
    out->notes[out->note_count] = malloc(strlen(buffer) + 1);
    if (out->notes[out->note_count] == NULL) {
        return -1;
    }
    strcpy(out->notes[out->note_count], buffer);
    out->note_count += 1;
    return 0;
}

static char *dup_range(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static char *trim_dup(const char *text) {
    size_t len = strlen(text);
    size_t start = 0;
    size_t end = len;
    while (start < end && isspace((unsigned char)text[start])) {
        start += 1;
    }
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end -= 1;
    }
    return dup_range(text + start, end - start);
}

static void trim_in_place(char *text) {
    size_t len = strlen(text);
    size_t start = 0;
    while (start < len && isspace((unsigned char)text[start])) {
        start += 1;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end -= 1;
    }
    memmove(text, text + start, end - start);
    text[end - start] = '\0';
}

static void trim_start_in_place(char *text) {
    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start += 1;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text) - start + 1);
    }
}

static void trim_end_in_place(char *text) {
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }
}

static int str_assign(char **dst, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        return -1;
    }
    char *next = malloc((size_t)needed + 1);
    if (next == NULL) {
        return -1;
    }
    va_start(ap, fmt);
    vsnprintf(next, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    free(*dst);
    *dst = next;
    return 0;
}

static bool is_prose_word(const char *token) {
    size_t len = strlen(token);
    if (len < 2) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (token[i] < 'a' || token[i] > 'z') {
            return false;
        }
    }
    return true;
}

static bool valid_cube_token(const char *token) {
    size_t len = strlen(token);
    size_t index = 0;
    if (len > 1 && isdigit((unsigned char)token[0])) {
        if (token[0] < '2' || token[0] > '7') {
            return false;
        }
        index = 1;
    }
    if (index >= len || !isalpha((unsigned char)token[index])) {
        return false;
    }
    if (token[index] == 'w' || token[index] == 'W') {
        return false;
    }
    index += 1;
    if (index < len && (token[index] == 'w' || (isupper((unsigned char)token[index]) &&
                                                isupper((unsigned char)token[index - 1])))) {
        index += 1;
    }
    bool prime = false;
    bool two = false;
    while (index < len) {
        if (token[index] == '\'' && !prime) {
            prime = true;
        } else if (token[index] == '2' && !two) {
            two = true;
        } else {
            return false;
        }
        index += 1;
    }
    return true;
}

static int parse_count(const char *digits, size_t digit_len, size_t *out) {
    char buffer[32];
    if (digit_len == 0 || digit_len >= sizeof buffer) {
        return -1;
    }
    memcpy(buffer, digits, digit_len);
    buffer[digit_len] = '\0';
    errno = 0;
    unsigned long long parsed = strtoull(buffer, NULL, 10);
    if (errno == ERANGE || parsed == 0 || parsed > BT_SEEDPACK_REPEAT_LIMIT) {
        return -1;
    }
    *out = (size_t)parsed;
    return 0;
}

static int build_repeated(char **out, const char *content, size_t count) {
    size_t content_len = strlen(content);
    size_t total = count > 0 ? count * content_len + (count - 1) : 0;
    if (total > BT_SEEDPACK_EXPAND_LIMIT) {
        return -1;
    }
    char *joined = malloc(total + 1);
    if (joined == NULL) {
        return -2;
    }
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            joined[used++] = ' ';
        }
        memcpy(joined + used, content, content_len);
        used += content_len;
    }
    joined[used] = '\0';
    free(*out);
    *out = joined;
    return 0;
}

static bool expand_bracket_repeats(const char *alg_text, char **current, bt_normalize_error *err) {
    char *content = NULL;
    char *after = NULL;
    char *repeated = NULL;
    bool ok = false;
    while (true) {
        const char *open = strchr(*current, '[');
        if (open == NULL) {
            ok = true;
            break;
        }
        const char *close = strchr(open + 1, ']');
        if (close == NULL) {
            break;
        }
        size_t content_len = (size_t)(close - open - 1);
        if (memchr(open + 1, '[', content_len) != NULL) {
            break;
        }
        free(content);
        content = dup_range(open + 1, content_len);
        if (content == NULL) {
            break;
        }
        trim_in_place(content);
        const char *tail = close + 1;
        while (*tail != '\0' && isspace((unsigned char)*tail)) {
            tail += 1;
        }
        if (*tail != '*') {
            break;
        }
        const char *star_tail = tail + 1;
        size_t digit_len = 0;
        while (isdigit((unsigned char)star_tail[digit_len])) {
            digit_len += 1;
        }
        size_t count = 0;
        if (parse_count(star_tail, digit_len, &count) != 0) {
            break;
        }
        free(after);
        after = dup_range(star_tail + digit_len, strlen(star_tail + digit_len));
        if (after == NULL) {
            break;
        }
        trim_start_in_place(after);
        if (after[0] != '\0') {
            break;
        }
        if (build_repeated(&repeated, content, count) != 0) {
            break;
        }
        char *prefix = dup_range(*current, (size_t)(open - *current));
        if (prefix == NULL) {
            break;
        }
        int rc = str_assign(current, "%s %s %s", prefix, repeated, after);
        free(prefix);
        if (rc != 0) {
            break;
        }
        trim_in_place(*current);
    }
    free(content);
    free(after);
    free(repeated);
    if (!ok) {
        set_error(err, BT_NORMALIZE_ERR_BAD_REPEAT, alg_text, NULL);
    }
    return ok;
}

typedef struct {
    size_t open;
    size_t close;
    size_t count;
    size_t digits;
    bool found;
    bool failed;
} bt_repeat_hit;

static void find_group_repeat(const char *text, const char *alg_text, bt_repeat_hit *hit,
                              bt_normalize_error *err) {
    size_t stack[256];
    size_t depth = 0;
    size_t len = strlen(text);
    hit->found = false;
    hit->failed = false;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '(') {
            if (depth < 256) {
                stack[depth] = i;
            }
            depth += 1;
            continue;
        }
        if (text[i] != ')') {
            continue;
        }
        if (depth == 0) {
            set_error(err, BT_NORMALIZE_ERR_UNBALANCED, alg_text, NULL);
            hit->failed = true;
            return;
        }
        depth -= 1;
        if (depth >= 256) {
            continue;
        }
        size_t open = stack[depth];
        bool inner_clean = true;
        for (size_t j = open + 1; j < i; j++) {
            if (text[j] == '(' || text[j] == ')') {
                inner_clean = false;
                break;
            }
        }
        if (!inner_clean) {
            continue;
        }
        size_t digits = 0;
        while (isdigit((unsigned char)text[i + 1 + digits])) {
            digits += 1;
        }
        if (digits > 0) {
            size_t count = 0;
            if (parse_count(text + i + 1, digits, &count) != 0) {
                set_error(err, BT_NORMALIZE_ERR_BAD_REPEAT, alg_text, NULL);
                hit->failed = true;
                return;
            }
            char after = text[i + 1 + digits];
            if (isalnum((unsigned char)after) || after == '\'') {
                set_error(err, BT_NORMALIZE_ERR_BAD_REPEAT, alg_text, NULL);
                hit->failed = true;
                return;
            }
            hit->open = open;
            hit->close = i;
            hit->count = count;
            hit->digits = digits;
            hit->found = true;
            return;
        }
        char next = text[i + 1];
        if (next != '\0' && (isalnum((unsigned char)next) || next == '\'')) {
            set_error(err, BT_NORMALIZE_ERR_BAD_REPEAT, alg_text, NULL);
            hit->failed = true;
            return;
        }
    }
    if (depth != 0) {
        set_error(err, BT_NORMALIZE_ERR_UNBALANCED, alg_text, NULL);
        hit->failed = true;
    }
}

static bool expand_group_repeats(const char *alg_text, char **current, bt_normalize_error *err) {
    char *content = NULL;
    char *tail = NULL;
    char *repeated = NULL;
    bool ok = false;
    while (true) {
        bt_repeat_hit hit;
        find_group_repeat(*current, alg_text, &hit, err);
        if (hit.failed) {
            break;
        }
        if (!hit.found) {
            ok = true;
            break;
        }
        free(content);
        content = dup_range(*current + hit.open + 1, hit.close - hit.open - 1);
        if (content == NULL) {
            break;
        }
        trim_in_place(content);
        if (build_repeated(&repeated, content, hit.count) != 0) {
            break;
        }
        free(tail);
        tail = dup_range(*current + hit.close + 1 + hit.digits,
                         strlen(*current + hit.close + 1 + hit.digits));
        if (tail == NULL) {
            break;
        }
        trim_in_place(tail);
        char *prefix = dup_range(*current, hit.open);
        if (prefix == NULL) {
            break;
        }
        int rc = str_assign(current, "%s %s %s", prefix, repeated, tail);
        free(prefix);
        if (rc != 0) {
            break;
        }
        trim_in_place(*current);
    }
    free(content);
    free(tail);
    free(repeated);
    if (!ok && err->kind != BT_NORMALIZE_ERR_UNBALANCED) {
        set_error(err, BT_NORMALIZE_ERR_BAD_REPEAT, alg_text, NULL);
    }
    return ok;
}

static bool strip_grouping_parens(const char *alg_text, char *current, bt_normalize_error *err) {
    long long depth = 0;
    size_t used = 0;
    for (const char *cursor = current; *cursor != '\0'; cursor++) {
        if (*cursor == '(') {
            depth += 1;
            continue;
        }
        if (*cursor == ')') {
            depth -= 1;
            if (depth < 0) {
                set_error(err, BT_NORMALIZE_ERR_UNBALANCED, alg_text, NULL);
                return false;
            }
            continue;
        }
        current[used++] = *cursor;
    }
    if (depth != 0) {
        set_error(err, BT_NORMALIZE_ERR_UNBALANCED, alg_text, NULL);
        return false;
    }
    current[used] = '\0';
    return true;
}

static bool is_auf_inner(const char *inner) {
    char *copy = malloc(strlen(inner) + 1);
    if (copy == NULL) {
        return false;
    }
    strcpy(copy, inner);
    size_t count = 0;
    bool ok = true;
    char *cursor = copy;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor += 1;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
        count += 1;
        if (count > 2 || (start[0] != 'U' && start[0] != 'D')) {
            ok = false;
            break;
        }
        for (const char *p = start + 1; *p != '\0'; p++) {
            if (*p != '2' && *p != '\'') {
                ok = false;
                break;
            }
        }
        if (!ok) {
            break;
        }
    }
    free(copy);
    return (ok && count >= 1 && count <= 2) != 0;
}

static bool valid_int(const char *text) {
    size_t len = strlen(text);
    const char *digits = text;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        digits = text + 1;
    }
    size_t digit_len = strlen(digits);
    if (digit_len == 0) {
        return false;
    }
    for (size_t i = 0; i < digit_len; i++) {
        if (!isdigit((unsigned char)digits[i])) {
            return false;
        }
    }
    if (digit_len > 1 && digits[0] == '0') {
        return false;
    }
    if (negative && strcmp(digits, "0") == 0) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)text[i]) && text[i] != '-') {
            return false;
        }
    }
    return true;
}

static bool canonical_tuple(const char *inner, char *out, size_t out_len, bool *wrapped) {
    const char *comma = strchr(inner, ',');
    if (comma == NULL) {
        return false;
    }
    char *top = dup_range(inner, (size_t)(comma - inner));
    char *bottom = dup_range(comma + 1, strlen(comma + 1));
    if (top == NULL || bottom == NULL) {
        free(top);
        free(bottom);
        return false;
    }
    bool ok = (valid_int(top) && valid_int(bottom)) != 0;
    *wrapped = false;
    char cells[2][32];
    const char *parts[2] = {top, bottom};
    if (ok) {
        for (int i = 0; i < 2; i++) {
            errno = 0;
            long long value = strtoll(parts[i], NULL, 10);
            long long mapped;
            if (errno == ERANGE) {
                ok = false;
                break;
            }
            if (value == -HALF_RING) {
                mapped = HALF_RING;
            } else if (value >= TURN_MIN && value <= HALF_RING) {
                mapped = value;
            } else {
                ok = false;
                break;
            }
            if (mapped != value) {
                *wrapped = true;
            }
            snprintf(cells[i], sizeof cells[i], "%lld", mapped);
        }
    }
    free(top);
    free(bottom);
    if (!ok) {
        return false;
    }
    if (strcmp(cells[0], "0") == 0 && strcmp(cells[1], "0") == 0) {
        return false;
    }
    snprintf(out, out_len, "(%s,%s)", cells[0], cells[1]);
    return true;
}

static bool normalize_cube(const char *raw, bt_normalized *out, bt_normalize_error *err) {
    char *trimmed = trim_dup(raw);
    char *current = NULL;
    char *spaced = NULL;
    char **tokens = NULL;
    char *joined = NULL;
    bool ok = false;
    if (trimmed == NULL) {
        goto done;
    }
    if (trimmed[0] == '\0') {
        set_error(err, BT_NORMALIZE_ERR_EMPTY_ALG, trimmed, NULL);
        goto done;
    }
    current = dup_range(trimmed, strlen(trimmed));
    if (current == NULL) {
        goto done;
    }
    const char *arrow = strstr(current, "->");
    if (arrow != NULL) {
        char *alg_part = dup_range(current, (size_t)(arrow - current));
        char *note_part = dup_range(arrow + 2, strlen(arrow + 2));
        if (alg_part == NULL || note_part == NULL) {
            free(alg_part);
            free(note_part);
            goto done;
        }
        trim_in_place(alg_part);
        trim_in_place(note_part);
        if (alg_part[0] == '\0' || note_part[0] == '\0') {
            set_error(err, BT_NORMALIZE_ERR_BAD_CHAIN, trimmed, NULL);
            free(alg_part);
            free(note_part);
            goto done;
        }
        if (notes_push(out, "chain note: -> %s", note_part) != 0) {
            free(alg_part);
            free(note_part);
            goto done;
        }
        free(current);
        current = alg_part;
        free(note_part);
    }
    if (!expand_bracket_repeats(trimmed, &current, err)) {
        goto done;
    }
    if (!expand_group_repeats(trimmed, &current, err)) {
        goto done;
    }
    if (!strip_grouping_parens(trimmed, current, err)) {
        goto done;
    }
    spaced = dup_range(current, strlen(current));
    if (spaced == NULL) {
        goto done;
    }
    for (char *p = spaced; *p != '\0'; p++) {
        if (*p == ',') {
            *p = ' ';
        }
    }
    size_t token_cap = 16;
    tokens = malloc(token_cap * sizeof *tokens);
    if (tokens == NULL) {
        goto done;
    }
    size_t token_count = 0;
    char *cursor = spaced;
    while (*cursor != '\0') {
        while (isspace((unsigned char)*cursor)) {
            cursor += 1;
        }
        if (*cursor == '\0') {
            break;
        }
        if (token_count == token_cap) {
            token_cap *= 2;
            char **grown = realloc(tokens, token_cap * sizeof *grown);
            if (grown == NULL) {
                goto done;
            }
            tokens = grown;
        }
        tokens[token_count++] = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            cursor += 1;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    if (token_count == 0) {
        set_error(err, BT_NORMALIZE_ERR_EMPTY_ALG, trimmed, NULL);
        goto done;
    }
    bool all_prose = true;
    for (size_t i = 0; i < token_count; i++) {
        if (!is_prose_word(tokens[i])) {
            all_prose = false;
            break;
        }
    }
    if (all_prose) {
        out->alg = dup_range("", 0);
        if (out->alg == NULL || notes_push(out, "prose alg: %s", trimmed) != 0) {
            goto done;
        }
        ok = true;
        goto done;
    }
    joined = dup_range("", 0);
    if (joined == NULL) {
        goto done;
    }
    for (size_t i = 0; i < token_count; i++) {
        const char *token = tokens[i];
        if (strcmp(token, "X") == 0) {
            if (notes_push(out, "mapped X to x") != 0) {
                goto done;
            }
            token = "x";
        } else if (!valid_cube_token(token)) {
            set_error(err, BT_NORMALIZE_ERR_UNKNOWN_TOKEN, trimmed, token);
            goto done;
        }
        if (str_assign(&joined, "%s%s%s", joined, i == 0 ? "" : " ", token) != 0) {
            goto done;
        }
    }
    out->alg = joined;
    joined = NULL;
    ok = true;
done:
    free(trimmed);
    free(current);
    free(spaced);
    free(tokens);
    free(joined);
    return ok;
}

static bool normalize_sq1(const char *raw, bt_normalized *out, bt_normalize_error *err) {
    char *trimmed = trim_dup(raw);
    char *current = NULL;
    char **parts = NULL;
    char *joined = NULL;
    size_t part_cap = 16;
    size_t part_count = 0;
    bool ok = false;
    if (trimmed == NULL) {
        goto done;
    }
    if (trimmed[0] == '\0') {
        set_error(err, BT_NORMALIZE_ERR_EMPTY_ALG, trimmed, NULL);
        goto done;
    }
    current = dup_range(trimmed, strlen(trimmed));
    if (current == NULL) {
        goto done;
    }
    size_t len = strlen(current);
    if (current[len - 1] == '*') {
        current[len - 1] = '\0';
        trim_end_in_place(current);
        if (current[0] == '\0') {
            set_error(err, BT_NORMALIZE_ERR_UNKNOWN_TOKEN, trimmed, "*");
            goto done;
        }
        if (notes_push(out, "equator marker stripped: *") != 0) {
            goto done;
        }
    }
    if (current[0] == '(') {
        const char *close = strchr(current, ')');
        if (close != NULL) {
            char *inner = dup_range(current + 1, (size_t)(close - current - 1));
            if (inner == NULL) {
                goto done;
            }
            if (is_auf_inner(inner)) {
                const char *rest = close + 1;
                if (rest[0] == '\0' || isspace((unsigned char)rest[0]) || rest[0] == '(') {
                    if (notes_push(out, "auf prefix stripped: (%s)", inner) != 0) {
                        free(inner);
                        goto done;
                    }
                    memmove(current, rest, strlen(rest) + 1);
                    trim_start_in_place(current);
                }
            }
            free(inner);
        }
    }
    if (current[0] == '\0') {
        set_error(err, BT_NORMALIZE_ERR_EMPTY_ALG, trimmed, NULL);
        goto done;
    }
    parts = malloc(part_cap * sizeof *parts);
    if (parts == NULL) {
        goto done;
    }
    const char *cursor = current;
    while (*cursor != '\0') {
        if (isspace((unsigned char)*cursor)) {
            cursor += 1;
            continue;
        }
        char tuple[32];
        if (*cursor == '/') {
            snprintf(tuple, sizeof tuple, "/");
            cursor += 1;
        } else if (*cursor == '(') {
            const char *close = strchr(cursor + 1, ')');
            if (close == NULL) {
                set_error(err, BT_NORMALIZE_ERR_UNBALANCED, trimmed, NULL);
                goto done;
            }
            char *inner = dup_range(cursor + 1, (size_t)(close - cursor - 1));
            if (inner == NULL) {
                goto done;
            }
            bool wrapped;
            if (!canonical_tuple(inner, tuple, sizeof tuple, &wrapped)) {
                char *token = dup_range(cursor, (size_t)(close - cursor + 1));
                if (token != NULL) {
                    set_error(err, BT_NORMALIZE_ERR_UNKNOWN_TOKEN, trimmed, token);
                    free(token);
                }
                free(inner);
                goto done;
            }
            if (wrapped && notes_push(out, "wraparound: (%s) -> %s", inner, tuple) != 0) {
                free(inner);
                goto done;
            }
            free(inner);
            cursor = close + 1;
        } else if (*cursor == '-' || isdigit((unsigned char)*cursor)) {
            const char *start = cursor;
            while (*cursor == '-' || *cursor == ',' || isdigit((unsigned char)*cursor)) {
                cursor += 1;
            }
            char *token = dup_range(start, (size_t)(cursor - start));
            if (token == NULL) {
                goto done;
            }
            bool wrapped;
            if (!canonical_tuple(token, tuple, sizeof tuple, &wrapped)) {
                set_error(err, BT_NORMALIZE_ERR_UNKNOWN_TOKEN, trimmed, token);
                free(token);
                goto done;
            }
            if (wrapped && notes_push(out, "wraparound: (%s) -> %s", token, tuple) != 0) {
                free(token);
                goto done;
            }
            free(token);
        } else {
            const char *start = cursor;
            while (*cursor != '\0' && !isspace((unsigned char)*cursor) && *cursor != '/' &&
                   *cursor != '(') {
                cursor += 1;
            }
            char *token = dup_range(start, (size_t)(cursor - start));
            if (token != NULL) {
                set_error(err, BT_NORMALIZE_ERR_UNKNOWN_TOKEN, trimmed, token);
                free(token);
            }
            goto done;
        }
        if (part_count == part_cap) {
            part_cap *= 2;
            char **grown = realloc(parts, part_cap * sizeof *grown);
            if (grown == NULL) {
                goto done;
            }
            parts = grown;
        }
        parts[part_count] = malloc(strlen(tuple) + 1);
        if (parts[part_count] == NULL) {
            goto done;
        }
        strcpy(parts[part_count], tuple);
        part_count += 1;
    }
    joined = dup_range("", 0);
    if (joined == NULL) {
        goto done;
    }
    for (size_t i = 0; i < part_count; i++) {
        if (str_assign(&joined, "%s%s%s", joined, i == 0 ? "" : " ", parts[i]) != 0) {
            goto done;
        }
    }
    out->alg = joined;
    joined = NULL;
    ok = true;
done:
    free(trimmed);
    free(current);
    for (size_t i = 0; i < part_count; i++) {
        free(parts[i]);
    }
    free(parts);
    free(joined);
    return ok;
}

bool bt_seedpack_normalize_alg(const char *event, const char *raw, bt_normalized *out,
                               bt_normalize_error *err) {
    BT_TRACE_ENTER("normalizer.normalize_alg");
    out->alg = NULL;
    out->notes = NULL;
    out->note_count = 0;
    bool ok;
    if (strcmp(event, BT_SEEDPACK_SQ1_EVENT) == 0) {
        ok = normalize_sq1(raw, out, err);
    } else {
        ok = normalize_cube(raw, out, err);
    }
    if (!ok) {
        bt_normalized_free(out);
        BT_TRACE_FAIL("normalizer.normalize_alg", "normalize failed for %s", raw);
        return false;
    }
    BT_TRACE_EXIT("normalizer.normalize_alg");
    return true;
}
