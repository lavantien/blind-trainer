#include "common/bt_harness.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "policy/lexer.h"
#include "policy/lib.h"

static void mask_c(const char *text, policy_masked *out) {
    policy_mask_source(text, strlen(text), POLICY_LANG_C, out);
}

static void mask_py(const char *text, policy_masked *out) {
    policy_mask_source(text, strlen(text), POLICY_LANG_PY, out);
}

static void span_is(const char *input, size_t start, size_t end, const char *want) {
    BT_CHECK_EQ(end - start, strlen(want));
    BT_CHECK(memcmp(input + start, want, end - start) == 0);
}

static void spaces_between(const char *head, int count, const char *tail, const char *actual) {
    char want[512];
    snprintf(want, sizeof want, "%s%*s%s", head, count, "", tail);
    BT_CHECK(strcmp(actual, want) == 0);
}

BT_TEST(policy_lexer, line_comment_is_masked_and_reported_with_exact_span) {
    const char *input = "int x = 1; // note\nint y = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    spaces_between("int x = 1; ", 7, "\nint y = 2;", masked.text);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_LINE);
    span_is(input, masked.comments[0].start, masked.comments[0].end, "// note");
    BT_CHECK_EQ(policy_line_of(input, masked.comments[0].start), 1);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, line_comment_at_eof_covers_the_tail) {
    const char *input = "int x = 1; // tail";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 1);
    span_is(input, masked.comments[0].start, masked.comments[0].end, "// tail");
    BT_CHECK_EQ(masked.comments[0].end, strlen(input));
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, hash_comments_mark_python_lines) {
    const char *input = "x = 1\n# hash note\ny = 2";
    policy_masked masked;
    mask_py(input, &masked);
    spaces_between("x = 1\n", 11, "\ny = 2", masked.text);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_HASH);
    span_is(input, masked.comments[0].start, masked.comments[0].end, "# hash note");
    BT_CHECK_EQ(policy_line_of(input, masked.comments[0].start), 2);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, block_comment_ends_at_the_first_close_in_c) {
    const char *input = "int a = 1; /* outer /* inner */ still */ int b = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_BLOCK);
    span_is(input, masked.comments[0].start, masked.comments[0].end, "/* outer /* inner */");
    spaces_between("int a = 1; ", (int)(masked.comments[0].end - masked.comments[0].start),
                   " still */ int b = 2;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, urls_inside_strings_are_not_comments) {
    const char *input = "const char *s = \"http://example.org/x\"; int t = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    spaces_between("const char *s = \"", 20, "\"; int t = 2;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, escaped_quote_keeps_the_string_open) {
    const char *input = "const char *s = \"a\\\"b//c\"; int t = 1;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    spaces_between("const char *s = \"", 7, "\"; int t = 1;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, python_triple_quoted_strings_span_newlines) {
    const char *input = "x = \"\"\"line1\nwith // and /* and # inside\"\"\"\ny = 3";
    policy_masked masked;
    mask_py(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK_EQ(strlen(masked.text), strlen(input));
    size_t head = 7;
    size_t tail = 9;
    BT_CHECK(memcmp(masked.text, "x = \"\"\"", head) == 0);
    BT_CHECK(memcmp(masked.text + strlen(masked.text) - tail, "\"\"\"\ny = 3", tail) == 0);
    for (size_t at = head; at < strlen(masked.text) - tail; at++) {
        BT_CHECK_EQ(masked.text[at], ' ');
    }
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, python_raw_string_closes_at_the_next_quote) {
    const char *input = "x = r\"a // b\"; t = 8";
    policy_masked masked;
    mask_py(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "x = r\"      \"; t = 8") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, python_prefixed_bytes_string_is_masked) {
    const char *input = "x = b\"a//b\"; t = 4";
    policy_masked masked;
    mask_py(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "x = b\"    \"; t = 4") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, python_prefixed_raw_string_is_masked) {
    const char *input = "x = rb\"x/*y\"; t = 5";
    policy_masked masked;
    mask_py(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "x = rb\"    \"; t = 5") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, char_literals_of_every_shape_are_masked) {
    const char *input = "char a = 'x'; char b = '\\n'; char c = '\\''; char d = '\\x41'; char e = "
                        "'\\101';";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "char a = ' '; char b = '  '; char c = '  '; char d = '    '; "
                                 "char e = '    ';") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, prefixed_char_and_string_literals_are_masked) {
    const char *input = "wchar_t w = L'z'; char16_t u = u'x'; const char *s = u8\"a//b\";";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "wchar_t w = L' '; char16_t u = u' '; const char *s = "
                                 "u8\"    \";") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, digit_separators_stay_code) {
    const char *input = "int n = 1'000'000;\nint m = 0x1'f;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, input) == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, masking_preserves_byte_length_with_multibyte_content) {
    const char *input = "const char *s = \"h\xc3\xa9llo // w\xc3\xb6rld\"; int t = 7;";
    const char *head = "const char *s = \"";
    const char *tail = "\"; int t = 7;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(strlen(masked.text), strlen(input));
    int content_len = (int)(strlen(input) - strlen(head) - strlen(tail));
    spaces_between(head, content_len, tail, masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, line_comment_continues_across_a_backslash_newline_splice) {
    const char *input = "int x = 1; // tail \\\nstill comment\nint y = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_LINE);
    BT_CHECK_EQ(policy_line_of(input, masked.comments[0].start), 1);
    BT_CHECK(input[masked.comments[0].start] == '/' && input[masked.comments[0].start + 1] == '/');
    spaces_between("int x = 1; ", (int)(masked.comments[0].end - masked.comments[0].start),
                   "\nint y = 2;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, block_comment_opener_spans_a_splice) {
    const char *input = "int a = 1; /\\\n* block */ int b = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_BLOCK);
    BT_CHECK_EQ(policy_line_of(input, masked.comments[0].start), 1);
    spaces_between("int a = 1; ", (int)(masked.comments[0].end - masked.comments[0].start),
                   " int b = 2;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, block_comment_close_spans_a_splice) {
    const char *input = "int a = 1; /* block *\\\n/ int b = 2;";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 1);
    BT_CHECK_EQ(masked.comments[0].kind, POLICY_COMMENT_BLOCK);
    spaces_between("int a = 1; ", (int)(masked.comments[0].end - masked.comments[0].start),
                   " int b = 2;", masked.text);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, universal_char_escapes_are_masked) {
    const char *input = "char a = '\\u0041'; char b = '\\U0001F600';";
    policy_masked masked;
    mask_c(input, &masked);
    BT_CHECK_EQ(masked.comment_count, 0);
    BT_CHECK(strcmp(masked.text, "char a = '      '; char b = '          ';") == 0);
    policy_masked_free(&masked);
}

BT_TEST(policy_lexer, line_of_counts_newlines_before_the_offset) {
    const char *text = "ab\ncd\nef";
    BT_CHECK_EQ(policy_line_of(text, 0), 1);
    BT_CHECK_EQ(policy_line_of(text, 3), 2);
    BT_CHECK_EQ(policy_line_of(text, 6), 3);
}

BT_TEST(policy_lexer, pseudo_fuzz_over_adversarial_alphabet_never_panics) {
    static const char alphabet[] = "\"'\\/br#xufz0189ab{} \n\t$.*L";
    uint64_t state = 0x2545F4914F6CDD1DULL;
    for (int round = 0; round < 2000; round++) {
        size_t len = (size_t)(state % 160);
        char text[192];
        for (size_t at = 0; at < len; at++) {
            state ^= state << 13U;
            state ^= state >> 7U;
            state ^= state << 17U;
            text[at] = alphabet[state % (sizeof alphabet - 1)];
        }
        text[len] = '\0';
        policy_masked c_masked;
        policy_masked py_masked;
        policy_mask_source(text, len, POLICY_LANG_C, &c_masked);
        policy_mask_source(text, len, POLICY_LANG_PY, &py_masked);
        BT_CHECK_EQ(strlen(c_masked.text), len);
        BT_CHECK_EQ(strlen(py_masked.text), len);
        policy_masked_free(&c_masked);
        policy_masked_free(&py_masked);
    }
}
