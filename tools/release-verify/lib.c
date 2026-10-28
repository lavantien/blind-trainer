#include "release-verify/lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/paths.h"
#include "release-verify/png.h"
#include "release-verify/zip.h"

#define RV_SQLITE_MAGIC "SQLite format 3\0"
#define RV_SQLITE_MAGIC_LEN 16
#define RV_USER_VERSION_OFFSET 60
#define RV_WORK_ROOT_NAME "bt-release-verify-run"

static unsigned long rv_work_counter;

static void rv_join(char out[BT_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
}

static void rv_mkdir_p(const char *path, char *err, size_t err_cap) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            CreateDirectoryA(copy, NULL);
            *cursor = '/';
        }
    }
    if (!CreateDirectoryA(copy, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        snprintf(err, err_cap, "mkdir %s: %lu", path, GetLastError());
        return;
    }
    err[0] = '\0';
}

int bt_rv_parse_args(int argc, const char *const argv[], bt_rv_parsed_args *out, char *err,
                     size_t err_cap) {
    BT_TRACE_ENTER("rv.parse_args");
    memset(out, 0, sizeof *out);
    const char *positional[3] = {NULL, NULL, NULL};
    size_t count = 0;
    for (int at = 0; at < argc; at++) {
        if (strcmp(argv[at], "--run") == 0) {
            out->run = 1;
            continue;
        }
        if (strncmp(argv[at], "--", 2) == 0) {
            snprintf(err, err_cap, "unknown flag %s", argv[at]);
            BT_TRACE_FAIL("rv.parse_args", "%s", err);
            BT_TRACE_EXIT("rv.parse_args");
            return -1;
        }
        if (count >= 2) {
            count++;
            continue;
        }
        positional[count++] = argv[at];
    }
    if (count != 2) {
        snprintf(err, err_cap, "%s", BT_RV_USAGE_LINE);
        BT_TRACE_FAIL("rv.parse_args", "%s", err);
        BT_TRACE_EXIT("rv.parse_args");
        return -1;
    }
    snprintf(out->staging, sizeof out->staging, "%s", positional[0]);
    snprintf(out->zip, sizeof out->zip, "%s", positional[1]);
    BT_TRACE_EXIT("rv.parse_args");
    return 0;
}

static int rv_cfg_str(const bt_json_value *section, const char *key, char *out, size_t out_cap,
                      char *err, size_t err_cap) {
    const bt_json_value *value = bt_json_get(section, key);
    if (value == NULL || value->type != BT_JSON_STRING) {
        snprintf(err, err_cap, "config release.%s missing", key);
        return -1;
    }
    snprintf(out, out_cap, "%s", value->as.string);
    return 0;
}

static int rv_cfg_u64(const bt_json_value *section, const char *key, unsigned long long *out,
                      char *err, size_t err_cap) {
    const bt_json_value *value = bt_json_get(section, key);
    if (value == NULL || !bt_json_is_integer(value) || value->integer < 1) {
        snprintf(err, err_cap, "config release.%s must be a positive integer", key);
        return -1;
    }
    *out = (unsigned long long)value->integer;
    return 0;
}

static int rv_cfg_i32(const bt_json_value *section, const char *key, int *out, char *err,
                      size_t err_cap) {
    const bt_json_value *value = bt_json_get(section, key);
    if (value == NULL || !bt_json_is_integer(value) || value->integer < -2147483648LL ||
        value->integer > 2147483647LL) {
        snprintf(err, err_cap, "config release.%s must be an exit code integer", key);
        return -1;
    }
    *out = (int)value->integer;
    return 0;
}

