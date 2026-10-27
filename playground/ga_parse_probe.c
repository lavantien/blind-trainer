#include "ga/features.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *words[] = {"kh\xc3\xb4ng", "khong", "ta", "ma", "a", "\xc3\xa1t", "g\xc3\xac",
                           "gia", "g\xc3\xac" "a", "kh\xc3\xb4ng"};
    size_t count = sizeof words / sizeof words[0];
    size_t from = argc > 1 ? (size_t)atoi(argv[1]) : 0;
    for (size_t i = from; i < count; i++) {
        bt_ga_features out;
        int ok = bt_ga_parse(words[i], &out);
        printf("%zu %s -> %d tone=%d onset=%d place=%d manner=%d nuc=%d coda=%d\n", i, words[i],
               ok, (int)out.tone, out.has_onset, (int)out.onset.place, (int)out.onset.manner,
               (int)out.nucleus, (int)out.coda);
    }
    return 0;
}
