#include "common/json.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

typedef struct {
    const char *cursor;
    const char *end;
    size_t line;
    size_t column;
    bt_json_error *error;
    int failed;
} bt_json_reader;

static void reader_advance(bt_json_reader *reader) {
    if (*reader->cursor == '\n') {
        reader->line++;
        reader->column = 1;
    } else {
        reader->column++;
    }
    reader->cursor++;
}

static void fail_at(bt_json_reader *reader, size_t line, size_t column, const char *fmt, ...) {
    if (reader->failed) {
        return;
    }
    reader->failed = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(reader->error->message, sizeof reader->error->message, fmt, ap);
    va_end(ap);
    reader->error->line = line;
    reader->error->column = column;
}

static void fail_here(bt_json_reader *reader, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char message[96];
    vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    fail_at(reader, reader->line, reader->column, "%s", message);
}

static void skip_space(bt_json_reader *reader) {
    while (reader->cursor < reader->end && (*reader->cursor == ' ' || *reader->cursor == '\t' ||
                                            *reader->cursor == '\n' || *reader->cursor == '\r')) {
        reader_advance(reader);
    }
}

static bt_json_value *parse_value(bt_json_reader *reader, int depth);

static bt_json_value *value_new(bt_json_type type) {
    bt_json_value *value = calloc(1, sizeof(bt_json_value));
    if (value != NULL) {
        value->type = type;
    }
    return value;
}

static char *text_dup(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

bt_json_value *bt_json_new_null(void) {
    BT_TRACE_ENTER("json.new_null");
    bt_json_value *value = value_new(BT_JSON_NULL);
    BT_TRACE_EXIT("json.new_null");
    return value;
}

bt_json_value *bt_json_new_bool(int value) {
    BT_TRACE_ENTER("json.new_bool");
    bt_json_value *fresh = value_new(BT_JSON_BOOL);
    if (fresh != NULL) {
        fresh->as.boolean = value ? 1 : 0;
    }
    BT_TRACE_EXIT("json.new_bool");
    return fresh;
}

bt_json_value *bt_json_new_number(double value) {
    BT_TRACE_ENTER("json.new_number");
    bt_json_value *fresh = value_new(BT_JSON_NUMBER);
    if (fresh != NULL) {
        fresh->as.number = value;
    }
    BT_TRACE_EXIT("json.new_number");
    return fresh;
}

bt_json_value *bt_json_new_integer(long long value) {
    BT_TRACE_ENTER("json.new_integer");
    bt_json_value *fresh = value_new(BT_JSON_NUMBER);
    if (fresh != NULL) {
        fresh->is_integer = 1;
        fresh->integer = value;
        fresh->as.number = (double)value;
    }
    BT_TRACE_EXIT("json.new_integer");
    return fresh;
}

int bt_json_is_integer(const bt_json_value *value) {
    return value != NULL && value->type == BT_JSON_NUMBER && value->is_integer;
}

long long bt_json_integer_or(const bt_json_value *value, long long fallback) {
    if (bt_json_is_integer(value)) {
        return value->integer;
    }
    if (value != NULL && value->type == BT_JSON_NUMBER) {
        return (long long)value->as.number;
    }
    return fallback;
}

bt_json_value *bt_json_new_string(const char *value) {
    BT_TRACE_ENTER("json.new_string");
    bt_json_value *fresh = value_new(BT_JSON_STRING);
    if (fresh == NULL) {
        BT_TRACE_EXIT("json.new_string");
        return NULL;
    }
    fresh->as.string = text_dup(value);
    if (fresh->as.string == NULL) {
        free(fresh);
        BT_TRACE_FAIL("json.new_string", "strdup failed");
        BT_TRACE_EXIT("json.new_string");
        return NULL;
    }
    BT_TRACE_EXIT("json.new_string");
    return fresh;
}

bt_json_value *bt_json_new_array(void) {
    BT_TRACE_ENTER("json.new_array");
    bt_json_value *value = value_new(BT_JSON_ARRAY);
    BT_TRACE_EXIT("json.new_array");
    return value;
}

bt_json_value *bt_json_new_object(void) {
    BT_TRACE_ENTER("json.new_object");
    bt_json_value *value = value_new(BT_JSON_OBJECT);
    BT_TRACE_EXIT("json.new_object");
    return value;
}

void bt_json_free(bt_json_value *value) {
    if (value == NULL) {
        return;
    }
    BT_TRACE_ENTER("json.free");
    switch (value->type) {
    case BT_JSON_STRING:
        free(value->as.string);
        break;
    case BT_JSON_ARRAY:
        for (size_t i = 0; i < value->as.array.count; i++) {
            bt_json_free(value->as.array.items[i]);
        }
        free(value->as.array.items);
        break;
    case BT_JSON_OBJECT:
        for (size_t i = 0; i < value->as.object.count; i++) {
            free(value->as.object.keys[i]);
            bt_json_free(value->as.object.values[i]);
        }
        free(value->as.object.keys);
        free(value->as.object.values);
        break;
    default:
        break;
    }
    free(value);
    BT_TRACE_EXIT("json.free");
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int read_hex4(bt_json_reader *reader, unsigned *out) {
    unsigned value = 0;
    for (int i = 0; i < 4; i++) {
        if (reader->cursor >= reader->end) {
            fail_here(reader, "unexpected end in \\u escape");
            return -1;
        }
        int digit = hex_value(*reader->cursor);
        if (digit < 0) {
            fail_here(reader, "invalid hex digit in \\u escape");
            return -1;
        }
        value = value * 16U + (unsigned)digit;
        reader_advance(reader);
    }
    *out = value;
    return 0;
}

static int utf8_valid(const unsigned char *bytes, size_t len, size_t *advance) {
    if (len == 0) {
        return 0;
    }
    unsigned char lead = bytes[0];
    if (lead < 0x80) {
        *advance = 1;
        return 1;
    }
    size_t need = 0;
    unsigned codepoint = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
        need = 1;
        codepoint = lead & 0x1FU;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        need = 2;
        codepoint = lead & 0x0FU;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        need = 3;
        codepoint = lead & 0x07U;
    } else {
        return 0;
    }
    if (len < need + 1) {
        return 0;
    }
    for (size_t i = 1; i <= need; i++) {
        unsigned char cont = bytes[i];
        if (cont < 0x80 || cont > 0xBF) {
            return 0;
        }
        codepoint = (codepoint << 6U) + (cont & 0x3FU);
    }
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        return 0;
    }
    if (codepoint > 0x10FFFF) {
        return 0;
    }
    if (need == 2 && codepoint < 0x800) {
        return 0;
    }
    if (need == 3 && codepoint < 0x10000) {
        return 0;
    }
    *advance = need + 1;
    return 1;
}

