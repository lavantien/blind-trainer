#ifndef VISUAL_DIFF_TEST_UTIL_H
#define VISUAL_DIFF_TEST_UTIL_H

#include "common/bt_harness.h"
#include "visual-diff/fs.h"
#include "visual-diff/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#define VD_UTIL_PATH_MAX 512

static void vd_util_mkdir_p(const char *path) {
    btvd_fs_mkdir_p(path);
}

static void vd_util_join(char out[VD_UTIL_PATH_MAX], const char *dir, const char *leaf) {
    snprintf(out, VD_UTIL_PATH_MAX, "%s/%s", dir, leaf);
}

static int vd_util_is_file(const char *path) {
    return btvd_fs_is_file(path);
}

static void vd_util_rm_rf(const char *path) {
    btvd_fs_rm_rf(path);
}

static void vd_util_scratch(const char *tag, char out[VD_UTIL_PATH_MAX]) {
    char cwd[VD_UTIL_PATH_MAX];
#ifdef _WIN32
    _getcwd(cwd, sizeof cwd);
#else
    getcwd(cwd, sizeof cwd);
#endif
    snprintf(out, VD_UTIL_PATH_MAX, "%s/build/test-tmp/visual-diff-%s", cwd, tag);
    vd_util_rm_rf(out);
    vd_util_mkdir_p(out);
}

static void vd_util_write_bytes(const char *path, const unsigned char *data, size_t len) {
    char err[BTVD_ERR_MAX];
    if (btvd_fs_write_bytes(path, data, len, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "%s", err);
    }
}

static void vd_util_write(const char *path, const char *text) {
    vd_util_write_bytes(path, (const unsigned char *)text, strlen(text));
}

static void vd_util_solid_image(btvd_image *img, uint32_t width, uint32_t height,
                                const unsigned char rgba[4]) {
    img->width = width;
    img->height = height;
    size_t pixels = (size_t)width * height;
    img->pixels = malloc(pixels * 4);
    if (img->pixels == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "out of memory");
        return;
    }
    for (size_t at = 0; at < pixels; at++) {
        memcpy(img->pixels + at * 4, rgba, 4);
    }
}

static void vd_util_free_image(btvd_image *img) {
    free(img->pixels);
    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
}

static void vd_util_write_png(const char *path, const btvd_image *img) {
    Color base = {img->pixels[0], img->pixels[1], img->pixels[2], img->pixels[3]};
    Image ray = GenImageColor((int)img->width, (int)img->height, base);
    for (uint32_t y = 0; y < img->height; y++) {
        for (uint32_t x = 0; x < img->width; x++) {
            const unsigned char *px = img->pixels + ((size_t)y * img->width + x) * 4;
            if (px[0] != base.r || px[1] != base.g || px[2] != base.b || px[3] != base.a) {
                Color dot = {px[0], px[1], px[2], px[3]};
                ImageDrawPixel(&ray, (int)x, (int)y, dot);
            }
        }
    }
    int size = 0;
    unsigned char *bytes = ExportImageToMemory(ray, ".png", &size);
    UnloadImage(ray);
    if (bytes == NULL || size <= 0) {
        MemFree(bytes);
        bt_harness_fail(__FILE__, __LINE__, "cannot export png %s", path);
        return;
    }
    vd_util_write_bytes(path, bytes, (size_t)size);
    MemFree(bytes);
}

#endif
