#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/alg.h"

void old_alg_init(bt_alg *alg);
void old_alg_free(bt_alg *alg);
bool old_alg_parse(const char *text, bt_alg *out, char *err_token, size_t err_token_size);

#define TABLE_END 0xFE

static unsigned long long cases;
static unsigned long long mismatches;

static void dump_bytes(const unsigned char *bytes, size_t fill) {
    for (size_t i = 0; i < fill; i++) {
        printf("%02X", bytes[i]);
    }
}

static void compare(const char *phase, const unsigned char *bytes, size_t fill, bool null_err) {
    char text[32];
    if (fill >= sizeof text) {
        printf("MISMATCH phase=%s internal fill=%zu over text buffer\n", phase, fill);
        mismatches++;
        return;
    }
    memcpy(text, bytes, fill);
    text[fill] = 0;
    char err_old[80];
    char err_new[80];
    memset(err_old, 0x5A, sizeof err_old - 1);
    memset(err_new, 0xA5, sizeof err_new - 1);
    err_old[sizeof err_old - 1] = 0;
    err_new[sizeof err_new - 1] = 0;
    bt_alg old_alg;
    bt_alg new_alg;
    bt_alg_init(&old_alg);
    bt_alg_init(&new_alg);
    bool ok_old = old_alg_parse(text, &old_alg, null_err ? NULL : err_old,
                                null_err ? 0 : sizeof err_old);
    bool ok_new = bt_alg_parse(text, &new_alg, null_err ? NULL : err_new,
                               null_err ? 0 : sizeof err_new);
    cases++;
    bool same = ok_old == ok_new && old_alg.len == new_alg.len;
    if (same && !ok_old && !null_err && strcmp(err_old, err_new) != 0) {
        same = false;
    }
    if (same) {
        for (size_t i = 0; i < old_alg.len; i++) {
            bt_move a = old_alg.items[i];
            bt_move b = new_alg.items[i];
            if (a.kind != b.kind || a.face != b.face || a.slice != b.slice || a.axis != b.axis ||
                a.quarter != b.quarter) {
                same = false;
                break;
            }
        }
    }
    if (!same) {
        mismatches++;
        if (mismatches <= 20) {
            printf("MISMATCH phase=%s fill=%zu ok_old=%d ok_new=%d len_old=%zu len_new=%zu "
                   "err_old=%s err_new=%s bytes=",
                   phase, fill, ok_old, ok_new, old_alg.len, new_alg.len,
                   null_err ? "(null)" : err_old, null_err ? "(null)" : err_new);
            dump_bytes(bytes, fill);
            printf("\n");
        }
    }
    bt_alg_free(&old_alg);
    bt_alg_free(&new_alg);
}

static unsigned long long rng_state = 0x2545F4914F6CDD1DULL;

static unsigned long long rng_next(void) {
    unsigned long long x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x9E3779B97F4A7C15ULL;
}