static int append_utf8(bt_strbuf *sb, unsigned codepoint) {
    char encoded[4];
    size_t len = 0;
    if (codepoint < 0x80) {
        encoded[len++] = (char)codepoint;
    } else if (codepoint < 0x800) {
        encoded[len++] = (char)(0xC0U | (codepoint >> 6U));
        encoded[len++] = (char)(0x80U | (codepoint & 0x3FU));
    } else if (codepoint < 0x10000) {
        encoded[len++] = (char)(0xE0U | (codepoint >> 12U));
        encoded[len++] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[len++] = (char)(0x80U | (codepoint & 0x3FU));
    } else {
        encoded[len++] = (char)(0xF0U | (codepoint >> 18U));
        encoded[len++] = (char)(0x80U | ((codepoint >> 12U) & 0x3FU));
        encoded[len++] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        encoded[len++] = (char)(0x80U | (codepoint & 0x3FU));
    }
    return bt_strbuf_append(sb, encoded, len);
}

static char simple_escape(char esc, int *has) {
    *has = 1;
    switch (esc) {
    case '"':
        return '"';
    case '\\':
        return '\\';
    case '/':
        return '/';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    default:
        *has = 0;
        return 0;
    }
}

static int parse_escaped_codepoint(bt_json_reader *reader, bt_strbuf *sb) {
    reader_advance(reader);
    unsigned codepoint = 0;
    if (read_hex4(reader, &codepoint) != 0) {
        return -1;
    }
    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
        if (reader->end - reader->cursor < 2 || reader->cursor[0] != '\\' ||
            reader->cursor[1] != 'u') {
            fail_here(reader, "lone surrogate");
            return -1;
        }
        reader_advance(reader);
        reader_advance(reader);
        unsigned low = 0;
        if (read_hex4(reader, &low) != 0) {
            return -1;
        }
        if (low < 0xDC00 || low > 0xDFFF) {
            fail_here(reader, "invalid surrogate pair");
            return -1;
        }
        codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);
    } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
        fail_here(reader, "lone surrogate");
        return -1;
    }
    return append_utf8(sb, codepoint);
}

