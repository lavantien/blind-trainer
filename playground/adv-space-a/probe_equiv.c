#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/space.h"

size_t old_whitespace_len(const char *s);

static unsigned long long checks;
static unsigned long long mismatches;

static void fail_case(const char *phase, const unsigned char *bytes, size_t fill, size_t at,
                      size_t len, size_t got_old, size_t got_new) {
    if (mismatches > 20) {
        return;
    }
    printf("MISMATCH phase=%s fill=%zu at=%zu len=%zu old=%zu new=%zu bytes=", phase, fill, at, len,
           got_old, got_new);
    for (size_t i = 0; i < fill; i++) {
        printf("%02X", bytes[i]);
    }
    printf("\n");
}

static void compare_offsets(const char *phase, unsigned char *bytes, size_t fill, size_t from,
                            int interior) {
    bytes[fill] = 0;
    size_t len = strlen((const char *)bytes);
    for (size_t at = from; at <= len; at++) {
        if (!interior && at > from && at < len) {
            continue;
        }
        size_t got_old = old_whitespace_len((const char *)bytes + at);
        size_t got_new = bt_space_len((const char *)bytes, at, len);
        checks++;
        if (got_old != got_new) {
            mismatches++;
            fail_case(phase, bytes, fill, at, len, got_old, got_new);
        }
    }
}

static unsigned long long rng_state = 0x9E3779B97F4A7C15ULL;

static unsigned long long rng_next(void) {
    unsigned long long x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void phase_exhaustive(void) {
    printf("phase exhaustive: enter\n");
    fflush(stdout);
    unsigned char bytes[8];
    compare_offsets("exhaustive0", bytes, 0, 0, 1);
    for (unsigned b0 = 0; b0 < 256; b0++) {
        bytes[0] = (unsigned char)b0;
        compare_offsets("exhaustive1", bytes, 1, 0, 1);
    }
    for (unsigned b0 = 0; b0 < 256; b0++) {
        for (unsigned b1 = 0; b1 < 256; b1++) {
            bytes[0] = (unsigned char)b0;
            bytes[1] = (unsigned char)b1;
            compare_offsets("exhaustive2", bytes, 2, 0, 1);
        }
    }
    unsigned long long done = 0;
    for (unsigned b0 = 0; b0 < 256; b0++) {
        for (unsigned b1 = 0; b1 < 256; b1++) {
            for (unsigned b2 = 0; b2 < 256; b2++) {
                bytes[0] = (unsigned char)b0;
                bytes[1] = (unsigned char)b1;
                bytes[2] = (unsigned char)b2;
                compare_offsets("exhaustive3", bytes, 3, 0, 0);
                done++;
            }
        }
        printf("phase exhaustive3: %llu/256 lead bytes\n", done >> 16);
        fflush(stdout);
    }
    printf("phase exhaustive: exit checks=%llu mismatches=%llu\n", checks, mismatches);
    fflush(stdout);
}

static void phase_families(void) {
    printf("phase families: enter\n");
    fflush(stdout);
    static const unsigned leads[][2] = {
        {0xC2, 0x85}, {0xC2, 0xA0}, {0xC2, 0x84}, {0xE1, 0x9A}, {0xE1, 0x9B},
        {0xE2, 0x80}, {0xE2, 0x81}, {0xE2, 0x82}, {0xE3, 0x80}, {0xE3, 0x81},
        {0xED, 0xA0}, {0xED, 0xBF}, {0xF0, 0x90}, {0xF0, 0x80}, {0xF4, 0x8F},
        {0xF5, 0x80}, {0xC0, 0x85}, {0xC1, 0xA0}, {0x80, 0x80}, {0xBF, 0xBF},
    };
    unsigned char bytes[8];
    for (size_t f = 0; f < sizeof leads / sizeof leads[0]; f++) {
        for (unsigned b2 = 0; b2 < 256; b2++) {
            for (unsigned b3 = 0; b3 < 256; b3++) {
                bytes[0] = (unsigned char)leads[f][0];
                bytes[1] = (unsigned char)leads[f][1];
                bytes[2] = (unsigned char)b2;
                bytes[3] = (unsigned char)b3;
                compare_offsets("families4", bytes, 4, 0, 1);
            }
        }
    }
    printf("phase families: exit checks=%llu mismatches=%llu\n", checks, mismatches);
    fflush(stdout);
}

static void phase_random(void) {
    printf("phase random: enter seed=0x9E3779B97F4A7C15\n");
    fflush(stdout);
    unsigned char bytes[20];
    for (unsigned long long iter = 0; iter < 2000000ULL; iter++) {
        size_t fill = (size_t)(rng_next() % 17);
        for (size_t i = 0; i < fill; i++) {
            bytes[i] = (unsigned char)(rng_next() >> 24);
        }
        compare_offsets("random", bytes, fill, 0, 1);
        if ((iter & 0xFFFFFU) == 0xFFFFFU) {
            printf("phase random: %llu iterations\n", iter + 1);
            fflush(stdout);
        }
    }
    printf("phase random: exit checks=%llu mismatches=%llu\n", checks, mismatches);
    fflush(stdout);
}

static void phase_guard(void) {
    printf("phase guard: enter\n");
    fflush(stdout);
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    size_t page = info.dwPageSize;
    char *region = VirtualAlloc(NULL, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (region == NULL) {
        printf("phase guard: VirtualAlloc failed %lu\n", (unsigned long)GetLastError());
        exit(2);
    }
    DWORD protect = 0;
    if (!VirtualProtect(region + page, page, PAGE_NOACCESS, &protect)) {
        printf("phase guard: VirtualProtect failed %lu\n", (unsigned long)GetLastError());
        exit(2);
    }
    char *limit = region + page;
    unsigned long long cases = 0;
    unsigned long long sum = 0;
    for (unsigned lead = 0xC0; lead <= 0xFF; lead++) {
        for (unsigned b1 = 0; b1 < 256; b1++) {
            for (unsigned b2 = 0; b2 < 256; b2++) {
                for (size_t fill = 1; fill <= 3; fill++) {
                    char *start = limit - 1 - (ptrdiff_t)fill;
                    start[0] = (char)lead;
                    if (fill >= 2) {
                        start[1] = (char)b1;
                    }
                    if (fill >= 3) {
                        start[2] = (char)b2;
                    }
                    limit[-1] = 0;
                    sum += old_whitespace_len(start);
                    cases++;
                }
            }
        }
        printf("phase guard: lead %02X done cases=%llu\n", lead, cases);
        fflush(stdout);
    }
    printf("phase guard: exit cases=%llu checksum=%llu guard_page_intact=yes\n", cases, sum);
    fflush(stdout);
}

int main(void) {
    printf("probe_equiv: enter old_whitespace_len vs bt_space_len\n");
    fflush(stdout);
    phase_exhaustive();
    phase_families();
    phase_random();
    phase_guard();
    printf("probe_equiv: exit checks=%llu mismatches=%llu\n", checks, mismatches);
    return mismatches == 0 ? 0 : 1;
}
