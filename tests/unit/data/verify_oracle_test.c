#include "common.h"
#include "data/corpus.h"
#include "data/verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[512];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("corpus load failed: %s", text);
    }
}

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("alg_of failed on %s", text);
    }
    return alg;
}

static size_t split_ws(char *text, char *tokens[], size_t cap) {
    size_t count = 0;
    char *cursor = text;
    while (*cursor != '\0' && count < cap) {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static size_t join_tokens(char *tokens[], size_t count, size_t skip, char *out, size_t out_size) {
    size_t used = 0;
    out[0] = '\0';
    int wrote = 0;
    for (size_t i = 0; i < count; i++) {
        if (i == skip) {
            continue;
        }
        used +=
            (size_t)snprintf(out + used, out_size - used, "%s%s", wrote == 0 ? "" : " ", tokens[i]);
        wrote = 1;
    }
    return used;
}

static int drop_move(const char *text, char *out, size_t out_size) {
    char copy[TD_TEXT_MAX];
    snprintf(copy, sizeof copy, "%s", text);
    char *tokens[64];
    size_t count = split_ws(copy, tokens, 64);
    if (count < 2) {
        return 0;
    }
    join_tokens(tokens, count, count / 2, out, out_size);
    return 1;
}

static int invert_move(const char *text, char *out, size_t out_size) {
    char copy[TD_TEXT_MAX];
    snprintf(copy, sizeof copy, "%s", text);
    char *tokens[64];
    size_t count = split_ws(copy, tokens, 64);
    int index = -1;
    for (size_t i = 0; i < count; i++) {
        if (tokens[i][strlen(tokens[i]) - 1] != '2') {
            index = (int)i;
            break;
        }
    }
    if (index < 0) {
        return 0;
    }
    char inverted[32];
    const char *target = tokens[index];
    if (target[strlen(target) - 1] == '\'') {
        snprintf(inverted, sizeof inverted, "%.*s", (int)(strlen(target) - 1), target);
    } else {
        snprintf(inverted, sizeof inverted, "%s'", target);
    }
    tokens[index] = inverted;
    join_tokens(tokens, count, (size_t)-1, out, out_size);
    return 1;
}

static char swap_face(char letter) {
    switch (letter) {
    case 'U':
        return 'D';
    case 'D':
        return 'U';
    case 'L':
        return 'R';
    case 'R':
        return 'L';
    case 'F':
        return 'B';
    case 'B':
        return 'F';
    case 'u':
        return 'd';
    case 'd':
        return 'u';
    case 'l':
        return 'r';
    case 'r':
        return 'l';
    case 'f':
        return 'b';
    case 'b':
        return 'f';
    case 'M':
        return 'E';
    case 'E':
        return 'M';
    case 'x':
        return 'y';
    case 'y':
        return 'z';
    case 'z':
        return 'x';
    default:
        return letter;
    }
}

static int swap_face_move(const char *text, char *out, size_t out_size) {
    char copy[TD_TEXT_MAX];
    snprintf(copy, sizeof copy, "%s", text);
    char *tokens[64];
    size_t count = split_ws(copy, tokens, 64);
    int index = -1;
    for (size_t i = 0; i < count && index < 0; i++) {
        for (const char *p = tokens[i]; *p != '\0'; p++) {
            if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
                index = (int)i;
                break;
            }
        }
    }
    if (index < 0) {
        return 0;
    }
    char swapped[32];
    size_t n = 0;
    for (const char *p = tokens[index]; *p != '\0'; p++) {
        swapped[n++] = ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) ? swap_face(*p) : *p;
    }
    swapped[n] = '\0';
    tokens[index] = swapped;
    join_tokens(tokens, count, (size_t)-1, out, out_size);
    return 1;
}

