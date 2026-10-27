#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"

static char *dup_str(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static int push_flag(char ***flags, size_t *count, const char *flag) {
    char *copy = dup_str(flag);
    if (copy == NULL) {
        return -1;
    }
    *flags = realloc(*flags, (*count + 1) * sizeof(char *));
    if (*flags == NULL) {
        free(copy);
        return -1;
    }
    (*flags)[*count] = copy;
    (*count)++;
    return 0;
}

static int clang_from_env_json(const char *repo_root, char out[BT_MUT_PATH_MAX]) {
    char env_path[BT_MUT_PATH_MAX];
    snprintf(env_path, sizeof env_path, "%s/build/env.json", repo_root);
    bt_json_value *env = NULL;
    if (bt_config_try_load(env_path, &env) != 0) {
        return -1;
    }
    const bt_json_value *clang = bt_config_value(env, "clang.path");
    int rc = -1;
    if (clang != NULL && clang->type == BT_JSON_STRING) {
        snprintf(out, BT_MUT_PATH_MAX, "%s", clang->as.string);
        rc = 0;
    }
    bt_json_free(env);
    return rc;
}

int bt_mut_build_load(const char *repo_root, const bt_json_value *config,
                      const char *clang_override, bt_mut_build *out) {
    BT_TRACE_ENTER("mutate.build_load");
    memset(out, 0, sizeof *out);
    if (clang_override != NULL && clang_override[0] != '\0') {
        snprintf(out->clang, BT_MUT_PATH_MAX, "%s", clang_override);
    } else {
        const char *configured = NULL;
        if (bt_config_str(config, "build.tools.clang", &configured) == 0 && configured[0] != '\0') {
            snprintf(out->clang, BT_MUT_PATH_MAX, "%s", configured);
        } else if (clang_from_env_json(repo_root, out->clang) != 0) {
            snprintf(out->clang, BT_MUT_PATH_MAX, "clang");
        }
    }
    const bt_json_value *build = bt_config_value(config, "build");
    if (build == NULL || build->type != BT_JSON_OBJECT) {
        bt_mut_build_free(out);
        BT_TRACE_FAIL("mutate.build_load", "missing build section");
        BT_TRACE_EXIT("mutate.build_load");
        return -1;
    }
    const char *target = "";
    const char *std = "c23";
    bt_config_str(config, "build.target", &target);
    bt_config_str(config, "build.std", &std);
    char std_flag[64];
    snprintf(std_flag, sizeof std_flag, "-std=%s", std);
    int rc = push_flag(&out->compile_flags, &out->compile_flag_count, "-target");
    rc = rc == 0 ? push_flag(&out->compile_flags, &out->compile_flag_count, target) : rc;
    rc = rc == 0 ? push_flag(&out->compile_flags, &out->compile_flag_count, std_flag) : rc;
    const bt_json_value *cflags = bt_json_get(build, "cflags");
    if (rc == 0 && cflags != NULL && cflags->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < cflags->as.array.count && rc == 0; i++) {
            const bt_json_value *flag = cflags->as.array.items[i];
            if (flag->type == BT_JSON_STRING) {
                rc = push_flag(&out->compile_flags, &out->compile_flag_count, flag->as.string);
            }
        }
    }
    const bt_json_value *release_flags = bt_json_get(build, "release_flags");
    if (rc == 0 && release_flags != NULL && release_flags->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < release_flags->as.array.count && rc == 0; i++) {
            const bt_json_value *flag = release_flags->as.array.items[i];
            if (flag->type == BT_JSON_STRING) {
                rc = push_flag(&out->compile_flags, &out->compile_flag_count, flag->as.string);
            }
        }
    }
    const bt_json_value *link_flags = bt_json_get(build, "link_flags");
    if (rc == 0 && link_flags != NULL && link_flags->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < link_flags->as.array.count && rc == 0; i++) {
            const bt_json_value *flag = link_flags->as.array.items[i];
            if (flag->type == BT_JSON_STRING) {
                rc = push_flag(&out->link_flags, &out->link_flag_count, flag->as.string);
            }
        }
    }
    static const char *const includes[3] = {"src", "tools", "tests"};
    for (int i = 0; i < 3 && rc == 0; i++) {
        snprintf(out->include_dirs[i], BT_MUT_PATH_MAX, "%s/%s", repo_root, includes[i]);
    }
    snprintf(out->system_include_dir, BT_MUT_PATH_MAX, "%s/vendor/sqlite", repo_root);
    if (rc != 0) {
        bt_mut_build_free(out);
        BT_TRACE_FAIL("mutate.build_load", "flag assembly failed");
        BT_TRACE_EXIT("mutate.build_load");
        return -1;
    }
    BT_TRACE_EXIT("mutate.build_load");
    return 0;
}