static int parse_escape(bt_json_reader *reader, bt_strbuf *sb) {
    reader_advance(reader);
    if (reader->cursor >= reader->end) {
        fail_here(reader, "unterminated escape");
        return -1;
    }
    char esc = *reader->cursor;
    int has_simple = 0;
    char simple = simple_escape(esc, &has_simple);
    if (has_simple) {
        reader_advance(reader);
        return bt_strbuf_append(sb, &simple, 1);
    }
    if (esc != 'u') {
        fail_here(reader, "invalid escape");
        return -1;
    }
    return parse_escaped_codepoint(reader, sb);
}

static int parse_string(bt_json_reader *reader, char **out) {
    size_t line = reader->line;
    size_t column = reader->column;
    reader_advance(reader);
    bt_strbuf sb;
    if (bt_strbuf_init(&sb, 32) != 0) {
        fail_at(reader, line, column, "string allocation failed");
        return -1;
    }
    while (1) {
        if (reader->cursor >= reader->end) {
            bt_strbuf_free(&sb);
            fail_here(reader, "unterminated string");
            return -1;
        }
        unsigned char c = (unsigned char)*reader->cursor;
        if (c == '"') {
            reader_advance(reader);
            if (bt_strbuf_append(&sb, "", 0) != 0) {
                bt_strbuf_free(&sb);
                fail_at(reader, line, column, "string allocation failed");
                return -1;
            }
            *out = sb.data;
            return 0;
        }
        if (c == '\\') {
            if (parse_escape(reader, &sb) != 0) {
                bt_strbuf_free(&sb);
                return -1;
            }
            continue;
        }
        if (c < 0x20) {
            bt_strbuf_free(&sb);
            fail_here(reader, "control character in string");
            return -1;
        }
        size_t advance = 0;
        if (!utf8_valid((const unsigned char *)reader->cursor,
                        (size_t)(reader->end - reader->cursor), &advance)) {
            bt_strbuf_free(&sb);
            fail_here(reader, "invalid utf-8 in string");
            return -1;
        }
        if (bt_strbuf_append(&sb, reader->cursor, advance) != 0) {
            bt_strbuf_free(&sb);
            fail_at(reader, line, column, "string allocation failed");
            return -1;
        }
        for (size_t i = 0; i < advance; i++) {
            reader_advance(reader);
        }
    }
}

static int digit_at(bt_json_reader *reader) {
    return reader->cursor < reader->end && *reader->cursor >= '0' && *reader->cursor <= '9';
}

static void scan_digits(bt_json_reader *reader, bt_strbuf *sb) {
    while (digit_at(reader)) {
        bt_strbuf_append(sb, reader->cursor, 1);
        reader_advance(reader);
    }
}

static int parse_int_part(bt_json_reader *reader, bt_strbuf *sb, size_t start_line,
                          size_t start_column) {
    if (reader->cursor < reader->end && *reader->cursor == '-') {
        bt_strbuf_append(sb, "-", 1);
        reader_advance(reader);
    }
    if (!digit_at(reader)) {
        fail_at(reader, start_line, start_column, "invalid number: digit expected");
        return -1;
    }
    if (*reader->cursor == '0') {
        bt_strbuf_append(sb, "0", 1);
        reader_advance(reader);
        if (reader->cursor < reader->end && *reader->cursor >= '1' && *reader->cursor <= '9') {
            fail_here(reader, "invalid number: leading zero");
            return -1;
        }
        return 0;
    }
    scan_digits(reader, sb);
    return 0;
}

static int parse_fraction(bt_json_reader *reader, bt_strbuf *sb) {
    if (reader->cursor >= reader->end || *reader->cursor != '.') {
        return 0;
    }
    bt_strbuf_append(sb, ".", 1);
    reader_advance(reader);
    if (!digit_at(reader)) {
        fail_here(reader, "invalid number: digit after point expected");
        return -1;
    }
    scan_digits(reader, sb);
    return 0;
}