int bt_rv_release_cfg_from(const bt_json_value *value, bt_rv_release_cfg *out, char *err,
                           size_t err_cap) {
    BT_TRACE_ENTER("rv.release_cfg_from");
    memset(out, 0, sizeof *out);
    const bt_json_value *section = bt_json_get(value, "release");
    if (section == NULL) {
        snprintf(err, err_cap, "config release section missing");
        BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
        BT_TRACE_EXIT("rv.release_cfg_from");
        return -1;
    }
    const bt_json_value *e2e = bt_json_get(value, "e2e");
    const bt_json_value *shots = e2e != NULL ? bt_json_get(e2e, "shots") : NULL;
    if (shots == NULL) {
        snprintf(err, err_cap, "config e2e.shots missing");
        BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
        BT_TRACE_EXIT("rv.release_cfg_from");
        return -1;
    }
    if (shots->type != BT_JSON_ARRAY) {
        snprintf(err, err_cap, "config e2e.shots must be a list");
        BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
        BT_TRACE_EXIT("rv.release_cfg_from");
        return -1;
    }
    if (rv_cfg_str(section, "staging_dir", out->staging_dir, sizeof out->staging_dir, err,
                   err_cap) != 0 ||
        rv_cfg_str(section, "exe", out->exe, sizeof out->exe, err, err_cap) != 0 ||
        rv_cfg_str(section, "config_file", out->config_file, sizeof out->config_file, err,
                   err_cap) != 0 ||
        rv_cfg_str(section, "fonts_dir", out->fonts_dir, sizeof out->fonts_dir, err, err_cap) !=
            0 ||
        rv_cfg_u64(section, "run_timeout_secs", &out->run_timeout_secs, err, err_cap) != 0 ||
        rv_cfg_i32(section, "neg_config_exit", &out->neg_config_exit, err, err_cap) != 0 ||
        rv_cfg_i32(section, "usage_exit", &out->usage_exit, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
        BT_TRACE_EXIT("rv.release_cfg_from");
        return -1;
    }
    snprintf(out->db_file, sizeof out->db_file, "%s", BT_RV_DB_FILE_FALLBACK);
    const bt_json_value *paths = bt_json_get(value, "paths");
    const bt_json_value *db_file = paths != NULL ? bt_json_get(paths, "db_file") : NULL;
    if (db_file != NULL && db_file->type == BT_JSON_STRING && db_file->as.string[0] != '\0') {
        snprintf(out->db_file, sizeof out->db_file, "%s", db_file->as.string);
    }
    for (size_t at = 0; at < shots->as.array.count; at++) {
        const bt_json_value *shot = bt_json_at(shots, at);
        if (shot == NULL || shot->type != BT_JSON_STRING) {
            snprintf(err, err_cap, "config e2e.shots must hold names");
            BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
            BT_TRACE_EXIT("rv.release_cfg_from");
            return -1;
        }
        if (out->shot_count >= BT_RV_SHOTS_MAX) {
            snprintf(err, err_cap, "config e2e.shots holds too many names");
            BT_TRACE_FAIL("rv.release_cfg_from", "%s", err);
            BT_TRACE_EXIT("rv.release_cfg_from");
            return -1;
        }
        snprintf(out->shots[out->shot_count], sizeof out->shots[out->shot_count], "%s",
                 shot->as.string);
        out->shot_count++;
    }
    BT_TRACE_EXIT("rv.release_cfg_from");
    return 0;
}

int bt_rv_read_db_user_version(const unsigned char *bytes, size_t len, unsigned int *version,
                               char *err, size_t err_cap) {
    if (len < RV_USER_VERSION_OFFSET + 4 ||
        memcmp(bytes, RV_SQLITE_MAGIC, RV_SQLITE_MAGIC_LEN) != 0) {
        snprintf(err, err_cap, "db file is not a sqlite database");
        return -1;
    }
    *version = ((unsigned int)bytes[RV_USER_VERSION_OFFSET] << 24) |
               ((unsigned int)bytes[RV_USER_VERSION_OFFSET + 1] << 16) |
               ((unsigned int)bytes[RV_USER_VERSION_OFFSET + 2] << 8) |
               bytes[RV_USER_VERSION_OFFSET + 3];
    return 0;
}

static void rv_prep_sandbox(const char *sandbox_root, char cwd[BT_PATH_MAX],
                            char appdata[BT_PATH_MAX], char *err, size_t err_cap) {
    rv_join(cwd, sandbox_root, "cwd");
    rv_join(appdata, sandbox_root, "appdata");
    char inner[BT_RV_ERR_MAX];
    rv_mkdir_p(cwd, inner, sizeof inner);
    if (inner[0] != '\0') {
        snprintf(err, err_cap, "%s", inner);
        return;
    }
    rv_mkdir_p(appdata, inner, sizeof inner);
    if (inner[0] != '\0') {
        snprintf(err, err_cap, "%s", inner);
        return;
    }
    err[0] = '\0';
}

int bt_rv_db_stage(const char *sandbox_root, const bt_rv_release_cfg *cfg, char *err,
                   size_t err_cap) {
    BT_TRACE_ENTER("rv.db_stage");
    char cwd[BT_PATH_MAX];
    char appdata[BT_PATH_MAX];
    rv_prep_sandbox(sandbox_root, cwd, appdata, err, err_cap);
    if (err[0] != '\0') {
        BT_TRACE_FAIL("rv.db_stage", "%s", err);
        BT_TRACE_EXIT("rv.db_stage");
        return -1;
    }
    char db_dir[BT_PATH_MAX];
    char db_path[BT_PATH_MAX];
    rv_join(db_dir, appdata, BT_APP_DIR_NAME);
    rv_join(db_path, db_dir, cfg->db_file);
    FILE *file = fopen(db_path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "cannot read migrated db %s: %s", db_path, strerror(errno));
        BT_TRACE_FAIL("rv.db_stage", "%s", err);
        BT_TRACE_EXIT("rv.db_stage");
        return -1;
    }
    unsigned char bytes[4096];
    size_t len = fread(bytes, 1, sizeof bytes, file);
    int read_error = ferror(file);
    fclose(file);
    if (read_error) {
        snprintf(err, err_cap, "cannot read migrated db %s: short read", db_path);
        BT_TRACE_FAIL("rv.db_stage", "%s", err);
        BT_TRACE_EXIT("rv.db_stage");
        return -1;
    }
    unsigned int version = 0;
    if (bt_rv_read_db_user_version(bytes, len, &version, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.db_stage", "%s", err);
        BT_TRACE_EXIT("rv.db_stage");
        return -1;
    }
    if (version != (unsigned int)BT_RV_LATEST_USER_VERSION) {
        snprintf(err, err_cap, "db user_version %u, expected %llu", version,
                 BT_RV_LATEST_USER_VERSION);
        BT_TRACE_FAIL("rv.db_stage", "%s", err);
        BT_TRACE_EXIT("rv.db_stage");
        return -1;
    }
    BT_TRACE_EXIT("rv.db_stage");
    return 0;
}

int bt_rv_packaged_window_dims(const char *staging, const char *config_file, unsigned int *width,
                               unsigned int *height, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.packaged_window_dims");
    char path[BT_PATH_MAX];
    rv_join(path, staging, config_file);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "cannot read packaged config %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("rv.packaged_window_dims", "%s", err);
        BT_TRACE_EXIT("rv.packaged_window_dims");
        return -1;
    }
    char text[65536];
    size_t len = fread(text, 1, sizeof text - 1, file);
    fclose(file);
    text[len] = '\0';
    bt_json_value *value = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, len, &value, &json_err) != 0) {
        snprintf(err, err_cap, "cannot parse packaged config %s: %s", path, json_err.message);
        BT_TRACE_FAIL("rv.packaged_window_dims", "%s", err);
        BT_TRACE_EXIT("rv.packaged_window_dims");
        return -1;
    }
    const bt_json_value *app = bt_json_get(value, "app");
    const bt_json_value *window = app != NULL ? bt_json_get(app, "window") : NULL;
    const bt_json_value *width_value = window != NULL ? bt_json_get(window, "width") : NULL;
    const bt_json_value *height_value = window != NULL ? bt_json_get(window, "height") : NULL;
    if (width_value == NULL || !bt_json_is_integer(width_value) || width_value->integer < 0 ||
        width_value->integer > 4294967295LL || height_value == NULL ||
        !bt_json_is_integer(height_value) || height_value->integer < 0 ||
        height_value->integer > 4294967295LL) {
        bt_json_free(value);
        snprintf(err, err_cap, "packaged config app.window.%s missing",
                 width_value == NULL || !bt_json_is_integer(width_value) ? "width" : "height");
        BT_TRACE_FAIL("rv.packaged_window_dims", "%s", err);
        BT_TRACE_EXIT("rv.packaged_window_dims");
        return -1;
    }
    *width = (unsigned int)width_value->integer;
    *height = (unsigned int)height_value->integer;
    bt_json_free(value);
    BT_TRACE_EXIT("rv.packaged_window_dims");
    return 0;
}

