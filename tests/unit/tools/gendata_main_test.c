#include "gendata/lib.h"
#include "gendata/model.h"

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "unit/tools/spawn_util.h"

static char g_config_text[1U << 18U];
static char g_repo_root[BT_PATH_MAX];

static void ensure_root(void) {
    if (g_repo_root[0] == '\0') {
        bt_test_repo_root(g_repo_root);
    }
}

static const char *hub_config_path(void) {
    static char path[BT_PATH_MAX];
    ensure_root();
    const char *env = getenv("BT_CONFIG");
    if (env != NULL && env[0] != '\0') {
        snprintf(path, sizeof path, "%s", env);
        return path;
    }
    snprintf(path, sizeof path, "%s/config/config.json", g_repo_root);
    return path;
}

static bt_json_value *hub_config(void) {
    FILE *file = fopen(hub_config_path(), "rb");
    BT_CHECK(file != NULL);
    size_t got = fread(g_config_text, 1, sizeof g_config_text - 1, file);
    fclose(file);
    g_config_text[got] = '\0';
    bt_json_error jerr;
    bt_json_value *config = NULL;
    BT_CHECK_EQ(bt_json_parse(g_config_text, strlen(g_config_text), &config, &jerr), 0);
    return config;
}

static void write_config(bt_json_value *config, const char *path) {
    bt_strbuf text;
    bt_strbuf_init(&text, 0);
    bt_gd_json_pretty(config, &text);
    FILE *file = fopen(path, "wb");
    BT_CHECK(file != NULL);
    fwrite(text.data, 1, text.len, file);
    fclose(file);
    bt_strbuf_free(&text);
}

static void fixture_dir(char out[BT_PATH_MAX], const char *name) {
    ensure_root();
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/gdmain/%s", g_repo_root, name);
    bt_test_mkdirs(out);
}

static void set_paths(bt_json_value *config, const char *vendor, int with_ground) {
    bt_json_value *paths = bt_json_new_object();
    if (with_ground) {
        bt_json_object_set(paths, "ref_ground", bt_json_new_string("ref_ground"));
    }
    bt_json_object_set(paths, "data_vendor", bt_json_new_string(vendor));
    bt_json_object_set(config, "paths", paths);
}

static void shrink_budget(bt_json_value *config) {
    bt_json_value *section = bt_json_get(config, "synth");
    bt_json_object_set(section, "max_setup", bt_json_new_integer(3));
    bt_json_object_set(section, "max_insert", bt_json_new_integer(4));
    bt_json_object_set(section, "node_budget", bt_json_new_integer(1000));
}

static void spawn_gendata(const char *config, const char *ground, int *code, char *out,
                          size_t out_cap, char *err, size_t err_cap) {
    ensure_root();
    static char exe[BT_PATH_MAX];
    static char prior_config[BT_PATH_MAX];
    static char prior_ground[BT_PATH_MAX];
    if (exe[0] == '\0') {
        bt_test_bin_path(exe, "gendata.exe");
        const char *config_env = getenv("BT_CONFIG");
        const char *ground_env = getenv("BT_REF_GROUND");
        snprintf(prior_config, sizeof prior_config, "%s", config_env != NULL ? config_env : "");
        snprintf(prior_ground, sizeof prior_ground, "%s", ground_env != NULL ? ground_env : "");
    }
    _putenv_s("BT_CONFIG", config);
    if (ground != NULL) {
        _putenv_s("BT_REF_GROUND", ground);
    } else {
        _putenv_s("BT_REF_GROUND", "");
    }
    _putenv_s("BT_ARTIFACTS", "artifacts");
    const char *argv[] = {NULL};
    int rc = bt_spawn_capture(exe, argv, g_repo_root, 120000, code, out, out_cap, err, err_cap);
    _putenv_s("BT_CONFIG", prior_config);
    _putenv_s("BT_REF_GROUND", prior_ground);
    if (rc != 0) {
        char text[320];
        snprintf(text, sizeof text, "spawn failed rc=%d diag=%s", rc, bt_spawn_diag());
        BT_ABORT("%s", text);
    }
}

