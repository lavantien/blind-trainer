#ifndef BT_SCHEME_FUZZ_RUNNER_H
#define BT_SCHEME_FUZZ_RUNNER_H

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/rng.h"

#include <direct.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

static inline void bt_fuzz_run_plainloop(const char *corpus_dir, const char *pending_name,
                                         int (*harness)(const uint8_t *, size_t)) {
    char pattern[512];
    snprintf(pattern, sizeof pattern, "%s/*", corpus_dir);
    WIN32_FIND_DATAA found;
    HANDLE find = FindFirstFileA(pattern, &found);
    BT_CHECK(find != INVALID_HANDLE_VALUE);
    size_t replayed = 0;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }
            char path[512];
            snprintf(path, sizeof path, "%s/%s", corpus_dir, found.cFileName);
            FILE *file = fopen(path, "rb");
            if (file == NULL) {
                BT_ABORT("cannot open corpus file %s", path);
            }
            uint8_t buffer[4096];
            size_t size = fread(buffer, 1, sizeof buffer, file);
            fclose(file);
            harness(buffer, size);
            replayed++;
        } while (FindNextFileA(find, &found));
        FindClose(find);
    }
    BT_CHECK(replayed > 0);

    const bt_json_value *config = bt_config_load();
    long long runs = 0;
    long long secs = 0;
    bool has_runs = bt_config_int(config, "fuzz.runs", &runs) == 0 && runs > 0;
    bool has_secs = bt_config_int(config, "fuzz.secs", &secs) == 0 && secs > 0;
    const char *artifacts = getenv("BT_ARTIFACTS");
    if (artifacts == NULL || *artifacts == '\0') {
        artifacts = "artifacts";
    }
    _mkdir(artifacts);
    char fuzz_dir[512];
    snprintf(fuzz_dir, sizeof fuzz_dir, "%s/fuzz", artifacts);
    _mkdir(fuzz_dir);
    char pending[512];
    snprintf(pending, sizeof pending, "%s/fuzz/%s", artifacts, pending_name);
    bt_rng rng;
    bt_rng_seed(&rng, 20261012ULL);
    clock_t start = clock();
    uint64_t generated = 0;
    for (;;) {
        if (has_runs && generated >= (uint64_t)runs) {
            break;
        }
        if (has_secs && (double)(clock() - start) / CLOCKS_PER_SEC >= (double)secs) {
            break;
        }
        size_t len = (size_t)bt_rng_below(&rng, 64);
        uint8_t buffer[64];
        for (size_t k = 0; k < len; k++) {
            buffer[k] = (uint8_t)bt_rng_below(&rng, 256);
        }
        FILE *mark = fopen(pending, "wb");
        if (mark != NULL) {
            fwrite(buffer, 1, len, mark);
            fclose(mark);
        }
        harness(buffer, len);
        generated++;
    }
    remove(pending);
    printf("fuzz %s: %zu corpus inputs, %llu generated inputs\n", corpus_dir, replayed,
           (unsigned long long)generated);
}

#endif