int bt_rv_zip_root_from(const char *staging, char out[BT_PATH_MAX], char *err, size_t err_cap) {
    const char *slash = strrchr(staging, '/');
    const char *backslash = strrchr(staging, '\\');
    const char *cut = slash > backslash ? slash : backslash;
    if (cut == NULL || cut[1] == '\0') {
        snprintf(err, err_cap, "staging dir %s has no folder name", staging);
        return -1;
    }
    snprintf(out, BT_PATH_MAX, "%s", cut + 1);
    return 0;
}

int bt_rv_check_exit(const bt_rv_outcome *outcome, int expected, char *err, size_t err_cap) {
    if (outcome->kind == BT_RV_OUTCOME_EXIT && outcome->exit_code == expected) {
        err[0] = '\0';
        return 0;
    }
    if (outcome->kind == BT_RV_OUTCOME_EXIT) {
        snprintf(err, err_cap, "exit code %d, expected %d", outcome->exit_code, expected);
        return -1;
    }
    if (outcome->kind == BT_RV_OUTCOME_TIMEOUT) {
        snprintf(err, err_cap, "timed out");
        return -1;
    }
    snprintf(err, err_cap, "did not run: %s", outcome->detail);
    return -1;
}

int bt_rv_check_shot_dir(const char *dir, const bt_rv_release_cfg *cfg, unsigned int width,
                         unsigned int height, size_t *count, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.check_shot_dir");
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        snprintf(err, err_cap, "cannot read shots dir %s", dir);
        BT_TRACE_FAIL("rv.check_shot_dir", "%s", err);
        BT_TRACE_EXIT("rv.check_shot_dir");
        return -1;
    }
    char **names = NULL;
    size_t name_count = 0;
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char path[BT_PATH_MAX];
        rv_join(path, dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        char **grown = realloc(names, (name_count + 1) * sizeof *grown);
        if (grown == NULL) {
            fputs("bt-release-verify: out of memory\n", stderr);
            abort();
        }
        names = grown;
        names[name_count] = malloc(strlen(found.cFileName) + 1);
        if (names[name_count] == NULL) {
            fputs("bt-release-verify: out of memory\n", stderr);
            abort();
        }
        strcpy(names[name_count], found.cFileName);
        name_count++;
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    int failed = 0;
    for (size_t at = 0; at < name_count && !failed; at++) {
        int matched = 0;
        for (size_t shot = 0; shot < cfg->shot_count; shot++) {
            char wanted[BT_RV_SHOT_NAME_MAX + 8];
            snprintf(wanted, sizeof wanted, "%s.png", cfg->shots[shot]);
            if (strcmp(names[at], wanted) == 0) {
                matched = 1;
                break;
            }
        }
        if (!matched) {
            snprintf(err, err_cap, "unexpected file %s in shots dir", names[at]);
            failed = 1;
        }
    }
    for (size_t shot = 0; shot < cfg->shot_count && !failed; shot++) {
        char wanted[BT_RV_SHOT_NAME_MAX + 8];
        snprintf(wanted, sizeof wanted, "%s.png", cfg->shots[shot]);
        int matched = 0;
        for (size_t at = 0; at < name_count; at++) {
            if (strcmp(names[at], wanted) == 0) {
                matched = 1;
                break;
            }
        }
        if (!matched) {
            snprintf(err, err_cap, "missing shot %s", wanted);
            failed = 1;
        }
    }
    for (size_t at = 0; at < name_count && !failed; at++) {
        char path[BT_PATH_MAX];
        rv_join(path, dir, names[at]);
        unsigned int shot_width = 0;
        unsigned int shot_height = 0;
        char inner[160];
        if (bt_rv_png_check(path, &shot_width, &shot_height, inner, sizeof inner) != 0) {
            snprintf(err, err_cap, "shot %s is not a decodable image: %s", names[at], inner);
            failed = 1;
            break;
        }
        if (shot_width != width || shot_height != height) {
            snprintf(err, err_cap, "shot %s is %ux%u, expected %ux%u", names[at], shot_width,
                     shot_height, width, height);
            failed = 1;
            break;
        }
    }
    if (!failed) {
        *count = name_count;
    }
    for (size_t at = 0; at < name_count; at++) {
        free(names[at]);
    }
    free(names);
    if (failed) {
        BT_TRACE_FAIL("rv.check_shot_dir", "%s", err);
        BT_TRACE_EXIT("rv.check_shot_dir");
        return -1;
    }
    BT_TRACE_EXIT("rv.check_shot_dir");
    return 0;
}

