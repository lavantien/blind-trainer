#include "ga/features.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    FILE *file = fopen("ref_doc/vietnamese/syllables-7184-abc.txt", "rb");
    if (file == NULL) {
        puts("no wordlist");
        return 1;
    }
    char line[64];
    size_t zero_onset = 0;
    size_t total = 0;
    size_t shown = 0;
    while (fgets(line, sizeof line, file) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') {
            continue;
        }
        bt_ga_features feat;
        if (!bt_ga_parse(line, &feat)) {
            continue;
        }
        total++;
        if (!feat.has_onset) {
            zero_onset++;
            if (shown < 8) {
                printf("zero form: %s\n", line);
                shown++;
            }
        }
    }
    fclose(file);
    printf("parseable %zu zero onset %zu\n", total, zero_onset);
    return 0;
}
