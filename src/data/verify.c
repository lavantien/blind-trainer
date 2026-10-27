#include "data/verify.h"

#include "common/bt_trace.h"
#include "core/analysis.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    BT_VERIFY_NAME_COUNT = 48,
    BT_VERIFY_FACELET_COUNT = 54,
};

static const int NORMALS[6][3] = {
    {0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0},
};

static int signum_i32(int value) {
    return (value > 0) - (value < 0);
}

int bt_verify_chirality(const uint8_t triple[3]) {
    const int *a = NORMALS[triple[0] / 4];
    const int *b = NORMALS[triple[1] / 4];
    const int *c = NORMALS[triple[2] / 4];
    int cross[3];
    cross[0] = b[1] * c[2] - b[2] * c[1];
    cross[1] = b[2] * c[0] - b[0] * c[2];
    cross[2] = b[0] * c[1] - b[1] * c[0];
    int dot = a[0] * cross[0] + a[1] * cross[1] + a[2] * cross[2];
    return signum_i32(dot);
}

void bt_verdict_init(bt_verdict *verdict) {
    verdict->problems = NULL;
    verdict->problem_count = 0;
    verdict->notes = NULL;
    verdict->note_count = 0;
}

void bt_verdict_free(bt_verdict *verdict) {
    free(verdict->problems);
    verdict->problems = NULL;
    verdict->problem_count = 0;
    free(verdict->notes);
    verdict->notes = NULL;
    verdict->note_count = 0;
}

bool bt_verdict_ok(const bt_verdict *verdict) {
    return verdict->problem_count == 0;
}

static void problem_string(const bt_problem *problem, char *out, size_t out_len) {
    switch (problem->kind) {
    case BT_PROBLEM_BAD_POSITION:
        snprintf(out, out_len, "bad position: %s", problem->detail);
        break;
    case BT_PROBLEM_CENTERS_MOVED:
        snprintf(out, out_len, "centers moved");
        break;
    case BT_PROBLEM_WRONG_CYCLE:
        snprintf(out, out_len, "sticker %s should sit on %s but sits on %s", problem->sticker,
                 problem->expected, problem->actual);
        break;
    case BT_PROBLEM_DISPLACED:
        snprintf(out, out_len, "piece %s should sit in slot %s but sits in %s", problem->piece,
                 problem->expected_slot, problem->actual_slot);
        break;
    case BT_PROBLEM_UNEXPECTED_DEFECT:
        snprintf(out, out_len, "piece %s is %s but should be intact", problem->piece,
                 problem->defect == BT_DEFECT_FLIP ? "flipped" : "twisted");
        break;
    case BT_PROBLEM_MISSING_DEFECT:
        snprintf(out, out_len, "piece %s should be %s but is intact", problem->piece,
                 problem->defect == BT_DEFECT_FLIP ? "flipped" : "twisted");
        break;
    case BT_PROBLEM_NON_FIXED_RESIDUE:
        snprintf(out, out_len, "sticker %s moved to %s outside the named pieces", problem->sticker,
                 problem->actual);
        break;
    case BT_PROBLEM_TWISTS_NOT_OPPOSITE:
        snprintf(out, out_len, "twists of %s and %s are not opposite", problem->first,
                 problem->second);
        break;
    case BT_PROBLEM_TWIST_SUM_NOT_ZERO:
        snprintf(out, out_len, "corner twist sum is %lld, not 0 mod 3", problem->sum);
        break;
    }
}

void bt_verdict_string(const bt_verdict *verdict, char *out, size_t out_len) {
    BT_TRACE_ENTER("verify.verdict_string");
    if (bt_verdict_ok(verdict)) {
        snprintf(out, out_len, "ok");
        BT_TRACE_EXIT("verify.verdict_string");
        return;
    }
    size_t used = 0;
    for (size_t i = 0; i < verdict->problem_count; i++) {
        char line[BT_VERIFY_DETAIL_MAX + 64];
        problem_string(&verdict->problems[i], line, sizeof line);
        size_t len = strlen(line);
        if (used > 0 && used + 1 < out_len) {
            out[used++] = '\n';
        }
        if (used + len >= out_len) {
            break;
        }
        memcpy(out + used, line, len);
        used += len;
    }
    out[used] = '\0';
    BT_TRACE_EXIT("verify.verdict_string");
}