static int parse_exponent(bt_json_reader *reader, bt_strbuf *sb) {
    if (reader->cursor >= reader->end || (*reader->cursor != 'e' && *reader->cursor != 'E')) {
        return 0;
    }
    bt_strbuf_append(sb, reader->cursor, 1);
    reader_advance(reader);
    if (reader->cursor < reader->end && (*reader->cursor == '+' || *reader->cursor == '-')) {
        bt_strbuf_append(sb, reader->cursor, 1);
        reader_advance(reader);
    }
    if (!digit_at(reader)) {
        fail_here(reader, "invalid number: exponent digit expected");
        return -1;
    }
    scan_digits(reader, sb);
    return 0;
}

static int parse_number(bt_json_reader *reader, double *out, int *is_integer, long long *integer) {
    size_t line = reader->line;
    size_t column = reader->column;
    bt_strbuf sb;
    if (bt_strbuf_init(&sb, 32) != 0) {
        return -1;
    }
    int rc = parse_int_part(reader, &sb, line, column);
    if (rc == 0) {
        rc = parse_fraction(reader, &sb);
    }
    if (rc == 0) {
        rc = parse_exponent(reader, &sb);
    }
    if (rc != 0) {
        bt_strbuf_free(&sb);
        return -1;
    }
    char *end_ptr = NULL;
    double value = strtod(sb.data, &end_ptr);
    int exact = 0;
    long long exact_value = 0;
    if (strpbrk(sb.data, ".eE") == NULL && strcmp(sb.data, "-0") != 0) {
        errno = 0;
        long long parsed = strtoll(sb.data, &end_ptr, 10);
        if (errno == 0 && end_ptr && *end_ptr == '\0') {
            exact = 1;
            exact_value = parsed;
        }
    }
    bt_strbuf_free(&sb);
    if (!isfinite(value)) {
        fail_here(reader, "number out of range");
        return -1;
    }
    *out = value;
    *is_integer = exact;
    *integer = exact_value;
    return 0;
}

static int literal(bt_json_reader *reader, const char *word) {
    size_t len = strlen(word);
    if ((size_t)(reader->end - reader->cursor) < len || memcmp(reader->cursor, word, len) != 0) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        reader_advance(reader);
    }
    return 1;
}

static bt_json_value *parse_object(bt_json_reader *reader, int depth) {
    size_t line = reader->line;
    size_t column = reader->column;
    reader_advance(reader);
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        fail_at(reader, line, column, "object allocation failed");
        return NULL;
    }
    skip_space(reader);
    if (reader->cursor < reader->end && *reader->cursor == '}') {
        reader_advance(reader);
        return object;
    }
    while (1) {
        skip_space(reader);
        if (reader->cursor >= reader->end || *reader->cursor != '"') {
            bt_json_free(object);
            fail_here(reader, "object key expected");
            return NULL;
        }
        char *key = NULL;
        if (parse_string(reader, &key) != 0) {
            bt_json_free(object);
            return NULL;
        }
        for (size_t i = 0; i < object->as.object.count; i++) {
            if (strcmp(object->as.object.keys[i], key) == 0) {
                free(key);
                bt_json_free(object);
                fail_here(reader, "duplicate object key");
                return NULL;
            }
        }
        skip_space(reader);
        if (reader->cursor >= reader->end || *reader->cursor != ':') {
            free(key);
            bt_json_free(object);
            fail_here(reader, "expected ':' after object key");
            return NULL;
        }
        reader_advance(reader);
        skip_space(reader);
        bt_json_value *value = parse_value(reader, depth + 1);
        if (value == NULL) {
            free(key);
            bt_json_free(object);
            return NULL;
        }
        size_t count = object->as.object.count;
        char **keys = (char **)realloc(object->as.object.keys, (count + 1) * sizeof(char *));
        bt_json_value **values = (bt_json_value **)realloc(object->as.object.values,
                                                           (count + 1) * sizeof(bt_json_value *));
        if (keys == NULL || values == NULL) {
            free(key);
            bt_json_free(value);
            bt_json_free(object);
            fail_here(reader, "object growth failed");
            return NULL;
        }
        object->as.object.keys = keys;
        object->as.object.values = values;
        object->as.object.keys[count] = key;
        object->as.object.values[count] = value;
        object->as.object.count = count + 1;
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == ',') {
            reader_advance(reader);
            continue;
        }
        if (reader->cursor < reader->end && *reader->cursor == '}') {
            reader_advance(reader);
            return object;
        }
        bt_json_free(object);
        if (reader->cursor >= reader->end) {
            fail_here(reader, "unexpected end inside object");
        } else {
            fail_here(reader, "expected ',' or '}' in object");
        }
        return NULL;
    }
}

