#ifndef BT_FUZZ_BODY_H
#define BT_FUZZ_BODY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "common/sha256.h"

static inline void fuzz_crash_path(const char *name, const uint8_t *data, size_t size, char *out,
                                   size_t out_size) {
    char root[BT_PATH_MAX];
    const char *env = getenv("BT_ARTIFACTS");
    if (env != NULL && env[0] != '\0') {
        snprintf(root, sizeof root, "%s", env);
    } else {
        bt_paths_workspace_root(root);
        snprintf(root, sizeof root, "%s\\artifacts", root);
    }
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s\\fuzz", root);
    CreateDirectoryA(dir, NULL);
    uint8_t digest[32];
    bt_sha256(data, size, digest);
    snprintf(out, out_size, "%s\\crash-%s-%02x%02x%02x%02x.bin", dir, name, digest[0], digest[1],
             digest[2], digest[3]);
}

static inline void fuzz_save_crash(const char *name, const uint8_t *data, size_t size) {
    char path[BT_PATH_MAX];
    fuzz_crash_path(name, data, size, path, sizeof path);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fwrite(data, 1, size, file);
    fclose(file);
    printf("crash input saved: %s\n", path);
}

static inline size_t fuzz_lossy_utf8(const uint8_t *data, size_t size, char *out, size_t out_size) {
    size_t filled = 0;
    size_t i = 0;
    while (i < size) {
        uint8_t b = data[i];
        size_t take = 0;
        if (b < 0x80U) {
            take = 1;
        } else if ((b & 0xE0U) == 0xC0U && i + 1 < size && (data[i + 1] & 0xC0U) == 0x80U) {
            take = 2;
        } else if ((b & 0xF0U) == 0xE0U && i + 2 < size && (data[i + 1] & 0xC0U) == 0x80U &&
                   (data[i + 2] & 0xC0U) == 0x80U) {
            take = 3;
        } else if ((b & 0xF8U) == 0xF0U && i + 3 < size && (data[i + 1] & 0xC0U) == 0x80U &&
                   (data[i + 2] & 0xC0U) == 0x80U && (data[i + 3] & 0xC0U) == 0x80U) {
            take = 4;
        }
        if (take == 0) {
            if (filled + 3 < out_size) {
                out[filled++] = (char)0xEF;
                out[filled++] = (char)0xBF;
                out[filled++] = (char)0xBD;
            }
            i += 1;
            continue;
        }
        for (size_t k = 0; k < take && filled + 1 < out_size; k++) {
            out[filled++] = (char)data[i + k];
        }
        i += take;
    }
    out[filled] = '\0';
    return filled;
}

#endif