int bt_rv_smoke_stage(const char *exe, const char *sandbox_root, const bt_rv_release_cfg *cfg,
                      unsigned int width, unsigned int height, const char *const vars[],
                      const char *bt_config, size_t *shot_count, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.smoke_stage");
    char cwd[BT_PATH_MAX];
    char appdata[BT_PATH_MAX];
    rv_prep_sandbox(sandbox_root, cwd, appdata, err, err_cap);
    if (err[0] != '\0') {
        BT_TRACE_FAIL("rv.smoke_stage", "%s", err);
        BT_TRACE_EXIT("rv.smoke_stage");
        return -1;
    }
    char shots[BT_PATH_MAX];
    rv_join(shots, sandbox_root, "shots");
    const char *args[] = {"--smoke", shots, NULL};
    bt_rv_env_pair extras[2];
    size_t extra_count = 1;
    extras[0].key = "APPDATA";
    extras[0].value = appdata;
    if (bt_config != NULL) {
        extras[1].key = "BT_CONFIG";
        extras[1].value = bt_config;
        extra_count = 2;
    }
    bt_rv_child_spec spec;
    bt_rv_stage_spec(exe, args, cwd, vars, extras, extra_count, &spec);
    bt_rv_run_result result;
    bt_rv_run_child(&spec, cfg->run_timeout_secs * 1000, &result);
    bt_rv_child_spec_free(&spec);
    if (bt_rv_check_exit(&result.outcome, 0, err, err_cap) != 0) {
        bt_rv_run_result_free(&result);
        BT_TRACE_FAIL("rv.smoke_stage", "%s", err);
        BT_TRACE_EXIT("rv.smoke_stage");
        return -1;
    }
    bt_rv_run_result_free(&result);
    if (bt_rv_check_shot_dir(shots, cfg, width, height, shot_count, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.smoke_stage", "%s", err);
        BT_TRACE_EXIT("rv.smoke_stage");
        return -1;
    }
    BT_TRACE_EXIT("rv.smoke_stage");
    return 0;
}

int bt_rv_n1_spec(const char *exe, const char *extract_dir, const char *sandbox_root,
                  const bt_rv_release_cfg *cfg, const char *const vars[], bt_rv_child_spec *out,
                  char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.n1_spec");
    char missing[BT_PATH_MAX];
    rv_join(missing, extract_dir, cfg->config_file);
    if (!DeleteFileA(missing)) {
        snprintf(err, err_cap, "cannot delete extracted config %s: %lu", missing, GetLastError());
        BT_TRACE_FAIL("rv.n1_spec", "%s", err);
        BT_TRACE_EXIT("rv.n1_spec");
        return -1;
    }
    char cwd[BT_PATH_MAX];
    char appdata[BT_PATH_MAX];
    rv_prep_sandbox(sandbox_root, cwd, appdata, err, err_cap);
    if (err[0] != '\0') {
        BT_TRACE_FAIL("rv.n1_spec", "%s", err);
        BT_TRACE_EXIT("rv.n1_spec");
        return -1;
    }
    char shots[BT_PATH_MAX];
    rv_join(shots, sandbox_root, "shots");
    const char *args[] = {"--smoke", shots, NULL};
    bt_rv_env_pair extras[] = {
        {.key = "APPDATA", .value = appdata},
        {.key = "BT_CONFIG", .value = missing},
    };
    bt_rv_stage_spec(exe, args, cwd, vars, extras, 2, out);
    BT_TRACE_EXIT("rv.n1_spec");
    return 0;
}

int bt_rv_n1_stage(const char *exe, const char *extract_dir, const char *sandbox_root,
                   const bt_rv_release_cfg *cfg, const char *const vars[], char *err,
                   size_t err_cap) {
    BT_TRACE_ENTER("rv.n1_stage");
    bt_rv_child_spec spec;
    if (bt_rv_n1_spec(exe, extract_dir, sandbox_root, cfg, vars, &spec, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.n1_stage", "%s", err);
        BT_TRACE_EXIT("rv.n1_stage");
        return -1;
    }
    bt_rv_run_result result;
    bt_rv_run_child(&spec, cfg->run_timeout_secs * 1000, &result);
    bt_rv_child_spec_free(&spec);
    if (bt_rv_check_exit(&result.outcome, cfg->neg_config_exit, err, err_cap) != 0) {
        bt_rv_run_result_free(&result);
        BT_TRACE_FAIL("rv.n1_stage", "%s", err);
        BT_TRACE_EXIT("rv.n1_stage");
        return -1;
    }
    bt_rv_run_result_free(&result);
    BT_TRACE_EXIT("rv.n1_stage");
    return 0;
}

int bt_rv_n2_stage(const char *exe, const char *sandbox_root, const bt_rv_release_cfg *cfg,
                   const char *const vars[], char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.n2_stage");
    char cwd[BT_PATH_MAX];
    char appdata[BT_PATH_MAX];
    rv_prep_sandbox(sandbox_root, cwd, appdata, err, err_cap);
    if (err[0] != '\0') {
        BT_TRACE_FAIL("rv.n2_stage", "%s", err);
        BT_TRACE_EXIT("rv.n2_stage");
        return -1;
    }
    const char *args[] = {"--smoke", NULL};
    bt_rv_env_pair extras[] = {{.key = "APPDATA", .value = appdata}};
    bt_rv_child_spec spec;
    bt_rv_stage_spec(exe, args, cwd, vars, extras, 1, &spec);
    bt_rv_run_result result;
    bt_rv_run_child(&spec, cfg->run_timeout_secs * 1000, &result);
    bt_rv_child_spec_free(&spec);
    if (bt_rv_check_exit(&result.outcome, cfg->usage_exit, err, err_cap) != 0) {
        bt_rv_run_result_free(&result);
        BT_TRACE_FAIL("rv.n2_stage", "%s", err);
        BT_TRACE_EXIT("rv.n2_stage");
        return -1;
    }
    bt_rv_run_result_free(&result);
    BT_TRACE_EXIT("rv.n2_stage");
    return 0;
}

static int rv_stage_ok(const char *name, int rc, const char *detail) {
    if (rc == 0) {
        printf("ok: %s (%s)\n", name, detail);
        return 0;
    }
    fprintf(stderr, "fail: %s: %s\n", name, detail);
    return BT_RV_EXIT_FAIL;
}

static int rv_zip_stage(const char *staging, const char *zip, const char *root,
                        const bt_rv_release_cfg *cfg, unsigned int dims[2], char *detail,
                        size_t detail_cap, char *err, size_t err_cap) {
    bt_rv_file_set set;
    if (bt_rv_staging_file_set(staging, cfg->exe, cfg->config_file, cfg->fonts_dir, &set, err,
                               err_cap) != 0) {
        return -1;
    }
    size_t count = 0;
    if (bt_rv_check_zip(zip, root, &set, &count, err, err_cap) != 0) {
        bt_rv_file_set_free(&set);
        return -1;
    }
    bt_rv_file_set_free(&set);
    if (bt_rv_packaged_window_dims(staging, cfg->config_file, &dims[0], &dims[1], err, err_cap) !=
        0) {
        return -1;
    }
    snprintf(detail, detail_cap, "%zu files byte-identical, window %ux%u", count, dims[0], dims[1]);
    return 0;
}

static int rv_extract_stage(const char *zip, const char *dest, const char *root, size_t *count,
                            char *detail, size_t detail_cap, char *err, size_t err_cap) {
    rv_mkdir_p(dest, err, err_cap);
    if (err[0] != '\0') {
        return -1;
    }
    if (bt_rv_extract_zip(zip, dest, root, count, err, err_cap) != 0) {
        return -1;
    }
    snprintf(detail, detail_cap, "%zu files at %s", *count, dest);
    return 0;
}

static void rv_work_root(char out[BT_PATH_MAX]) {
    char temp[BT_PATH_MAX];
    GetTempPathA(sizeof temp, temp);
    size_t len = strlen(temp);
    if (len > 0 && temp[len - 1] == '\\') {
        temp[len - 1] = '\0';
    }
    unsigned long sequence = rv_work_counter++;
    snprintf(out, BT_PATH_MAX, "%s/%s/%lu-%lu", temp, RV_WORK_ROOT_NAME, GetCurrentProcessId(),
             sequence);
}

static int rv_verify(const bt_rv_env *env, const bt_rv_parsed_args *args,
                     const bt_rv_release_cfg *cfg, const char *root) {
    BT_TRACE_ENTER("rv.verify");
    char detail[BT_RV_ERR_MAX];
    char err[BT_RV_ERR_MAX];
    unsigned int dims[2] = {0, 0};
    if (rv_zip_stage(args->staging, args->zip, root, cfg, dims, detail, sizeof detail, err,
                     sizeof err) != 0) {
        int code = rv_stage_ok("zip check", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    int code = rv_stage_ok("zip check", 0, detail);
    if (!args->run) {
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    char work[BT_PATH_MAX];
    rv_work_root(work);
    char extract[BT_PATH_MAX];
    rv_join(extract, work, "extract");
    size_t count = 0;
    if (rv_extract_stage(args->zip, extract, root, &count, detail, sizeof detail, err, sizeof err) !=
        0) {
        code = rv_stage_ok("extract", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    code = rv_stage_ok("extract", 0, detail);
    char exe_path[BT_PATH_MAX];
    rv_join(exe_path, extract, cfg->exe);
    char sandbox[BT_PATH_MAX];
    rv_join(sandbox, work, "p1");
    size_t shot_count = 0;
    if (bt_rv_smoke_stage(exe_path, sandbox, cfg, dims[0], dims[1], env->vars, NULL, &shot_count,
                          err, sizeof err) != 0) {
        code = rv_stage_ok("p1 smoke", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    snprintf(detail, sizeof detail, "exit 0, %zu shots at %ux%u", shot_count, dims[0], dims[1]);
    code = rv_stage_ok("p1 smoke", 0, detail);
    if (bt_rv_db_stage(sandbox, cfg, err, sizeof err) != 0) {
        code = rv_stage_ok("p1 db", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    snprintf(detail, sizeof detail, "%s at user_version %llu", cfg->db_file,
             BT_RV_LATEST_USER_VERSION);
    code = rv_stage_ok("p1 db", 0, detail);
    char packaged[BT_PATH_MAX];
    rv_join(packaged, extract, cfg->config_file);
    char p2[BT_PATH_MAX];
    rv_join(p2, work, "p2");
    shot_count = 0;
    if (bt_rv_smoke_stage(exe_path, p2, cfg, dims[0], dims[1], env->vars, packaged, &shot_count,
                          err, sizeof err) != 0) {
        code = rv_stage_ok("p2 smoke", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    snprintf(detail, sizeof detail, "exit 0, %zu shots at %ux%u", shot_count, dims[0], dims[1]);
    code = rv_stage_ok("p2 smoke", 0, detail);
    char n1[BT_PATH_MAX];
    rv_join(n1, work, "n1");
    if (bt_rv_n1_stage(exe_path, extract, n1, cfg, env->vars, err, sizeof err) != 0) {
        code = rv_stage_ok("n1 missing config", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    snprintf(detail, sizeof detail, "exit %d with the config deleted", cfg->neg_config_exit);
    code = rv_stage_ok("n1 missing config", 0, detail);
    char n2[BT_PATH_MAX];
    rv_join(n2, work, "n2");
    if (bt_rv_n2_stage(exe_path, n2, cfg, env->vars, err, sizeof err) != 0) {
        code = rv_stage_ok("n2 usage", -1, err);
        BT_TRACE_EXIT("rv.verify");
        return code;
    }
    snprintf(detail, sizeof detail, "exit %d", cfg->usage_exit);
    code = rv_stage_ok("n2 usage", 0, detail);
    BT_TRACE_EXIT("rv.verify");
    return code;
}

static int rv_read_config(const char *path, bt_json_value **out, char *err, size_t err_cap) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "cannot read config %s: %s", path, strerror(errno));
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > 8 * 1024 * 1024) {
        fclose(file);
        snprintf(err, err_cap, "cannot read config %s: too large", path);
        return -1;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL || ((size_t)size > 0 && fread(text, 1, (size_t)size, file) != (size_t)size)) {
        free(text);
        fclose(file);
        snprintf(err, err_cap, "cannot read config %s: short read", path);
        return -1;
    }
    fclose(file);
    text[size] = '\0';
    bt_json_error json_err;
    if (bt_json_parse(text, (size_t)size, out, &json_err) != 0) {
        free(text);
        snprintf(err, err_cap, "cannot parse config %s: %s at line %zu", path, json_err.message,
                 json_err.line);
        return -1;
    }
    free(text);
    return 0;
}

static size_t rv_argv_count(const char *const argv[]) {
    size_t count = 0;
    for (size_t at = 0; argv != NULL && argv[at] != NULL; at++) {
        count++;
    }
    return count;
}

int bt_rv_run(const bt_rv_env *env, int *exit_code, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.run");
    bt_rv_parsed_args args;
    if (bt_rv_parse_args((int)rv_argv_count(env->argv), env->argv, &args, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.run", "%s", err);
        BT_TRACE_EXIT("rv.run");
        return -1;
    }
    char config_path[BT_PATH_MAX];
    bt_paths_config_path_from(env->config, env->root, config_path);
    bt_json_value *config = NULL;
    if (rv_read_config(config_path, &config, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.run", "%s", err);
        BT_TRACE_EXIT("rv.run");
        return -1;
    }
    bt_rv_release_cfg cfg;
    int rc = bt_rv_release_cfg_from(config, &cfg, err, err_cap);
    bt_json_free(config);
    if (rc != 0) {
        BT_TRACE_FAIL("rv.run", "%s", err);
        BT_TRACE_EXIT("rv.run");
        return -1;
    }
    char root[BT_PATH_MAX];
    if (bt_rv_zip_root_from(args.staging, root, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.run", "%s", err);
        BT_TRACE_EXIT("rv.run");
        return -1;
    }
    *exit_code = rv_verify(env, &args, &cfg, root);
    BT_TRACE_EXIT("rv.run");
    return 0;
}
