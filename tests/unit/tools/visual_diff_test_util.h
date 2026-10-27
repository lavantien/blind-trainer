#ifndef VISUAL_DIFF_TEST_UTIL_H
#define VISUAL_DIFF_TEST_UTIL_H

#include "common/bt_harness.h"
#include "visual-diff/lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "raylib.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define VD_UTIL_PATH_MAX 512

static void vd_util_mkdir_p(const char *path) {
    char partial[VD_UTIL_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
#ifdef _WIN32
        _mkdir(partial);
#else
        mkdir(partial, 0755);
#endif
        partial[at] = keep;
    }
#ifdef _WIN32
    _mkdir(partial);
#else
    mkdir(partial, 0755);
#endif
}

static void vd_util_join(char out[VD_UTIL_PATH_MAX], const char *dir, const char *leaf) {
    snprintf(out, VD_UTIL_PATH_MAX, "%s/%s", dir, leaf);
}

static int vd_util_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static int vd_util_is_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
}

static void vd_util_rm_rf(const char *path) {
    if (!vd_util_is_dir(path)) {
        remove(path);
        return;
    }
#ifdef _WIN32
    char pattern[VD_UTIL_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[VD_UTIL_PATH_MAX];
            vd_util_join(child, path, entry.name);
            vd_util_rm_rf(child);
        } while (_findnext(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
#else
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[VD_UTIL_PATH_MAX];
            vd_util_join(child, path, entry->d_name);
            vd_util_rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
#endif
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

static void vd_util_write(const char *path, const char *text) {
    char parent[VD_UTIL_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash != NULL) {
        *slash = '\0';
        vd_util_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "cannot open %s", path);
        return;
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, file) != len) {
        bt_harness_fail(__FILE__, __LINE__, "short write on %s", path);
    }
    fclose(file);
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
    if (!ExportImage(ray, path)) {
        bt_harness_fail(__FILE__, __LINE__, "cannot write png %s", path);
    }
    UnloadImage(ray);
}

#endif
