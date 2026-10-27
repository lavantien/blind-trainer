#ifndef VISUAL_DIFF_LIB_TEST_UTIL_H
#define VISUAL_DIFF_LIB_TEST_UTIL_H

#include "visual_diff_test_util.h"

static void vd_util_put_pixel(btvd_image *img, uint32_t x, uint32_t y,
                              const unsigned char rgba[4]) {
    memcpy(img->pixels + ((size_t)y * img->width + x) * 4, rgba, 4);
}

static int vd_util_is_dir(const char *path) {
    return btvd_fs_is_dir(path);
}

static size_t vd_util_read_bytes(const char *path, unsigned char *out, size_t cap) {
    char err[BTVD_ERR_MAX];
    size_t len = 0;
    unsigned char *bytes = btvd_fs_read_bytes(path, &len, err, sizeof err);
    if (bytes == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "%s", err);
        return 0;
    }
    if (len > cap) {
        len = cap;
    }
    memcpy(out, bytes, len);
    free(bytes);
    return len;
}

static void vd_util_env_for(btvd_env *env, const char *root, const char *config, char **argv,
                            size_t argc) {
    env->argv = argv;
    env->argc = argc;
    env->config = config;
    env->artifacts = NULL;
    snprintf(env->root, BT_PATH_MAX, "%s", root);
}

static void vd_util_write_config(const char *root, char out[VD_UTIL_PATH_MAX], unsigned tolerance) {
    vd_util_join(out, root, "config.json");
    char text[128];
    snprintf(text, sizeof text, "{\"visual\": {\"tolerance\": %u}}", tolerance);
    vd_util_write(out, text);
}

static int vd_util_redirect_stdout(const char *path) {
    fflush(stdout);
#ifdef _WIN32
    int saved = _dup(1);
#else
    int saved = dup(1);
#endif
    if (saved < 0) {
        return -1;
    }
    if (freopen(path, "wb", stdout) == NULL) {
        return -1;
    }
    return saved;
}

static void vd_util_restore_stdout(int saved) {
    fflush(stdout);
#ifdef _WIN32
    _dup2(saved, 1);
    _close(saved);
#else
    dup2(saved, 1);
    close(saved);
#endif
}

#endif
