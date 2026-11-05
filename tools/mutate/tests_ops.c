#include "mutate/mutate.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"

static long find_token(const bt_mut_tokens *tokens, const char *text) {
    for (size_t i = 0; i < tokens->count; i++) {
        const char *candidate = bt_mut_token_text(tokens, &tokens->items[i]);
        size_t candidate_len = tokens->items[i].len;
        if (strlen(text) == candidate_len && memcmp(candidate, text, candidate_len) == 0) {
            return (long)i;
        }
    }
    return -1;
}

BT_TEST(mutate, tokenizer_skips_comments_and_reads_prefixed_literals) {
    const char *src = "// line one\n"
                      "int a; /* open\n"
                      "still open */ int b;\n"
                      "const char *s = u8\"esc\\\"ape\";\n"
                      "wchar_t w = L'x';\n"
                      "double h = 0x1p-4;\n"
                      "double f = 1.5e+3;\n";
    bt_mut_tokens tokens;
    BT_CHECK_EQ(bt_mut_tokenize(src, strlen(src), &tokens), 0);
    long a_index = find_token(&tokens, "a");
    long b_index = find_token(&tokens, "b");
    BT_CHECK(a_index >= 0);
    BT_CHECK(b_index > a_index);
    if (a_index >= 0) {
        BT_CHECK_EQ(tokens.items[a_index].line, 2ULL);
        BT_CHECK_EQ(tokens.items[a_index].kind, (long long)BT_MUT_TOK_IDENT);
    }
    if (b_index >= 0) {
        BT_CHECK_EQ(tokens.items[b_index].line, 3ULL);
    }
    long h_index = find_token(&tokens, "0x1p-4");
    long f_index = find_token(&tokens, "1.5e+3");
    BT_CHECK(h_index >= 0);
    BT_CHECK(f_index >= 0);
    if (h_index >= 0) {
        BT_CHECK_EQ(tokens.items[h_index].kind, (long long)BT_MUT_TOK_NUMBER);
    }
    if (f_index >= 0) {
        BT_CHECK_EQ(tokens.items[f_index].kind, (long long)BT_MUT_TOK_NUMBER);
    }
    int saw_string = 0;
    int saw_char = 0;
    for (size_t i = 0; i < tokens.count; i++) {
        const char *text = bt_mut_token_text(&tokens, &tokens.items[i]);
        if (tokens.items[i].kind == BT_MUT_TOK_STRING) {
            saw_string = 1;
            BT_CHECK(strstr(text, "u8\"") == text || text[0] == '"');
        }
        if (tokens.items[i].kind == BT_MUT_TOK_CHAR) {
            saw_char = 1;
        }
    }
    BT_CHECK(saw_string);
    BT_CHECK(saw_char);
    bt_mut_tokens_free(&tokens);
}

BT_TEST(mutate, tokenizer_and_span_helpers_edge_inputs) {
    const char *src = "int f(void) {\n    return 3;\n}\nint g(void) {\n    return 4;\n}\n";
    bt_mut_tokens tokens;
    BT_CHECK_EQ(bt_mut_tokenize(src, strlen(src), &tokens), 0);
    BT_CHECK_EQ(bt_mut_offset_at(src, 1, 1), 0ULL);
    BT_CHECK_EQ(bt_mut_offset_at(src, 99, 99), (size_t)-1);
    size_t three = bt_mut_offset_at(src, 2, 12);
    const char *fname = bt_mut_function_at(&tokens, three);
    BT_CHECK(fname != NULL && strcmp(fname, "f") == 0);
    size_t four = bt_mut_offset_at(src, 5, 12);
    fname = bt_mut_function_at(&tokens, four);
    BT_CHECK(fname != NULL && strcmp(fname, "g") == 0);
    size_t second_int = bt_mut_offset_at(src, 4, 1);
    fname = bt_mut_function_at(&tokens, second_int);
    BT_CHECK(fname == NULL || fname[0] == '\0');
    bt_mut_tokens_free(&tokens);
}