static bool text_matches(const char *field, const char *want) {
    if (want == NULL) {
        return true;
    }
    return strcmp(field, want) == 0;
}

bool bt_verdict_contains_problem(const bt_verdict *verdict, bt_problem_kind kind,
                                 const char *a_text, const char *b_text, const char *c_text) {
    for (size_t i = 0; i < verdict->problem_count; i++) {
        const bt_problem *problem = &verdict->problems[i];
        if (problem->kind != kind) {
            continue;
        }
        bool matched = true;
        if (kind == BT_PROBLEM_BAD_POSITION || kind == BT_PROBLEM_NON_FIXED_RESIDUE ||
            kind == BT_PROBLEM_WRONG_CYCLE) {
            matched = text_matches(problem->sticker, a_text);
            if (kind != BT_PROBLEM_BAD_POSITION) {
                matched = (matched && text_matches(problem->expected, b_text) &&
                           text_matches(problem->actual, c_text)) != 0;
            }
        } else if (kind == BT_PROBLEM_DISPLACED) {
            matched = (text_matches(problem->piece, a_text) &&
                       text_matches(problem->expected_slot, b_text) &&
                       text_matches(problem->actual_slot, c_text)) != 0;
        } else if (kind == BT_PROBLEM_UNEXPECTED_DEFECT || kind == BT_PROBLEM_MISSING_DEFECT) {
            matched = text_matches(problem->piece, a_text);
        } else if (kind == BT_PROBLEM_TWISTS_NOT_OPPOSITE) {
            matched = (text_matches(problem->first, a_text) &&
                       text_matches(problem->second, b_text)) != 0;
        }
        if (matched) {
            return true;
        }
    }
    return false;
}

bool bt_verdict_contains_note(const bt_verdict *verdict, bt_note_kind kind, const char *piece,
                              int8_t twist) {
    for (size_t i = 0; i < verdict->note_count; i++) {
        const bt_note *note = &verdict->notes[i];
        if (note->kind == kind && strcmp(note->piece, piece) == 0 && note->twist == twist) {
            return true;
        }
    }
    return false;
}

static int problems_push(bt_verdict *verdict, const bt_problem *problem) {
    bt_problem *grown = realloc(verdict->problems, (verdict->problem_count + 1) * sizeof *grown);
    if (grown == NULL) {
        return -1;
    }
    verdict->problems = grown;
    verdict->problems[verdict->problem_count] = *problem;
    verdict->problem_count += 1;
    return 0;
}

static int notes_push(bt_verdict *verdict, const bt_note *note) {
    bt_note *grown = realloc(verdict->notes, (verdict->note_count + 1) * sizeof *grown);
    if (grown == NULL) {
        return -1;
    }
    verdict->notes = grown;
    verdict->notes[verdict->note_count] = *note;
    verdict->note_count += 1;
    return 0;
}

static bt_piece piece_of_sticker(uint8_t sticker) {
    return bt_piece_of(sticker);
}

static bool piece_eq(bt_piece a, bt_piece b) {
    return (a.kind == b.kind && a.index == b.index) != 0;
}

static void name_into(char *out, size_t out_len, uint8_t sticker) {
    snprintf(out, out_len, "%s", bt_scheme_name_of(sticker));
}

static int sticker_by_name(const char *name) {
    for (int sticker = 0; sticker < BT_VERIFY_NAME_COUNT; sticker++) {
        if (strcmp(bt_scheme_name_of(sticker), name) == 0) {
            return sticker;
        }
    }
    return -1;
}

static bool resolve_name(const char *name, uint8_t *out) {
    int sticker = sticker_by_name(name);
    if (sticker < 0) {
        return false;
    }
    *out = (uint8_t)sticker;
    return true;
}

static uint8_t piece_index(bt_piece piece) {
    return piece.index;
}

static bool is_edge_piece(bt_piece piece) {
    return piece.kind == BT_PIECE_EDGE;
}

static void piece_label(char *out, size_t out_len, bt_piece piece) {
    name_into(out, out_len, bt_head_sticker(piece));
}

