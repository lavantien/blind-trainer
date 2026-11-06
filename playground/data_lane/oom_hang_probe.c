#include "common/alloc.h"
#include "data/seedpack/normalizer.h"
#include "data/seedpack/parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at";

static bt_pack_entry entry_of(void) {
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", "probe.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "3x3");
    snprintf(entry.method, sizeof entry.method, "%s", "cfop");
    snprintf(entry.stage, sizeof entry.stage, "%s", "pll");
    entry.expected_rows = 1;
    return entry;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }
    const char *mode = argv[1];
    size_t ok = (size_t)strtoul(argv[2], NULL, 10);
    bt_pack_entry entry = entry_of();
    printf("mode=%s arm=%zu\n", mode, ok);
    fflush(stdout);
    if (strcmp(mode, "norm") == 0) {
        const char *alg = argc > 3 ? argv[3] : "R U R'";
        bt_normalized normalized;
        bt_normalize_error err;
        bt_alloc_fail_arm(ok);
        bool good = bt_seedpack_normalize_alg("3x3", alg, &normalized, &err);
        bt_alloc_fail_disarm();
        printf("good=%d count=%zu\n", (int)good, normalized.note_count);
        bt_normalized_free(&normalized);
        return 0;
    }
    char text[512];
    const char *alg = argc > 3 ? argv[3] : "R U R'";
    const char *front = argc > 4 ? argv[4] : "{}";
    snprintf(text, sizeof text, "%s\nT\t%s\t%s\thttps://x\t2026-01-01\n", HEADER, alg, front);
    bt_seed_rows rows;
    bt_seed_error err;
    bt_alloc_fail_arm(ok);
    int rc = bt_seedpack_parse_pack(&entry, text, &rows, &err);
    bt_alloc_fail_disarm();
    printf("rc=%d count=%zu\n", rc, rows.count);
    bt_seed_rows_free(&rows);
    return 0;
}