static bt_json_value *parse_array(bt_json_reader *reader, int depth) {
    size_t line = reader->line;
    size_t column = reader->column;
    reader_advance(reader);
    bt_json_value *array = bt_json_new_array();
    if (array == NULL) {
        fail_at(reader, line, column, "array allocation failed");
        return NULL;
    }
    skip_space(reader);
    if (reader->cursor < reader->end && *reader->cursor == ']') {
        reader_advance(reader);
        return array;
    }
    while (1) {
        skip_space(reader);
        bt_json_value *item = parse_value(reader, depth + 1);
        if (item == NULL) {
            bt_json_free(array);
            return NULL;
        }
        size_t count = array->as.array.count;
        bt_json_value **items =
            (bt_json_value **)realloc(array->as.array.items, (count + 1) * sizeof(bt_json_value *));
        if (items == NULL) {
            bt_json_free(item);
            bt_json_free(array);
            fail_here(reader, "array growth failed");
            return NULL;
        }
        array->as.array.items = items;
        array->as.array.items[count] = item;
        array->as.array.count = count + 1;
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == ',') {
            reader_advance(reader);
            continue;
        }
        if (reader->cursor < reader->end && *reader->cursor == ']') {
            reader_advance(reader);
            return array;
        }
        bt_json_free(array);
        if (reader->cursor >= reader->end) {
            fail_here(reader, "unexpected end inside array");
        } else {
            fail_here(reader, "expected ',' or ']' in array");
        }
        return NULL;
    }
}

static bt_json_value *parse_value(bt_json_reader *reader, int depth) {
    if (depth > BT_JSON_MAX_DEPTH) {
        fail_here(reader, "nesting depth exceeded");
        return NULL;
    }
    if (reader->cursor >= reader->end) {
        fail_here(reader, "unexpected end of input, value expected");
        return NULL;
    }
    char c = *reader->cursor;
    if (c == '{') {
        return parse_object(reader, depth);
    }
    if (c == '[') {
        return parse_array(reader, depth);
    }
    if (c == '"') {
        char *text = NULL;
        if (parse_string(reader, &text) != 0) {
            return NULL;
        }
        bt_json_value *value = bt_json_new_string(text);
        free(text);
        return value;
    }
    if (c == 't') {
        if (literal(reader, "true")) {
            return bt_json_new_bool(1);
        }
        fail_here(reader, "invalid literal, value expected");
        return NULL;
    }
    if (c == 'f') {
        if (literal(reader, "false")) {
            return bt_json_new_bool(0);
        }
        fail_here(reader, "invalid literal, value expected");
        return NULL;
    }
    if (c == 'n') {
        if (literal(reader, "null")) {
            return bt_json_new_null();
        }
        fail_here(reader, "invalid literal, value expected");
        return NULL;
    }
    if (c != '-' && (c < '0' || c > '9')) {
        fail_here(reader, "value expected");
        return NULL;
    }
    double number = 0;
    int is_integer = 0;
    long long integer = 0;
    if (parse_number(reader, &number, &is_integer, &integer) == 0) {
        if (is_integer) {
            return bt_json_new_integer(integer);
        }
        return bt_json_new_number(number);
    }
    if (!reader->failed) {
        fail_here(reader, "value expected");
    }
    return NULL;
}