static void sticker_set(bt_piece piece, uint8_t out[3], size_t *out_len) {
    if (piece.kind == BT_PIECE_CORNER) {
        bt_corner_stickers(piece.index, out);
        *out_len = 3;
    } else {
        bt_edge_stickers(piece.index, out);
        *out_len = 2;
    }
}

static bool parse_named(const char *token, bool expect_edge, uint8_t *out, char *detail,
                        size_t detail_len) {
    char trimmed[64];
    size_t len = strlen(token);
    size_t start = 0;
    size_t end = len;
    while (start < end && (token[start] == ' ' || token[start] == '\t')) {
        start += 1;
    }
    while (end > start && (token[end - 1] == ' ' || token[end - 1] == '\t')) {
        end -= 1;
    }
    snprintf(trimmed, sizeof trimmed, "%.*s", (int)(end - start), token + start);
    if (!resolve_name(trimmed, out)) {
        snprintf(detail, detail_len, "unknown sticker name %s", trimmed);
        return false;
    }
    bool edge = *out >= 24;
    if (edge != expect_edge) {
        snprintf(detail, detail_len, "token %s is in the %s orbit", trimmed,
                 edge ? "edge" : "corner");
        return false;
    }
    return true;
}

enum {
    BT_NAMED_MAX = 6,
};

typedef struct {
    size_t count;
    uint8_t stickers[BT_NAMED_MAX];
    bt_piece pieces[BT_NAMED_MAX];
} bt_named;

static bool named_all(const char *position, bool expect_edge, bt_named *out, char *detail,
                      size_t detail_len) {
    out->count = 0;
    size_t token_total = 1;
    for (const char *cursor = position; *cursor != '\0'; cursor++) {
        if (*cursor == '-') {
            token_total += 1;
        }
    }
    if (token_total > BT_NAMED_MAX) {
        snprintf(detail, detail_len, "expected 3 tokens, found %zu", token_total);
        return false;
    }
    const char *cursor = position;
    while (1) {
        const char *start = cursor;
        const char *dash = strchr(cursor, '-');
        const char *stop = dash == NULL ? cursor + strlen(cursor) : dash;
        char token[64];
        snprintf(token, sizeof token, "%.*s", (int)(stop - start), start);
        uint8_t sticker;
        if (!parse_named(token, expect_edge, &sticker, detail, detail_len)) {
            return false;
        }
        bt_piece piece = piece_of_sticker(sticker);
        for (size_t i = 0; i < out->count; i++) {
            if (piece_eq(out->pieces[i], piece)) {
                snprintf(detail, detail_len, "piece %s named twice", token);
                return false;
            }
        }
        out->stickers[out->count] = sticker;
        out->pieces[out->count] = piece;
        out->count += 1;
        if (dash == NULL) {
            break;
        }
        cursor = dash + 1;
    }
    return true;
}

static uint8_t slot_of(const bt_cube *cube, bt_piece piece) {
    uint8_t stickers[3];
    size_t len;
    sticker_set(piece, stickers, &len);
    uint8_t position = bt_cube_position_of(cube, stickers[0]);
    uint8_t probe[3];
    size_t probe_len;
    if (piece.kind == BT_PIECE_CORNER) {
        for (uint8_t slot = 0; slot < 8; slot++) {
            bt_corner_stickers(slot, probe);
            probe_len = 3;
            for (size_t i = 0; i < probe_len; i++) {
                if (probe[i] == position) {
                    return slot;
                }
            }
        }
    } else {
        for (uint8_t slot = 0; slot < 12; slot++) {
            bt_edge_stickers(slot, probe);
            probe_len = 2;
            for (size_t i = 0; i < probe_len; i++) {
                if (probe[i] == position) {
                    return slot;
                }
            }
        }
    }
    return 0;
}

typedef struct {
    bool flipped;
    int8_t twist;
} bt_defect;

static bt_defect defect_of(const bt_cube *cube, bt_piece piece) {
    bt_defect defect = {false, 0};
    uint8_t slot = slot_of(cube, piece);
    if (is_edge_piece(piece)) {
        defect.flipped = bt_edge_flip(cube, slot);
    } else {
        defect.twist = bt_corner_twist(cube, slot);
    }
    return defect;
}