BT_TEST(mutate, comparison_swap_variants) {
    const char *src =
        "int f(int a, int b) {\n    if (a < b) {\n        return 1;\n    }\n    if (a "
        "== b) {\n        return 2;\n    }\n    while (a >= b) {\n        return "
        "3;\n    }\n    return 0;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t swaps = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_COMPARISON_SWAP) {
            swaps++;
        }
    }
    BT_CHECK_EQ(swaps, 3ULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, "<", "<=", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, "==", "!=", 5) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, ">=", ">", 8) != NULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, negation_guard_mutants) {
    const char *src = "int f(int a) {\n    if (a) {\n        return 1;\n    }\n    while (a) "
                      "{\n        a = 0;\n    }\n    return 0;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t negations = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_CONDITION_NEGATION) {
            negations++;
        }
    }
    BT_CHECK_EQ(negations, 2ULL);
    const bt_mut_mutant *if_mutant =
        bt_mut_find(&list, BT_MUT_CONDITION_NEGATION, "if", "!(...)", 2);
    const bt_mut_mutant *while_mutant =
        bt_mut_find(&list, BT_MUT_CONDITION_NEGATION, "while", "!(...)", 5);
    BT_CHECK(if_mutant != NULL);
    BT_CHECK(while_mutant != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (if_mutant != NULL) {
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), if_mutant, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "if (!(a)) {") != NULL);
        free(applied);
        applied = NULL;
    }
    if (while_mutant != NULL) {
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), while_mutant, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "while (!(a)) {") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, numeric_variants_and_suffixes) {
    const char *src = "unsigned a = 3u;\ndouble d = 2.5f;\nlong z = 0;\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t numeric = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_NUMERIC_TWEAK) {
            numeric++;
        }
    }
    BT_CHECK_EQ(numeric, 10ULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "3u", "4u", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "3u", "0u", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2.5f", "3.5f", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2.5f", "0.0f", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "-1", 3) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", 3) != NULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, exponent_literals_are_float_tweaks) {
    const char *src = "double a = 1.5e3;\ndouble b = 0x1p4;\nfloat c = 2e9f;\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t numeric = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_NUMERIC_TWEAK) {
            numeric++;
        }
    }
    BT_CHECK_EQ(numeric, 12ULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "1.5e3", "1501.0", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "1.5e3", "1499.0", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "1.5e3", "0.0", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0x1p4", "17.0", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0x1p4", "0.0", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2e9f", "0.0f", 3) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2e9f", "1.0f", 3) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2e9f", "2000000001.0f", 3) != NULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, preprocessor_lines_stay_untouched) {
    const char *src = "#define N 3\n#define CMP(a, b) ((a) < (b))\nint f(void) {\n    return N + "
                      "3;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    BT_CHECK_EQ(list.count, 4ULL);
    for (size_t i = 0; i < list.count; i++) {
        BT_CHECK_EQ(list.items[i].op, (long long)BT_MUT_NUMERIC_TWEAK);
        BT_CHECK_EQ(list.items[i].line, 4ULL);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, statement_delete_skips_declarations_and_control) {
    const char *src = "int f(int v) {\n    int r = 10;\n    r = v + 5;\n    for (r = 0; r < v; r"
                      "++) {\n        r = 9;\n    }\n    return r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    const bt_mut_mutant *target = NULL;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_STATEMENT_DELETE) {
            deletions++;
            if (target == NULL || list.items[i].line < target->line) {
                target = &list.items[i];
            }
        }
    }
    BT_CHECK_EQ(deletions, 2ULL);
    BT_CHECK(target != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (target != NULL) {
        BT_CHECK_EQ(target->line, 3ULL);
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), target, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "r = v + 5;") == NULL);
        BT_CHECK(strstr(applied, "r = 9;") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, tokenizer_kinds_and_positions) {
    const char *src = "int f(void) {\n    return 3;\n}\n#define X 4\n";
    bt_mut_tokens tokens;
    BT_CHECK_EQ(bt_mut_tokenize(src, strlen(src), &tokens), 0);
    long kw = find_token(&tokens, "int");
    long ident = find_token(&tokens, "f");
    long number = find_token(&tokens, "3");
    long ret = find_token(&tokens, "return");
    BT_CHECK(kw >= 0 && tokens.items[kw].kind == BT_MUT_TOK_KEYWORD);
    BT_CHECK_EQ(tokens.items[kw].line, 1ULL);
    BT_CHECK_EQ(tokens.items[kw].col, 1ULL);
    BT_CHECK(ident >= 0 && tokens.items[ident].kind == BT_MUT_TOK_IDENT);
    BT_CHECK(number >= 0 && tokens.items[number].kind == BT_MUT_TOK_NUMBER);
    BT_CHECK_EQ(tokens.items[number].line, 2ULL);
    BT_CHECK_EQ(tokens.items[number].col, 12ULL);
    BT_CHECK(ret >= 0 && tokens.items[ret].kind == BT_MUT_TOK_KEYWORD);
    BT_CHECK(tokens.count >= 1 && tokens.items[tokens.count - 1].kind == BT_MUT_TOK_PP);
    BT_CHECK(strncmp(bt_mut_token_text(&tokens, &tokens.items[tokens.count - 1]), "#define", 7) ==
             0);
    bt_mut_tokens_free(&tokens);
}

BT_TEST(mutate, unary_drop_mutants) {
    const char *src = "int f(int a) {\n    if (!a) {\n        a++;\n    }\n    --a;\n    return "
                      "a;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t drops = 0;
    const bt_mut_mutant *bang = NULL;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_UNARY_DROP) {
            drops++;
            if (strcmp(list.items[i].from, "!") == 0) {
                bang = &list.items[i];
            }
        }
    }
    BT_CHECK_EQ(drops, 3ULL);
    BT_CHECK(bang != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (bang != NULL) {
        BT_CHECK_EQ(bang->to[0], '\0');
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), bang, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "if (a)") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, for_headers_and_unbraced_bodies_delete_correctly) {
    const char *src = "int f(int v, int n) {\n    int r = 0;\n    for (r = 0; r < n; r++) {\n"
                      "        r = 9;\n    }\n    if (v)\n        r = 1;\n    return r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op != BT_MUT_STATEMENT_DELETE) {
            continue;
        }
        deletions++;
        BT_CHECK(list.items[i].line == 4 || list.items[i].line == 7);
    }
    BT_CHECK_EQ(deletions, 2ULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, literal_only_trace_calls_stay_undeleted) {
    const char *src = "int f(int v) {\n    int r = 0;\n    BT_TRACE_ENTER(\"unit\");\n"
                      "    BT_TRACE_FAIL(\"unit %d\", helper(r));\n    r = v + 1;\n    return "
                      "r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op != BT_MUT_STATEMENT_DELETE) {
            continue;
        }
        deletions++;
        BT_CHECK(list.items[i].line == 4 || list.items[i].line == 5);
    }
    BT_CHECK_EQ(deletions, 2ULL);
    bt_mut_mutants_free(&list);
}