int bt_json_parse(const char *text, size_t len, bt_json_value **out, bt_json_error *error) {
    BT_TRACE_ENTER("json.parse");
    *out = NULL;
    error->line = 1;
    error->column = 1;
    error->message[0] = '\0';
    bt_json_reader reader = {
        .cursor = text,
        .end = text + len,
        .line = 1,
        .column = 1,
        .error = error,
        .failed = 0,
    };
    skip_space(&reader);
    bt_json_value *value = parse_value(&reader, 1);
    if (value == NULL) {
        BT_TRACE_FAIL("json.parse", "parse failed at %zu:%zu: %s", error->line, error->column,
                      error->message);
        BT_TRACE_EXIT("json.parse");
        return -1;
    }
    skip_space(&reader);
    if (reader.cursor != reader.end) {
        bt_json_free(value);
        fail_here(&reader, "trailing content after value");
        BT_TRACE_FAIL("json.parse", "trailing content at %zu:%zu", error->line, error->column);
        BT_TRACE_EXIT("json.parse");
        return -1;
    }
    *out = value;
    BT_TRACE_EXIT("json.parse");
    return 0;
}

int bt_json_array_push(bt_json_value *array, bt_json_value *item) {
    BT_TRACE_ENTER("json.array_push");
    if (array == NULL || array->type != BT_JSON_ARRAY || item == NULL) {
        BT_TRACE_FAIL("json.array_push", "invalid arguments");
        BT_TRACE_EXIT("json.array_push");
        return -1;
    }
    size_t count = array->as.array.count;
    bt_json_value **items =
        (bt_json_value **)realloc(array->as.array.items, (count + 1) * sizeof(bt_json_value *));
    if (items == NULL) {
        BT_TRACE_FAIL("json.array_push", "growth failed");
        BT_TRACE_EXIT("json.array_push");
        return -1;
    }
    array->as.array.items = items;
    array->as.array.items[count] = item;
    array->as.array.count = count + 1;
    BT_TRACE_EXIT("json.array_push");
    return 0;
}

int bt_json_object_set(bt_json_value *object, const char *key, bt_json_value *value) {
    BT_TRACE_ENTER("json.object_set");
    if (object == NULL || object->type != BT_JSON_OBJECT || key == NULL || value == NULL) {
        BT_TRACE_FAIL("json.object_set", "invalid arguments");
        BT_TRACE_EXIT("json.object_set");
        return -1;
    }
    for (size_t i = 0; i < object->as.object.count; i++) {
        if (strcmp(object->as.object.keys[i], key) == 0) {
            bt_json_free(object->as.object.values[i]);
            object->as.object.values[i] = value;
            BT_TRACE_EXIT("json.object_set");
            return 0;
        }
    }
    size_t count = object->as.object.count;
    char **keys = (char **)realloc(object->as.object.keys, (count + 1) * sizeof(char *));
    bt_json_value **values =
        (bt_json_value **)realloc(object->as.object.values, (count + 1) * sizeof(bt_json_value *));
    if (keys == NULL || values == NULL) {
        BT_TRACE_FAIL("json.object_set", "growth failed");
        BT_TRACE_EXIT("json.object_set");
        return -1;
    }
    keys[count] = text_dup(key);
    if (keys[count] == NULL) {
        BT_TRACE_FAIL("json.object_set", "key copy failed");
        BT_TRACE_EXIT("json.object_set");
        return -1;
    }
    object->as.object.keys = keys;
    object->as.object.values = values;
    object->as.object.values[count] = value;
    object->as.object.count = count + 1;
    BT_TRACE_EXIT("json.object_set");
    return 0;
}

bt_json_value *bt_json_get(const bt_json_value *object, const char *key) {
    if (object == NULL || object->type != BT_JSON_OBJECT || key == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < object->as.object.count; i++) {
        if (strcmp(object->as.object.keys[i], key) == 0) {
            return object->as.object.values[i];
        }
    }
    return NULL;
}

bt_json_value *bt_json_at(const bt_json_value *array, size_t index) {
    if (array == NULL || array->type != BT_JSON_ARRAY || index >= array->as.array.count) {
        return NULL;
    }
    return array->as.array.items[index];
}