static void fail_detail(bt_verdict *verdict, const char *detail) {
    bt_problem problem;
    memset(&problem, 0, sizeof problem);
    problem.kind = BT_PROBLEM_BAD_POSITION;
    snprintf(problem.detail, sizeof problem.detail, "%s", detail);
    problems_push(verdict, &problem);
}

static void residue_problems(const bt_cube *cube, bt_verdict *verdict, const bt_piece *exempt,
                             size_t exempt_count) {
    for (int sticker = 0; sticker < BT_VERIFY_NAME_COUNT; sticker++) {
        bt_piece piece = piece_of_sticker(sticker);
        bool exempted = false;
        for (size_t i = 0; i < exempt_count; i++) {
            if (piece_eq(exempt[i], piece)) {
                exempted = true;
                break;
            }
        }
        if (exempted) {
            continue;
        }
        uint8_t actual = bt_cube_position_of(cube, sticker);
        if (actual != sticker) {
            bt_problem problem;
            memset(&problem, 0, sizeof problem);
            problem.kind = BT_PROBLEM_NON_FIXED_RESIDUE;
            name_into(problem.sticker, sizeof problem.sticker, sticker);
            name_into(problem.actual, sizeof problem.actual, actual);
            problems_push(verdict, &problem);
        }
    }
}

static bool place_rigid(int16_t expected[BT_VERIFY_FACELET_COUNT], uint8_t from, uint8_t to,
                        char *detail,
                        size_t detail_len) {
    bt_piece piece = piece_of_sticker(from);
    bt_piece slot_piece = piece_of_sticker(to);
    uint8_t piece_stickers[3];
    size_t piece_len;
    sticker_set(piece, piece_stickers, &piece_len);
    size_t piece_split = 0;
    bool found_from = false;
    for (size_t i = 0; i < piece_len; i++) {
        if (piece_stickers[i] == from) {
            piece_split = i;
            found_from = true;
            break;
        }
    }
    if (!found_from) {
        snprintf(detail, detail_len, "sticker %u not on its piece", from);
        return false;
    }
    uint8_t rotated_piece[3];
    for (size_t i = 0; i < piece_len; i++) {
        rotated_piece[i] = piece_stickers[(piece_split + i) % piece_len];
    }
    uint8_t slot_stickers[3];
    size_t slot_len;
    sticker_set(slot_piece, slot_stickers, &slot_len);
    size_t slot_split = 0;
    bool found_to = false;
    for (size_t i = 0; i < slot_len; i++) {
        if (slot_stickers[i] == to) {
            slot_split = i;
            found_to = true;
            break;
        }
    }
    if (!found_to) {
        snprintf(detail, detail_len, "sticker %u not on its piece", to);
        return false;
    }
    uint8_t rotated_slot[3];
    for (size_t i = 0; i < slot_len; i++) {
        rotated_slot[i] = slot_stickers[(slot_split + i) % slot_len];
    }
    if (piece_len == 3) {
        uint8_t piece_triple[3] = {rotated_piece[0], rotated_piece[1], rotated_piece[2]};
        uint8_t slot_triple[3] = {rotated_slot[0], rotated_slot[1], rotated_slot[2]};
        if (bt_verify_chirality(piece_triple) != bt_verify_chirality(slot_triple)) {
            uint8_t swap = rotated_slot[1];
            rotated_slot[1] = rotated_slot[2];
            rotated_slot[2] = swap;
        }
    }
    for (size_t i = 0; i < piece_len; i++) {
        expected[rotated_slot[i]] = rotated_piece[i];
    }
    return true;
}

static int expected_position(const int16_t expected[BT_VERIFY_FACELET_COUNT], uint8_t sticker) {
    for (int slot = 0; slot < BT_VERIFY_FACELET_COUNT; slot++) {
        if (expected[slot] == (int16_t)sticker) {
            return slot;
        }
    }
    return -1;
}