static void phase_targeted(void) {
    printf("phase targeted: enter\n");
    fflush(stdout);
    static const unsigned char table[][30] = {
        {TABLE_END},
        {' ', TABLE_END},
        {' ', ' ', ' ', TABLE_END},
        {'\t', TABLE_END},
        {'\n', '\r', TABLE_END},
        {' ', 'R', ' ', 'U', ' ', TABLE_END},
        {'R', ' ', 'U', '\t', TABLE_END},
        {0xC2, 0x85, 'U', TABLE_END},
        {0xE3, 0x80, 0x80, 'U', TABLE_END},
        {'U', 0xC2, 0xA0, 'M', TABLE_END},
        {'U', 0xE3, 0x80, 0x80, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x80, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x81, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x82, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x83, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x84, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x85, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x86, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x87, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x88, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x89, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x8A, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0xA8, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0xA9, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0xAF, 'M', TABLE_END},
        {'U', 0xE2, 0x81, 0x9F, 'M', TABLE_END},
        {'U', 0xE1, 0x9A, 0x80, 'M', TABLE_END},
        {'U', 0xC2, 0x85, 'M', TABLE_END},
        {'U', 0x0B, 'M', TABLE_END},
        {'U', 0x0C, 'M', TABLE_END},
        {'U', 0xC2, 'M', TABLE_END},
        {'U', 0xC2, 0x85, TABLE_END},
        {'U', 0xC2, 0x86, 'M', TABLE_END},
        {0xC2, TABLE_END},
        {0xC2, 0x85, TABLE_END},
        {0xC2, 0xA0, TABLE_END},
        {'U', 0xE2, TABLE_END},
        {'U', 0xE2, 0x80, TABLE_END},
        {'U', 0xE2, 0x80, 0x8A, TABLE_END},
        {'U', 0xE2, 0x80, 0x8B, 'M', TABLE_END},
        {'U', 0xE2, 0x81, TABLE_END},
        {'U', 0xE1, 0x9A, TABLE_END},
        {'U', 0xE1, TABLE_END},
        {'U', 0xE3, 0x80, TABLE_END},
        {'U', 0xE3, TABLE_END},
        {'R', ' ', 0xE2, TABLE_END},
        {'R', 0xE2, 0x80, TABLE_END},
        {'R', ' ', 0xE3, 0x80, TABLE_END},
        {'U', 0xF0, 0x90, 0x80, 0x80, 'M', TABLE_END},
        {'U', 0xF0, 0x9F, 0x98, 0x80, 'M', TABLE_END},
        {'U', 0xED, 0xA0, 0x80, 'M', TABLE_END},
        {'U', 0xED, 0xBF, 0xBF, 'M', TABLE_END},
        {'U', 0xF4, 0x8F, 0xBF, 0xBF, 'M', TABLE_END},
        {'U', 0xC0, 0x85, 'M', TABLE_END},
        {'U', 0xC1, 0xA0, 'M', TABLE_END},
        {'R', 0x80, 'U', TABLE_END},
        {'R', 0xBF, 0x80, 'U', TABLE_END},
        {'R', 0x00, 'U', TABLE_END},
        {'R', ' ', 0x00, ' ', 'U', TABLE_END},
        {0xC2, 0x85, 'U', 0xE3, 0x80, 0x80, TABLE_END},
        {'U', 0xC2, 0xA0, 0xC2, 0xA0, 0xC2, 0xA0, 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x8A, 'M', 0xE2, 0x80, 0xAF, 'M', 0xE1, 0x9A, 0x80, 'M', 0xE2, 0x81, 0x9F,
         'M', 0xC2, 0x85, 'M', TABLE_END},
        {0xE2, 0x80, TABLE_END},
        {' ', 0xE2, 0x80, TABLE_END},
        {0xE2, 0x80, 0x00, 'U', TABLE_END},
        {'U', ' ', ' ', 0xE2, 0x80, 0x8A, ' ', 'M', TABLE_END},
        {'U', 0xE2, 0x80, 0x7F, 'M', TABLE_END},
        {'U', 0xE1, 0x9A, 0x7F, 'M', TABLE_END},
        {'U', 0xE2, 0x81, 0x7F, 'M', TABLE_END},
        {'U', 0xE3, 0x80, 0x7F, 'M', TABLE_END},
        {'U', 0xC2, 0x7F, 'M', TABLE_END},
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        size_t fill = 0;
        while (fill < sizeof table[i] && table[i][fill] != TABLE_END) {
            fill++;
        }
        compare("targeted", table[i], fill, false);
        compare("targeted_null_err", table[i], fill, true);
    }
    unsigned char tail[32];
    for (int variant = 0; variant < 4; variant++) {
        for (size_t i = 0; i < 26; i++) {
            tail[i] = 'R';
        }
        size_t fill = 26;
        if (variant == 0) {
            tail[fill++] = 0xC2;
        } else if (variant == 1) {
            tail[fill++] = 0xC2;
            tail[fill++] = 0x85;
            tail[fill++] = 'U';
        } else if (variant == 2) {
            tail[fill++] = ' ';
            tail[fill++] = 0xE2;
        } else {
            tail[fill++] = 0xE2;
            tail[fill++] = 0x80;
            tail[fill++] = 0x8A;
            tail[fill++] = 'M';
        }
        compare("targeted_long", tail, fill, false);
    }
    printf("phase targeted: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    fflush(stdout);
}

static void phase_exhaustive(void) {
    printf("phase exhaustive: enter lengths 0 to 2\n");
    fflush(stdout);
    unsigned char bytes[8];
    compare("exhaustive0", bytes, 0, false);
    for (unsigned b0 = 0; b0 < 256; b0++) {
        bytes[0] = (unsigned char)b0;
        compare("exhaustive1", bytes, 1, false);
        for (unsigned b1 = 0; b1 < 256; b1++) {
            bytes[0] = (unsigned char)b0;
            bytes[1] = (unsigned char)b1;
            compare("exhaustive2", bytes, 2, false);
        }
    }
    printf("phase exhaustive: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    fflush(stdout);
    printf("phase exhaustive3: enter stride 3\n");
    fflush(stdout);
    unsigned long long counter = 0;
    for (unsigned b0 = 0; b0 < 256; b0++) {
        for (unsigned b1 = 0; b1 < 256; b1++) {
            for (unsigned b2 = 0; b2 < 256; b2++) {
                if (counter % 3 == 0) {
                    bytes[0] = (unsigned char)b0;
                    bytes[1] = (unsigned char)b1;
                    bytes[2] = (unsigned char)b2;
                    compare("exhaustive3", bytes, 3, false);
                }
                counter++;
            }
        }
        printf("phase exhaustive3: %u/256 lead bytes\n", b0 + 1);
        fflush(stdout);
    }
    printf("phase exhaustive3: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    fflush(stdout);
}

static const unsigned char alphabet[] = {
    'R', 'U', 'L', 'F', 'D', 'B', 'r', 'u', 'M', 'E', 'S', 'x', 'y', 'z', '2', '\'',
    ' ', '\t', '\n', '\r', 0x0B, 0x0C, 0xC2, 0x85, 0xA0, 0xE1, 0x9A, 0xE2, 0x80, 0x81,
    0x9F, 0xA8, 0xA9, 0xAF, 0xE3, 0xF0, 0x90, 0xED, 0xBF, 0x00,
};

static void phase_biased(void) {
    printf("phase biased: enter alphabet=%zu seed=0x2545F4914F6CDD1D\n",
           sizeof alphabet / sizeof alphabet[0]);
    fflush(stdout);
    unsigned char bytes[32];
    for (unsigned long long iter = 0; iter < 2000000ULL; iter++) {
        size_t fill = (size_t)(rng_next() % 25);
        for (size_t i = 0; i < fill; i++) {
            bytes[i] = alphabet[rng_next() % (sizeof alphabet / sizeof alphabet[0])];
        }
        compare("biased", bytes, fill, false);
        if ((iter & 0xFFFFFU) == 0xFFFFFU) {
            printf("phase biased: %llu iterations\n", iter + 1);
            fflush(stdout);
        }
    }
    for (unsigned long long iter = 0; iter < 200000ULL; iter++) {
        size_t fill = (size_t)(rng_next() % 25);
        for (size_t i = 0; i < fill; i++) {
            bytes[i] = alphabet[rng_next() % (sizeof alphabet / sizeof alphabet[0])];
        }
        compare("biased_null_err", bytes, fill, true);
    }
    printf("phase biased: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    fflush(stdout);
}

static void phase_uniform(void) {
    printf("phase uniform: enter\n");
    fflush(stdout);
    unsigned char bytes[16];
    for (unsigned long long iter = 0; iter < 1000000ULL; iter++) {
        size_t fill = (size_t)(rng_next() % 13);
        for (size_t i = 0; i < fill; i++) {
            bytes[i] = (unsigned char)(rng_next() >> 32);
        }
        compare("uniform", bytes, fill, false);
        if ((iter & 0xFFFFFU) == 0xFFFFFU) {
            printf("phase uniform: %llu iterations\n", iter + 1);
            fflush(stdout);
        }
    }
    printf("phase uniform: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    fflush(stdout);
}

int main(void) {
    printf("probe_parse: enter old_alg_parse vs bt_alg_parse\n");
    fflush(stdout);
    phase_targeted();
    phase_exhaustive();
    phase_biased();
    phase_uniform();
    printf("probe_parse: exit cases=%llu mismatches=%llu\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