void bt_mut_build_free(bt_mut_build *build) {
    for (size_t i = 0; i < build->compile_flag_count; i++) {
        free(build->compile_flags[i]);
    }
    free(build->compile_flags);
    for (size_t i = 0; i < build->link_flag_count; i++) {
        free(build->link_flags[i]);
    }
    free(build->link_flags);
    build->compile_flags = NULL;
    build->compile_flag_count = 0;
    build->link_flags = NULL;
    build->link_flag_count = 0;
}

size_t bt_mut_compile_argv(const bt_mut_build *build, const char *dir_include,
                           const char *extra_include, const char *source, const char *obj,
                           char **argv, size_t cap) {
    size_t count = 0;
    if (cap < 4) {
        return 0;
    }
    argv[count++] = (char *)build->clang;
    for (size_t i = 0; i < build->compile_flag_count && count < cap; i++) {
        argv[count++] = build->compile_flags[i];
    }
    if (dir_include != NULL && dir_include[0] != '\0') {
        if (count + 2 > cap) {
            return 0;
        }
        argv[count++] = (char *)"-I";
        argv[count++] = (char *)dir_include;
    }
    if (extra_include != NULL && extra_include[0] != '\0') {
        if (count + 2 > cap) {
            return 0;
        }
        argv[count++] = (char *)"-I";
        argv[count++] = (char *)extra_include;
    }
    for (int i = 0; i < 3; i++) {
        if (count + 2 > cap) {
            return 0;
        }
        argv[count++] = (char *)"-I";
        argv[count++] = (char *)build->include_dirs[i];
    }
    if (count + 2 > cap) {
        return 0;
    }
    argv[count++] = (char *)"-isystem";
    argv[count++] = (char *)build->system_include_dir;
    if (count + 4 > cap) {
        return 0;
    }
    argv[count++] = (char *)"-c";
    argv[count++] = (char *)source;
    argv[count++] = (char *)"-o";
    argv[count++] = (char *)obj;
    return count;
}

size_t bt_mut_link_argv(const bt_mut_build *build, char *const *objects, size_t object_count,
                        const char *exe, char **argv, size_t cap) {
    size_t count = 0;
    if (cap < 4) {
        return 0;
    }
    argv[count++] = (char *)build->clang;
    for (size_t i = 0; i < build->compile_flag_count && count < cap; i++) {
        argv[count++] = build->compile_flags[i];
    }
    if (count + 2 > cap) {
        return 0;
    }
    argv[count++] = (char *)"-fuse-ld=lld-link";
    for (size_t i = 0; i < build->link_flag_count; i++) {
        if (count + 2 > cap) {
            return 0;
        }
        argv[count++] = (char *)"-Xlinker";
        argv[count++] = build->link_flags[i];
    }
    for (size_t i = 0; i < object_count; i++) {
        if (count + 1 > cap) {
            return 0;
        }
        argv[count++] = objects[i];
    }
    if (count + 2 > cap) {
        return 0;
    }
    argv[count++] = (char *)"-o";
    argv[count++] = (char *)exe;
    return count;
}