BT_TEST(gendata_main, unreadable_config_exits_one_with_the_reason) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "bad-config");
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/absent.json", dir);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, "ref_ground", &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "gendata: config load failed") != NULL);
}

BT_TEST(gendata_main, synthless_config_exits_one_naming_the_section) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "no-synth");
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    bt_json_value *hub = hub_config();
    set_paths(hub, "data/vendor", 1);
    bt_json_object_set(hub, "synth", bt_json_new_null());
    write_config(hub, config);
    bt_json_free(hub);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, "ref_ground", &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "gendata: config section missing: synth") != NULL);
}

BT_TEST(gendata_main, groundless_config_exits_one_naming_the_path) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "no-ground");
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    bt_json_value *hub = hub_config();
    set_paths(hub, "data/vendor", 0);
    shrink_budget(hub);
    write_config(hub, config);
    bt_json_free(hub);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, NULL, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "gendata: config section missing: paths.ref_ground") != NULL);
}

BT_TEST(gendata_main, empty_ref_ground_root_exits_one) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "empty-ground");
    char ground[BT_PATH_MAX];
    snprintf(ground, sizeof ground, "%s/ground", dir);
    bt_test_mkdirs(ground);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    bt_json_value *hub = hub_config();
    set_paths(hub, "data/vendor", 1);
    shrink_budget(hub);
    write_config(hub, config);
    bt_json_free(hub);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, ground, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK_EQ(strstr(err, "gendata: "), err);
}

static void rm_tree(const char *path) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddatai64_t entry;
    intptr_t handle = _findfirsti64(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            if (entry.attrib & _A_SUBDIR) {
                rm_tree(child);
            } else {
                remove(child);
            }
        } while (_findnexti64(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
}

BT_TEST(gendata_main, fresh_vendor_writes_outputs_then_skips_on_rerun) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "fresh-vendor");
    char vendor[BT_PATH_MAX];
    snprintf(vendor, sizeof vendor, "%s/vendor", dir);
    rm_tree(vendor);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    bt_json_value *hub = hub_config();
    set_paths(hub, vendor, 1);
    shrink_budget(hub);
    write_config(hub, config);
    bt_json_free(hub);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, "ref_ground", &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "gendata: 818 cases (") != NULL);
    char manifest[BT_PATH_MAX];
    char cases[BT_PATH_MAX];
    char specials[BT_PATH_MAX];
    snprintf(manifest, sizeof manifest, "%s/manifest.json", vendor);
    snprintf(cases, sizeof cases, "%s/cases.json", vendor);
    snprintf(specials, sizeof specials, "%s/specials.json", vendor);
    BT_CHECK(fopen(manifest, "rb") != NULL);
    BT_CHECK(fopen(cases, "rb") != NULL);
    BT_CHECK(fopen(specials, "rb") != NULL);
    spawn_gendata(config, "ref_ground", &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "skip regeneration") != NULL);
}

BT_TEST(gendata_main, vendor_path_under_a_file_fails_the_write) {
    char dir[BT_PATH_MAX];
    fixture_dir(dir, "blocked-vendor");
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker.txt", dir);
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    fwrite("x", 1, 1, file);
    fclose(file);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/config.json", dir);
    char vendor[BT_PATH_MAX];
    snprintf(vendor, sizeof vendor, "%s/blocker.txt/vendor", dir);
    bt_json_value *hub = hub_config();
    set_paths(hub, vendor, 1);
    shrink_budget(hub);
    write_config(hub, config);
    bt_json_free(hub);
    int code = -1;
    char out[512];
    char err[512];
    spawn_gendata(config, "ref_ground", &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "mkdir failed") != NULL);
}