BT_TEST(verify_oracle, every_vendor_row_verifies_ok) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t failures = 0;
    for (size_t i = 0; i < corpus.count; i++) {
        const bt_corpus_row *row = &corpus.rows[i];
        bt_verdict verdict;
        bt_verify(&row->alg, row->position, row->sheet, &verdict);
        if (!bt_verdict_ok(&verdict)) {
            if (failures < 5) {
                char text[512];
                bt_verdict_string(&verdict, text, sizeof text);
                printf("row %s/%d/%zu %s: %s\n", bt_solver_dir(row->solver), (int)row->sheet,
                       row->rowid, row->position, text);
            }
            failures += 1;
        }
        bt_verdict_free(&verdict);
    }
    BT_CHECK(failures == 0);
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, corrupted_algs_fail_verification_with_named_problems) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t checked = 0;
    size_t misses = 0;
    for (size_t i = 0; i < corpus.count; i++) {
        const bt_corpus_row *row = &corpus.rows[i];
        if (row->rowid > 6) {
            continue;
        }
        char mutated[3][TD_TEXT_MAX];
        int have[3] = {0};
        have[0] = drop_move(row->alg_text, mutated[0], TD_TEXT_MAX);
        have[1] = invert_move(row->alg_text, mutated[1], TD_TEXT_MAX);
        have[2] = swap_face_move(row->alg_text, mutated[2], TD_TEXT_MAX);
        for (int m = 0; m < 3; m++) {
            if (!have[m]) {
                continue;
            }
            bt_alg alg;
            bt_alg_init(&alg);
            char err[64];
            if (!bt_alg_parse(mutated[m], &alg, err, sizeof err)) {
                continue;
            }
            bt_verdict verdict;
            bt_verify(&alg, row->position, row->sheet, &verdict);
            checked += 1;
            if (bt_verdict_ok(&verdict)) {
                if (misses < 5) {
                    printf("accepted corruption: %s -> %s\n", row->alg_text, mutated[m]);
                }
                misses += 1;
            }
            bt_verdict_free(&verdict);
            bt_alg_free(&alg);
        }
    }
    bt_corpus_free(&corpus);
    BT_CHECK(checked >= 30);
    BT_CHECK(misses == 0);
}

