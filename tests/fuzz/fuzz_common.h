#ifndef BT_FUZZ_COMMON_H
#define BT_FUZZ_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"
#include "common/rng.h"
#include "fuzz/fuzz_body.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

typedef struct {
    bool active;
    size_t runs;
    size_t done;
    size_t secs;
    time_t start;
} fuzz_bounds;

static inline void fuzz_bounds_init(fuzz_bounds *bounds) {
    const bt_json_value *config = bt_config_load();
    long long runs = 0;
    long long secs = 0;
    if (bt_config_int(config, "fuzz.runs", &runs) != 0 || runs <= 0) {
        runs = 5000;
    }
    if (bt_config_int(config, "fuzz.secs", &secs) != 0 || secs <= 0) {
        secs = 30;
    }
    bounds->active = true;
    bounds->runs = (size_t)runs;
    bounds->done = 0;
    bounds->secs = (size_t)secs;
    bounds->start = time(NULL);
}

static inline bool fuzz_within_bounds(fuzz_bounds *bounds) {
    if (bounds->done >= bounds->runs) {
        return false;
    }
    if ((size_t)difftime(time(NULL), bounds->start) >= bounds->secs) {
        return false;
    }
    bounds->done += 1;
    return true;
}

static inline void fuzz_repo_root(char out[BT_PATH_MAX]) {
    const char *env = getenv("BT_REF_GROUND");
    if (env != NULL && env[0] != '\0' &&
        (env[0] == '/' || (env[1] == ':' && (env[2] == '\\' || env[2] == '/')))) {
        snprintf(out, BT_PATH_MAX, "%s", env);
        size_t len = strlen(out);
        const char *tail = "ref_ground";
        size_t tail_len = strlen(tail);
        if (len > tail_len && strcmp(out + len - tail_len, tail) == 0) {
            out[len - tail_len - 1] = '\0';
        }
        return;
    }
    bt_paths_workspace_root(out);
}

static inline bool fuzz_replay_dir(const char *name, int (*body)(const uint8_t *, size_t),
                                   fuzz_bounds *bounds, size_t *ran) {
    char root[BT_PATH_MAX];
    fuzz_repo_root(root);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s\\tests\\fuzz\\corpus\\%s", root, name);
#ifdef _WIN32
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s\\%s", dir, entry.cFileName);
        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            continue;
        }
        uint8_t buffer[4096];
        size_t size = fread(buffer, 1, sizeof buffer, file);
        fclose(file);
        if (size == 0) {
            continue;
        }
        if (!fuzz_within_bounds(bounds)) {
            FindClose(find);
            return true;
        }
        body(buffer, size);
        *ran += 1;
    } while (FindNextFileA(find, &entry));
    FindClose(find);
    return true;
#else
    DIR *handle = opendir(dir);
    if (handle == NULL) {
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            continue;
        }
        uint8_t buffer[4096];
        size_t size = fread(buffer, 1, sizeof buffer, file);
        fclose(file);
        if (size == 0) {
            continue;
        }
        if (!fuzz_within_bounds(bounds)) {
            closedir(handle);
            return true;
        }
        body(buffer, size);
        *ran += 1;
    }
    closedir(handle);
    return true;
#endif
}

static inline void fuzz_generate(int (*body)(const uint8_t *, size_t), fuzz_bounds *bounds,
                                 const char *alphabet, size_t alphabet_len, uint64_t seed,
                                 size_t *ran) {
    bt_rng rng;
    bt_rng_seed(&rng, seed);
    while (fuzz_within_bounds(bounds)) {
        uint8_t buffer[256];
        size_t size = (size_t)bt_rng_below(&rng, 128);
        for (size_t i = 0; i < size; i++) {
            if (alphabet_len > 0 && bt_rng_below(&rng, 4) != 0) {
                buffer[i] = (uint8_t)alphabet[bt_rng_below(&rng, (uint64_t)alphabet_len)];
            } else {
                buffer[i] = (uint8_t)bt_rng_below(&rng, 256);
            }
        }
        body(buffer, size);
        *ran += 1;
    }
}

#endif