static int serialize_string(const char *text, bt_strbuf *out) {
    if (bt_strbuf_append(out, "\"", 1) != 0) {
        return -1;
    }
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
        unsigned char c = *cursor;
        const char *escape = NULL;
        switch (c) {
        case '"':
            escape = "\\\"";
            break;
        case '\\':
            escape = "\\\\";
            break;
        case '\b':
            escape = "\\b";
            break;
        case '\f':
            escape = "\\f";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            escape = NULL;
            break;
        }
        if (escape != NULL) {
            if (bt_strbuf_append_str(out, escape) != 0) {
                return -1;
            }
            continue;
        }
        if (c < 0x20) {
            char buffer[7];
            snprintf(buffer, sizeof buffer, "\\u%04x", (unsigned)c);
            if (bt_strbuf_append_str(out, buffer) != 0) {
                return -1;
            }
            continue;
        }
        if (bt_strbuf_append(out, (const char *)cursor, 1) != 0) {
            return -1;
        }
    }
    if (bt_strbuf_append(out, "\"", 1) != 0) {
        return -1;
    }
    return 0;
}

static int serialize_number(double value, bt_strbuf *out) {
    if (!isfinite(value)) {
        return -1;
    }
    char buffer[64];
    for (int precision = 1; precision <= 17; precision++) {
        snprintf(buffer, sizeof buffer, "%.*g", precision, value);
        if (strtod(buffer, NULL) == value) {
            break;
        }
    }
    return bt_strbuf_append_str(out, buffer);
}

typedef struct {
    size_t index;
    const char *key;
} bt_json_key_slot;

static int compare_key_slots(const void *left, const void *right) {
    const bt_json_key_slot *a = left;
    const bt_json_key_slot *b = right;
    unsigned char ca = (unsigned char)a->key[0];
    unsigned char cb = (unsigned char)b->key[0];
    size_t i = 0;
    while (ca != '\0' && cb != '\0' && ca == cb) {
        i++;
        ca = (unsigned char)a->key[i];
        cb = (unsigned char)b->key[i];
    }
    return (int)ca - (int)cb;
}

static int serialize_value(const bt_json_value *value, bt_strbuf *out) {
    if (value == NULL) {
        return -1;
    }
    switch (value->type) {
    case BT_JSON_NULL:
        return bt_strbuf_append_str(out, "null");
    case BT_JSON_BOOL:
        return bt_strbuf_append_str(out, value->as.boolean ? "true" : "false");
    case BT_JSON_NUMBER:
        if (value->is_integer) {
            char buffer[32];
            snprintf(buffer, sizeof buffer, "%lld", value->integer);
            return bt_strbuf_append_str(out, buffer);
        }
        return serialize_number(value->as.number, out);
    case BT_JSON_STRING:
        return serialize_string(value->as.string, out);
    case BT_JSON_ARRAY:
        if (bt_strbuf_append(out, "[", 1) != 0) {
            return -1;
        }
        for (size_t i = 0; i < value->as.array.count; i++) {
            if (i > 0 && bt_strbuf_append(out, ",", 1) != 0) {
                return -1;
            }
            if (serialize_value(value->as.array.items[i], out) != 0) {
                return -1;
            }
        }
        return bt_strbuf_append(out, "]", 1);
    case BT_JSON_OBJECT:
        if (bt_strbuf_append(out, "{", 1) != 0) {
            return -1;
        }
        if (value->as.object.count > 0) {
            bt_json_key_slot *slots = malloc(value->as.object.count * sizeof(bt_json_key_slot));
            if (slots == NULL) {
                return -1;
            }
            for (size_t i = 0; i < value->as.object.count; i++) {
                slots[i].index = i;
                slots[i].key = value->as.object.keys[i];
            }
            qsort(slots, value->as.object.count, sizeof(bt_json_key_slot), compare_key_slots);
            for (size_t i = 0; i < value->as.object.count; i++) {
                if (i > 0 && bt_strbuf_append(out, ",", 1) != 0) {
                    free(slots);
                    return -1;
                }
                size_t index = slots[i].index;
                if (serialize_string(value->as.object.keys[index], out) != 0 ||
                    bt_strbuf_append(out, ":", 1) != 0 ||
                    serialize_value(value->as.object.values[index], out) != 0) {
                    free(slots);
                    return -1;
                }
            }
            free(slots);
        }
        return bt_strbuf_append(out, "}", 1);
    }
    return -1;
}

int bt_json_serialize(const bt_json_value *value, bt_strbuf *out) {
    BT_TRACE_ENTER("json.serialize");
    int rc = serialize_value(value, out);
    if (rc != 0) {
        BT_TRACE_FAIL("json.serialize", "serialization failed");
    }
    BT_TRACE_EXIT("json.serialize");
    return rc;
}