static void verify_three_style(const bt_cube *cube, bt_verdict *verdict, const char *position,
                               bool edge) {
    char detail[BT_VERIFY_DETAIL_MAX];
    bt_named named;
    if (!named_all(position, edge, &named, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    if (named.count != 3) {
        snprintf(detail, sizeof detail, "expected 3 tokens, found %zu", named.count);
        fail_detail(verdict, detail);
        return;
    }
    int16_t expected[BT_VERIFY_FACELET_COUNT];
    for (int i = 0; i < BT_VERIFY_FACELET_COUNT; i++) {
        expected[i] = i >= BT_VERIFY_NAME_COUNT ? (int16_t)i : -1;
    }
    uint8_t pairs[3][2] = {
        {named.stickers[0], named.stickers[1]},
        {named.stickers[1], named.stickers[2]},
        {named.stickers[2], named.stickers[0]},
    };
    for (int i = 0; i < 3; i++) {
        if (!place_rigid(expected, pairs[i][0], pairs[i][1], detail, sizeof detail)) {
            fail_detail(verdict, detail);
            return;
        }
    }
    for (int sticker = 0; sticker < BT_VERIFY_NAME_COUNT; sticker++) {
        int want_slot = expected_position(expected, sticker);
        uint8_t want = want_slot < 0 ? sticker : (uint8_t)want_slot;
        uint8_t got = bt_cube_position_of(cube, sticker);
        if (want == got) {
            continue;
        }
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        name_into(problem.sticker, sizeof problem.sticker, sticker);
        name_into(problem.expected, sizeof problem.expected, want);
        name_into(problem.actual, sizeof problem.actual, got);
        bool named_piece = false;
        for (size_t i = 0; i < named.count; i++) {
            if (piece_eq(named.pieces[i], piece_of_sticker(sticker))) {
                named_piece = true;
                break;
            }
        }
        if (named_piece) {
            problem.kind = BT_PROBLEM_WRONG_CYCLE;
        } else {
            problem.kind = BT_PROBLEM_NON_FIXED_RESIDUE;
        }
        problems_push(verdict, &problem);
    }
}

static bool pair_tokens(const char *position, char first[BT_VERIFY_NAME_MAX],
                        char second[BT_VERIFY_NAME_MAX], char *detail, size_t detail_len) {
    const char *sep = strstr(position, " & ");
    if (sep == NULL) {
        snprintf(detail, detail_len, "expected 2 names joined by ' & ', found %s", position);
        return false;
    }
    const char *second_start = sep + 3;
    if (strstr(second_start, " & ") != NULL) {
        snprintf(detail, detail_len, "expected 2 names joined by ' & ', found %s", position);
        return false;
    }
    snprintf(first, BT_VERIFY_NAME_MAX, "%.*s", (int)(sep - position), position);
    snprintf(second, BT_VERIFY_NAME_MAX, "%s", second_start);
    return true;
}

static bool comma_parts(const char *position, char first[BT_VERIFY_DETAIL_MAX],
                        char second[BT_VERIFY_DETAIL_MAX], char *detail, size_t detail_len) {
    const char *comma = strchr(position, ',');
    if (comma == NULL || strchr(comma + 1, ',') != NULL) {
        snprintf(detail, detail_len, "expected an edge pair and a corner pair, found %s", position);
        return false;
    }
    snprintf(first, BT_VERIFY_DETAIL_MAX, "%.*s", (int)(comma - position), position);
    snprintf(second, BT_VERIFY_DETAIL_MAX, "%s", comma + 1);
    size_t head = strlen(first);
    while (head > 0 && (first[head - 1] == ' ' || first[head - 1] == '\t')) {
        head -= 1;
    }
    first[head] = '\0';
    size_t lead = 0;
    while (first[lead] == ' ' || first[lead] == '\t') {
        lead += 1;
    }
    if (lead > 0) {
        memmove(first, first + lead, strlen(first) - lead + 1);
    }
    size_t skip = 0;
    while (second[skip] == ' ' || second[skip] == '\t') {
        skip += 1;
    }
    if (skip > 0) {
        memmove(second, second + skip, strlen(second) - skip + 1);
    }
    size_t tail = strlen(second);
    while (tail > 0 && (second[tail - 1] == ' ' || second[tail - 1] == '\t')) {
        tail -= 1;
    }
    second[tail] = '\0';
    return true;
}

static void slot_name(char *out, size_t out_len, bt_piece piece, uint8_t slot) {
    uint8_t stickers[3];
    if (piece.kind == BT_PIECE_EDGE) {
        bt_edge_stickers(slot, stickers);
    } else {
        bt_corner_stickers(slot, stickers);
    }
    name_into(out, out_len, stickers[0]);
}

static void check_in_place(const bt_cube *cube, bt_verdict *verdict, bt_piece piece) {
    uint8_t home = piece_index(piece);
    uint8_t slot = slot_of(cube, piece);
    if (slot != home) {
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        problem.kind = BT_PROBLEM_DISPLACED;
        piece_label(problem.piece, sizeof problem.piece, piece);
        slot_name(problem.expected_slot, sizeof problem.expected_slot, piece, home);
        slot_name(problem.actual_slot, sizeof problem.actual_slot, piece, slot);
        problems_push(verdict, &problem);
    }
}

static void check_swapped(const bt_cube *cube, bt_verdict *verdict, bt_piece first,
                          bt_piece second) {
    uint8_t slot_first = slot_of(cube, first);
    uint8_t slot_second = slot_of(cube, second);
    bool ok = (slot_first == piece_index(second) && slot_second == piece_index(first)) != 0;
    if (ok) {
        return;
    }
    bt_problem a;
    memset(&a, 0, sizeof a);
    a.kind = BT_PROBLEM_DISPLACED;
    piece_label(a.piece, sizeof a.piece, first);
    slot_name(a.expected_slot, sizeof a.expected_slot, first, piece_index(second));
    slot_name(a.actual_slot, sizeof a.actual_slot, first, slot_first);
    problems_push(verdict, &a);
    bt_problem b;
    memset(&b, 0, sizeof b);
    b.kind = BT_PROBLEM_DISPLACED;
    piece_label(b.piece, sizeof b.piece, second);
    slot_name(b.expected_slot, sizeof b.expected_slot, second, piece_index(first));
    slot_name(b.actual_slot, sizeof b.actual_slot, second, slot_second);
    problems_push(verdict, &b);
}

static void verify_flip_pair(const bt_cube *cube, bt_verdict *verdict, const char *position) {
    char detail[BT_VERIFY_DETAIL_MAX];
    char tokens[2][BT_VERIFY_NAME_MAX];
    if (!pair_tokens(position, tokens[0], tokens[1], detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    uint8_t stickers[2];
    for (int i = 0; i < 2; i++) {
        if (!parse_named(tokens[i], true, &stickers[i], detail, sizeof detail)) {
            fail_detail(verdict, detail);
            return;
        }
    }
    bt_piece pieces[2] = {piece_of_sticker(stickers[0]), piece_of_sticker(stickers[1])};
    residue_problems(cube, verdict, pieces, 2);
    for (int i = 0; i < 2; i++) {
        check_in_place(cube, verdict, pieces[i]);
        if (!defect_of(cube, pieces[i]).flipped) {
            bt_problem problem;
            memset(&problem, 0, sizeof problem);
            problem.kind = BT_PROBLEM_MISSING_DEFECT;
            problem.defect = BT_DEFECT_FLIP;
            piece_label(problem.piece, sizeof problem.piece, pieces[i]);
            problems_push(verdict, &problem);
        }
    }
}

static void verify_twist_pair(const bt_cube *cube, bt_verdict *verdict, const char *position) {
    char detail[BT_VERIFY_DETAIL_MAX];
    char tokens[2][BT_VERIFY_NAME_MAX];
    if (!pair_tokens(position, tokens[0], tokens[1], detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    uint8_t stickers[2];
    for (int i = 0; i < 2; i++) {
        if (!parse_named(tokens[i], false, &stickers[i], detail, sizeof detail)) {
            fail_detail(verdict, detail);
            return;
        }
    }
    bt_piece pieces[2] = {piece_of_sticker(stickers[0]), piece_of_sticker(stickers[1])};
    residue_problems(cube, verdict, pieces, 2);
    for (int i = 0; i < 2; i++) {
        check_in_place(cube, verdict, pieces[i]);
    }
    int8_t twists[2] = {defect_of(cube, pieces[0]).twist, defect_of(cube, pieces[1]).twist};
    for (int i = 0; i < 2; i++) {
        bt_note note;
        memset(&note, 0, sizeof note);
        note.kind = BT_NOTE_TWIST;
        piece_label(note.piece, sizeof note.piece, pieces[i]);
        note.twist = twists[i];
        notes_push(verdict, &note);
        if (twists[i] == 0) {
            bt_problem problem;
            memset(&problem, 0, sizeof problem);
            problem.kind = BT_PROBLEM_MISSING_DEFECT;
            problem.defect = BT_DEFECT_TWIST;
            piece_label(problem.piece, sizeof problem.piece, pieces[i]);
            problems_push(verdict, &problem);
        }
    }
    if (twists[0] + twists[1] != 0) {
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        problem.kind = BT_PROBLEM_TWISTS_NOT_OPPOSITE;
        piece_label(problem.first, sizeof problem.first, pieces[0]);
        piece_label(problem.second, sizeof problem.second, pieces[1]);
        problems_push(verdict, &problem);
    }
}

static void note_orientation(const bt_cube *cube, bt_verdict *verdict, bt_piece piece) {
    bt_defect defect = defect_of(cube, piece);
    if (is_edge_piece(piece)) {
        if (defect.flipped) {
            bt_note note;
            memset(&note, 0, sizeof note);
            note.kind = BT_NOTE_FLIP;
            piece_label(note.piece, sizeof note.piece, piece);
            notes_push(verdict, &note);
        }
    } else if (defect.twist != 0) {
        bt_note note;
        memset(&note, 0, sizeof note);
        note.kind = BT_NOTE_TWIST;
        piece_label(note.piece, sizeof note.piece, piece);
        note.twist = defect.twist;
        notes_push(verdict, &note);
    }
}

static void verify_parity(const bt_cube *cube, bt_verdict *verdict, const char *position) {
    char detail[BT_VERIFY_DETAIL_MAX];
    char parts[2][BT_VERIFY_DETAIL_MAX];
    if (!comma_parts(position, parts[0], parts[1], detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    bt_named edge;
    if (!named_all(parts[0], true, &edge, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    bt_named corner;
    if (!named_all(parts[1], false, &corner, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    if (edge.count != 2 || corner.count != 2) {
        snprintf(detail, sizeof detail, "parity needs 2 edges and 2 corners: %s", position);
        fail_detail(verdict, detail);
        return;
    }
    bt_piece exempt[BT_NAMED_MAX];
    size_t exempt_count = 0;
    for (size_t i = 0; i < edge.count; i++) {
        exempt[exempt_count++] = edge.pieces[i];
    }
    for (size_t i = 0; i < corner.count; i++) {
        exempt[exempt_count++] = corner.pieces[i];
    }
    residue_problems(cube, verdict, exempt, exempt_count);
    check_swapped(cube, verdict, edge.pieces[0], edge.pieces[1]);
    check_swapped(cube, verdict, corner.pieces[0], corner.pieces[1]);
    for (size_t i = 0; i < exempt_count; i++) {
        note_orientation(cube, verdict, exempt[i]);
    }
}

static void verify_ltct(const bt_cube *cube, bt_verdict *verdict, const char *position) {
    char detail[BT_VERIFY_DETAIL_MAX];
    char parts[2][BT_VERIFY_DETAIL_MAX];
    if (!comma_parts(position, parts[0], parts[1], detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    bt_named edge;
    if (!named_all(parts[0], true, &edge, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    const char *corner_part = parts[1];
    char swap_text[BT_VERIFY_DETAIL_MAX];
    bool has_bracket = false;
    char bracket[BT_VERIFY_NAME_MAX];
    const char *open = strchr(corner_part, '[');
    if (open != NULL) {
        const char *close = strchr(open + 1, ']');
        if (close == NULL) {
            snprintf(detail, sizeof detail, "unclosed bracket in %s", position);
            fail_detail(verdict, detail);
            return;
        }
        has_bracket = true;
        snprintf(swap_text, sizeof swap_text, "%.*s", (int)(open - corner_part), corner_part);
        snprintf(bracket, sizeof bracket, "%.*s", (int)(close - open - 1), open + 1);
    } else {
        snprintf(swap_text, sizeof swap_text, "%s", corner_part);
    }
    if (edge.count != 2) {
        snprintf(detail, sizeof detail, "ltct needs 2 swapped edges: %s", position);
        fail_detail(verdict, detail);
        return;
    }
    if (!has_bracket) {
        snprintf(detail, sizeof detail, "ltct needs a bracketed twist corner: %s", position);
        fail_detail(verdict, detail);
        return;
    }
    bt_named corner;
    if (!named_all(swap_text, false, &corner, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    if (corner.count != 2) {
        snprintf(detail, sizeof detail, "ltct needs 2 swapped corners: %s", position);
        fail_detail(verdict, detail);
        return;
    }
    uint8_t bracket_sticker;
    if (!parse_named(bracket, false, &bracket_sticker, detail, sizeof detail)) {
        fail_detail(verdict, detail);
        return;
    }
    bt_piece bracket_piece = piece_of_sticker(bracket_sticker);
    bt_piece exempt[BT_NAMED_MAX];
    size_t exempt_count = 0;
    for (size_t i = 0; i < edge.count; i++) {
        exempt[exempt_count++] = edge.pieces[i];
    }
    for (size_t i = 0; i < corner.count; i++) {
        exempt[exempt_count++] = corner.pieces[i];
    }
    exempt[exempt_count++] = bracket_piece;
    if (exempt_count != 5) {
        snprintf(detail, sizeof detail, "ltct names a piece twice: %s", position);
        fail_detail(verdict, detail);
        return;
    }
    residue_problems(cube, verdict, exempt, exempt_count);
    check_swapped(cube, verdict, edge.pieces[0], edge.pieces[1]);
    for (size_t i = 0; i < edge.count; i++) {
        if (defect_of(cube, edge.pieces[i]).flipped) {
            bt_problem problem;
            memset(&problem, 0, sizeof problem);
            problem.kind = BT_PROBLEM_UNEXPECTED_DEFECT;
            problem.defect = BT_DEFECT_FLIP;
            piece_label(problem.piece, sizeof problem.piece, edge.pieces[i]);
            problems_push(verdict, &problem);
        }
    }
    check_swapped(cube, verdict, corner.pieces[0], corner.pieces[1]);
    note_orientation(cube, verdict, corner.pieces[0]);
    note_orientation(cube, verdict, corner.pieces[1]);
    check_in_place(cube, verdict, bracket_piece);
    int8_t bracket_twist = defect_of(cube, bracket_piece).twist;
    bt_note note;
    memset(&note, 0, sizeof note);
    note.kind = BT_NOTE_TWIST;
    piece_label(note.piece, sizeof note.piece, bracket_piece);
    note.twist = bracket_twist;
    notes_push(verdict, &note);
    if (bracket_twist == 0) {
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        problem.kind = BT_PROBLEM_MISSING_DEFECT;
        problem.defect = BT_DEFECT_TWIST;
        piece_label(problem.piece, sizeof problem.piece, bracket_piece);
        problems_push(verdict, &problem);
    }
    long long sum = (int)bracket_twist;
    for (size_t i = 0; i < corner.count; i++) {
        sum += (int)defect_of(cube, corner.pieces[i]).twist;
    }
    if (((sum % 3) + 3) % 3 != 0) {
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        problem.kind = BT_PROBLEM_TWIST_SUM_NOT_ZERO;
        problem.sum = sum;
        problems_push(verdict, &problem);
    }
}

void bt_verify(const bt_alg *alg, const char *position, bt_sheet_class sheet, bt_verdict *out) {
    BT_TRACE_ENTER("verify.verify");
    bt_verdict_init(out);
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, alg);
    if (!bt_cube_centers_fixed(&cube)) {
        bt_problem problem;
        memset(&problem, 0, sizeof problem);
        problem.kind = BT_PROBLEM_CENTERS_MOVED;
        problems_push(out, &problem);
    }
    switch (sheet) {
    case BT_SHEET_EDGE:
        verify_three_style(&cube, out, position, true);
        break;
    case BT_SHEET_CORNER:
        verify_three_style(&cube, out, position, false);
        break;
    case BT_SHEET_FLIPS:
        verify_flip_pair(&cube, out, position);
        break;
    case BT_SHEET_TWISTS:
        verify_twist_pair(&cube, out, position);
        break;
    case BT_SHEET_PARITY:
        verify_parity(&cube, out, position);
        break;
    case BT_SHEET_LTCT:
        verify_ltct(&cube, out, position);
        break;
    }
    BT_TRACE_EXIT("verify.verify");
}
