#include "scheme/scheme.h"

#include "internal.h"

#include <string.h>

const char *const BT_SCHEME_EDGE_FLOATING[12] = {
    "UF", "UB", "UR", "UL", "FR", "FL", "DF", "DB", "DR", "DL", "BR", "BL",
};

const char *const BT_SCHEME_CORNER_FLOATING[8] = {
    "UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL",
};

static const char *const EDGE_NAMES[24] = {
    "UB", "UR", "UF", "UL", "LU", "LF", "LD", "LB", "FU", "FR", "FD", "FL",
    "RU", "RB", "RD", "RF", "BU", "BL", "BD", "BR", "DF", "DR", "DB", "DL",
};

static const char *const CORNER_NAMES[24] = {
    "UBL", "UBR", "UFR", "UFL", "LUB", "LUF", "LDF", "LDB", "FUL", "FUR", "FDR", "FDL",
    "RUF", "RUB", "RDB", "RDF", "BUR", "BUL", "BDL", "BDR", "DFL", "DFR", "DBR", "DBL",
};

uint8_t bt_scheme_orbit_lo(bt_orbit orbit) {
    return orbit == BT_ORBIT_CORNER ? 0 : 24;
}

uint8_t bt_scheme_orbit_hi(bt_orbit orbit) {
    return orbit == BT_ORBIT_CORNER ? 24 : 48;
}

char bt_scheme_letter_of(uint8_t sticker) {
    BT_TRACE_ENTER("scheme.letter_of");
    if (sticker >= 48) {
        BT_SCHEME_ABORT("scheme.letter_of", "sticker id out of range: %u", sticker);
    }
    char letter = (char)('A' + sticker % 24);
    BT_TRACE_EXIT("scheme.letter_of");
    return letter;
}

uint8_t bt_scheme_sticker_of(bt_orbit orbit, char letter) {
    BT_TRACE_ENTER("scheme.sticker_of");
    if (letter < 'A' || letter > 'X') {
        BT_SCHEME_ABORT("scheme.sticker_of", "letter out of range: %c", letter);
    }
    uint8_t sticker =
        orbit == BT_ORBIT_CORNER ? (uint8_t)(letter - 'A') : (uint8_t)(24 + letter - 'A');
    BT_TRACE_EXIT("scheme.sticker_of");
    return sticker;
}

const char *bt_scheme_name_of(uint8_t sticker) {
    BT_TRACE_ENTER("scheme.name_of");
    if (sticker >= 48) {
        BT_SCHEME_ABORT("scheme.name_of", "sticker id out of range: %u", sticker);
    }
    const char *name = sticker < 24 ? CORNER_NAMES[sticker] : EDGE_NAMES[sticker - 24];
    BT_TRACE_EXIT("scheme.name_of");
    return name;
}

static void face_key(const char *name, uint8_t out[3]) {
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    for (size_t i = 0; i < 3 && name[i] != '\0'; i++) {
        out[i] = (uint8_t)name[i];
    }
    if (out[0] > out[1]) {
        uint8_t tmp = out[0];
        out[0] = out[1];
        out[1] = tmp;
    }
    if (out[1] > out[2]) {
        uint8_t tmp = out[1];
        out[1] = out[2];
        out[2] = tmp;
    }
    if (out[0] > out[1]) {
        uint8_t tmp = out[0];
        out[0] = out[1];
        out[1] = tmp;
    }
}

bool bt_scheme_same_piece(uint8_t a, uint8_t b) {
    BT_TRACE_ENTER("scheme.same_piece");
    if (a >= 48 || b >= 48) {
        BT_SCHEME_ABORT("scheme.same_piece", "sticker id out of range: %u %u", a, b);
    }
    if ((a < 24) != (b < 24)) {
        BT_TRACE_EXIT("scheme.same_piece");
        return false;
    }
    uint8_t key_a[3];
    uint8_t key_b[3];
    face_key(bt_scheme_name_of(a), key_a);
    face_key(bt_scheme_name_of(b), key_b);
    bool same = (key_a[0] == key_b[0] && key_a[1] == key_b[1] && key_a[2] == key_b[2]) != 0;
    BT_TRACE_EXIT("scheme.same_piece");
    return same;
}

uint8_t bt_scheme_head_sticker(const char *piece_name) {
    BT_TRACE_ENTER("scheme.head_sticker");
    size_t len = strlen(piece_name);
    if (len != 2 && len != 3) {
        BT_SCHEME_ABORT("scheme.head_sticker", "piece name must have 2 or 3 faces: %s (%zu)",
                        piece_name, len);
    }
    bt_orbit orbit = len == 2 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
    uint8_t target[3];
    face_key(piece_name, target);
    uint8_t members[3];
    size_t member_count = 0;
    for (uint8_t s = bt_scheme_orbit_lo(orbit); s < bt_scheme_orbit_hi(orbit); s++) {
        uint8_t key[3];
        face_key(bt_scheme_name_of(s), key);
        if (key[0] == target[0] && key[1] == target[1] && key[2] == target[2]) {
            members[member_count++] = s;
        }
    }
    if (member_count == 0) {
        BT_SCHEME_ABORT("scheme.head_sticker", "unknown piece: %s", piece_name);
    }
    uint8_t chosen[3];
    size_t chosen_count = 0;
    for (size_t i = 0; i < member_count; i++) {
        char first = bt_scheme_name_of(members[i])[0];
        if (first == 'U' || first == 'D') {
            chosen[chosen_count++] = members[i];
        }
    }
    if (chosen_count == 0) {
        for (size_t i = 0; i < member_count; i++) {
            char first = bt_scheme_name_of(members[i])[0];
            if (first == 'F' || first == 'B') {
                chosen[chosen_count++] = members[i];
            }
        }
    }
    if (chosen_count != 1) {
        BT_SCHEME_ABORT("scheme.head_sticker", "head is ambiguous for %s", piece_name);
    }
    BT_TRACE_EXIT("scheme.head_sticker");
    return chosen[0];
}

void bt_scheme_edge_floating_heads(uint8_t out[12]) {
    BT_TRACE_ENTER("scheme.edge_floating_heads");
    for (int i = 0; i < 12; i++) {
        out[i] = bt_scheme_head_sticker(BT_SCHEME_EDGE_FLOATING[i]);
    }
    BT_TRACE_EXIT("scheme.edge_floating_heads");
}

void bt_scheme_corner_floating_heads(uint8_t out[8]) {
    BT_TRACE_ENTER("scheme.corner_floating_heads");
    for (int i = 0; i < 8; i++) {
        out[i] = bt_scheme_head_sticker(BT_SCHEME_CORNER_FLOATING[i]);
    }
    BT_TRACE_EXIT("scheme.corner_floating_heads");
}
