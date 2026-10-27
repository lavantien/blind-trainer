#ifndef VISUAL_DIFF_LIB_TEST_UTIL_H
#define VISUAL_DIFF_LIB_TEST_UTIL_H

#include "visual_diff_test_util.h"

static void vd_util_put_pixel(btvd_image *img, uint32_t x, uint32_t y,
                              const unsigned char rgba[4]) {
    memcpy(img->pixels + ((size_t)y * img->width + x) * 4, rgba, 4);
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

#endif