BT_TEST(verify_oracle, wrong_alg_for_a_position_fails) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->position, "UF-UB-UL");
        bt_verdict verdict;
        bt_verify(&row->alg, "UF-UB-UR", BT_SHEET_EDGE, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict) == false);
        char text[512];
        bt_verdict_string(&verdict, text, sizeof text);
        BT_CHECK(strstr(text, "should sit on") != NULL);
        bt_verdict_free(&verdict);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, appended_rotation_reports_centers_moved) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_sheet_class sheets[5] = {
        BT_SHEET_EDGE, BT_SHEET_FLIPS, BT_SHEET_TWISTS, BT_SHEET_PARITY, BT_SHEET_LTCT,
    };
    for (int i = 0; i < 5; i++) {
        const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, sheets[i], 2);
        BT_CHECK(row != NULL);
        if (row == NULL) {
            continue;
        }
        char rotated[TD_TEXT_MAX];
        snprintf(rotated, sizeof rotated, "%s x", row->alg_text);
        bt_alg alg = alg_of(rotated);
        bt_verdict verdict;
        bt_verify(&alg, row->position, sheets[i], &verdict);
        BT_CHECK(bt_verdict_contains_problem(&verdict, BT_PROBLEM_CENTERS_MOVED, NULL, NULL, NULL));
        bt_verdict_free(&verdict);
        bt_alg_free(&alg);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, twists_verdict_reports_opposite_simulated_signs) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_TWISTS, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        bt_verdict verdict;
        bt_verify(&row->alg, row->position, BT_SHEET_TWISTS, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict));
        bt_note twists[2];
        size_t twist_count = 0;
        for (size_t i = 0; i < verdict.note_count && twist_count < 2; i++) {
            if (verdict.notes[i].kind == BT_NOTE_TWIST) {
                twists[twist_count++] = verdict.notes[i];
            }
        }
        BT_CHECK(twist_count == 2);
        BT_CHECK(strcmp(twists[0].piece, twists[1].piece) != 0);
        BT_CHECK(twists[0].twist == -twists[1].twist);
        BT_CHECK(twists[0].twist != 0 && twists[1].twist != 0);
        bt_verdict_free(&verdict);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, parity_verdict_reports_orientation_defects_but_never_fails_on_them) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *clean_row =
        bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_PARITY, 2);
    BT_CHECK(clean_row != NULL);
    if (clean_row != NULL) {
        bt_verdict clean;
        bt_verify(&clean_row->alg, clean_row->position, BT_SHEET_PARITY, &clean);
        BT_CHECK(bt_verdict_ok(&clean));
        BT_CHECK(clean.note_count == 0);
        bt_verdict_free(&clean);
    }
    const bt_corpus_row *twisted_row =
        bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_PARITY, 3);
    BT_CHECK(twisted_row != NULL);
    if (twisted_row != NULL) {
        bt_verdict twisted;
        bt_verify(&twisted_row->alg, twisted_row->position, BT_SHEET_PARITY, &twisted);
        BT_CHECK(bt_verdict_ok(&twisted));
        size_t twist_notes = 0;
        for (size_t i = 0; i < twisted.note_count; i++) {
            if (twisted.notes[i].kind == BT_NOTE_TWIST) {
                twist_notes += 1;
            }
            BT_CHECK(twisted.notes[i].kind != BT_NOTE_FLIP);
        }
        BT_CHECK(twist_notes == 2);
        bt_verdict_free(&twisted);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, ltct_verdict_reports_the_bracket_twist_and_swap_twists) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_LTCT, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        bt_verdict verdict;
        bt_verify(&row->alg, row->position, BT_SHEET_LTCT, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict));
        size_t twists = 0;
        for (size_t i = 0; i < verdict.note_count; i++) {
            if (verdict.notes[i].kind == BT_NOTE_TWIST) {
                twists += 1;
            }
            BT_CHECK(verdict.notes[i].kind != BT_NOTE_FLIP);
        }
        BT_CHECK(twists == 3);
        bt_verdict_free(&verdict);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, defect_carrying_three_style_rows_verify_and_clean_ones_do_too) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *defective = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 5);
    BT_CHECK(defective != NULL);
    if (defective != NULL) {
        BT_CHECK_EQ(defective->position, "UF-UB-LB");
        bt_verdict verdict;
        bt_verify(&defective->alg, defective->position, BT_SHEET_EDGE, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict));
        bt_verdict_free(&verdict);
    }
    const bt_corpus_row *clean = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(clean != NULL);
    if (clean != NULL) {
        bt_verdict verdict;
        bt_verify(&clean->alg, clean->position, BT_SHEET_EDGE, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict));
        bt_verdict_free(&verdict);
    }
    bt_corpus_free(&corpus);
}

BT_TEST(verify_oracle, malformed_positions_report_bad_position) {
    bt_corpus corpus;
    loaded(&corpus);
    const bt_corpus_row *row = bt_corpus_row_at(&corpus, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        struct {
            const char *position;
            bt_sheet_class sheet;
        } cases[] = {
            {"GARBAGE", BT_SHEET_EDGE},        {"UF-UB", BT_SHEET_EDGE},
            {"UF-UB-UL-DR", BT_SHEET_EDGE},    {"UF-UB-UFR", BT_SHEET_EDGE},
            {"UF-UB-UF", BT_SHEET_EDGE},       {"UFR & UBR", BT_SHEET_FLIPS},
            {"UF-UL, UFR", BT_SHEET_PARITY},   {"UF-UL", BT_SHEET_PARITY},
            {"UF-UR, UBR-LUB", BT_SHEET_LTCT}, {"", BT_SHEET_EDGE},
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            bt_verdict verdict;
            bt_verify(&row->alg, cases[i].position, cases[i].sheet, &verdict);
            BT_CHECK(
                bt_verdict_contains_problem(&verdict, BT_PROBLEM_BAD_POSITION, NULL, NULL, NULL));
            bt_verdict_free(&verdict);
        }
    }
    bt_corpus_free(&corpus);
}
