#include "release-verify/lib.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "unit/tools/release_verify_test_util.h"

#define RV_SPAWN_EXE "blind-trainer.bat"
#define RV_SPAWN_CONFIG_FILE "config/config.json"
#define RV_SPAWN_FONTS_DIR "assets/fonts"
#define RV_SPAWN_SHOT_A "menu"
#define RV_SPAWN_SHOT_B "drill"
#define RV_SPAWN_DB_NAME "db.bin"

enum {
    RV_SPAWN_SHOT_W = 1280,
    RV_SPAWN_SHOT_H = 800,
    RV_SPAWN_EXIT_OK = 0,
    RV_SPAWN_EXIT_NEG = 1,
    RV_SPAWN_EXIT_USAGE = 2,
    RV_SPAWN_EXIT_APP = 3,
    RV_SPAWN_TIMEOUT_SHORT = 1,
    RV_SPAWN_TIMEOUT_LONG = 120,
    RV_SPAWN_BAT_MAX = 2048,
    RV_SPAWN_ZIP_FILES_MAX = 16,
    RV_SPAWN_DB_BYTES = 100,
    RV_SPAWN_CFG_CAP = 8 * 1024 * 1024,
    RV_SPAWN_CFG_CHUNK = 4096,
};

static void spawn_cfg_json(char *out, size_t cap, unsigned long long timeout, int neg, int usage) {
    snprintf(out, cap,
             "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"%s\", \"config_file\": \"%s\", "
             "\"fonts_dir\": \"%s\", \"run_timeout_secs\": %llu, \"neg_config_exit\": %d, "
             "\"usage_exit\": %d}, \"e2e\": {\"shots\": [\"%s\", \"%s\"]}}",
             RV_SPAWN_EXE, RV_SPAWN_CONFIG_FILE, RV_SPAWN_FONTS_DIR, timeout, neg, usage,
             RV_SPAWN_SHOT_A, RV_SPAWN_SHOT_B);
}

static void parse_spawn_cfg(const char *text, bt_rv_release_cfg *cfg) {
    bt_json_value *value = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, strlen(text), &value, &json_err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn fixture config did not parse");
        return;
    }
    char err[BT_RV_ERR_MAX];
    if (bt_rv_release_cfg_from(value, cfg, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn fixture config rejected: %s", err);
    }
    bt_json_free(value);
}

static void spawn_cfg(unsigned long long timeout, int neg, int usage, bt_rv_release_cfg *cfg) {
    char text[512];
    spawn_cfg_json(text, sizeof text, timeout, neg, usage);
    parse_spawn_cfg(text, cfg);
}

typedef struct {
    char text[RV_SPAWN_BAT_MAX];
    size_t used;
} rv_bat;

static void bat_line(rv_bat *bat, const char *line) {
    if (bat->used < sizeof bat->text) {
        size_t wrote =
            (size_t)snprintf(bat->text + bat->used, sizeof bat->text - bat->used, "%s", line);
        bat->used +=
            wrote < sizeof bat->text - bat->used ? wrote : sizeof bat->text - bat->used - 1;
    }
}

static void bat_norm_line(rv_bat *bat, const char *var) {
    char line[96];
    snprintf(line, sizeof line, "set \"%s=%%%s:/=\\%%\"\r\n", var, var);
    bat_line(bat, line);
}

static void bat_shot_copy(rv_bat *bat, const char *shot) {
    char line[160];
    snprintf(line, sizeof line, "copy /y \"%%HERE%%%s\\%s.png\" \"%%SHOTS%%\\%s.png\" >nul\r\n",
             RV_SPAWN_FONTS_DIR, shot, shot);
    bat_line(bat, line);
}

static void bat_build(rv_bat *bat, int with_shots, int with_db, int reject_config) {
    bat->used = 0;
    bat->text[0] = '\0';
    bat_line(bat, "@echo off\r\n");
    bat_line(bat, "if \"%~2\"==\"\" exit /b 2\r\n");
    if (reject_config) {
        char line[64];
        snprintf(line, sizeof line, "if defined BT_CONFIG exit /b %d\r\n", RV_SPAWN_EXIT_APP);
        bat_line(bat, line);
    }
    bat_line(bat, "if not defined BT_CONFIG goto smoke\r\n");
    bat_line(bat, "set \"CFG=%BT_CONFIG%\"\r\n");
    bat_norm_line(bat, "CFG");
    char line[96];
    snprintf(line, sizeof line, "if not exist \"%%CFG%%\" exit /b %d\r\n", RV_SPAWN_EXIT_NEG);
    bat_line(bat, line);
    bat_line(bat, ":smoke\r\n");
    bat_line(bat, "set \"SHOTS=%~2\"\r\n");
    bat_norm_line(bat, "SHOTS");
    bat_line(bat, "set \"HERE=%~dp0\"\r\n");
    bat_norm_line(bat, "HERE");
    bat_line(bat, "set \"DATA=%APPDATA%\"\r\n");
    bat_norm_line(bat, "DATA");
    if (with_shots) {
        bat_line(bat, "mkdir \"%SHOTS%\" 2>nul\r\n");
        bat_shot_copy(bat, RV_SPAWN_SHOT_A);
        bat_shot_copy(bat, RV_SPAWN_SHOT_B);
    }
    if (with_db) {
        snprintf(line, sizeof line, "mkdir \"%%DATA%%\\%s\" 2>nul\r\n", BT_APP_DIR_NAME);
        bat_line(bat, line);
        snprintf(line, sizeof line, "copy /y \"%%HERE%%%s\\%s\" \"%%DATA%%\\%s\\%s\" >nul\r\n",
                 RV_SPAWN_FONTS_DIR, RV_SPAWN_DB_NAME, BT_APP_DIR_NAME, BT_RV_DB_FILE_FALLBACK);
        bat_line(bat, line);
    }
    bat_line(bat, "exit /b 0\r\n");
}

static void bat_tiny(rv_bat *bat, const char *tail) {
    bat->used = 0;
    bat->text[0] = '\0';
    bat_line(bat, "@echo off\r\n");
    bat_line(bat, tail);
}

static void write_fake_db(const char *path) {
    unsigned char bytes[RV_SPAWN_DB_BYTES];
    memset(bytes, 0, sizeof bytes);
    memcpy(bytes, "SQLite format 3\0", 16);
    unsigned int version = (unsigned int)BT_RV_LATEST_USER_VERSION;
    bytes[60] = (unsigned char)(version >> 24U);
    bytes[61] = (unsigned char)(version >> 16U);
    bytes[62] = (unsigned char)(version >> 8U);
    bytes[63] = (unsigned char)version;
    rv_util_write_bytes(path, bytes, sizeof bytes);
}

static void make_spawn_staging(const char *staging, const rv_bat *bat) {
    char config_dir[BT_PATH_MAX];
    char fonts[BT_PATH_MAX];
    char path[BT_PATH_MAX];
    rv_util_join(config_dir, staging, "config");
    rv_util_mkdir_p(config_dir);
    rv_util_join(fonts, staging, RV_SPAWN_FONTS_DIR);
    rv_util_mkdir_p(fonts);
    rv_util_join(path, staging, RV_SPAWN_EXE);
    rv_util_write(path, bat->text);
    rv_util_join(path, staging, RV_SPAWN_CONFIG_FILE);
    char window[128];
    snprintf(window, sizeof window, "{\"app\": {\"window\": {\"width\": %d, \"height\": %d}}}",
             RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H);
    rv_util_write(path, window);
    char shot_a[BT_PATH_MAX];
    char shot_b[BT_PATH_MAX];
    rv_util_join(shot_a, fonts, RV_SPAWN_SHOT_A ".png");
    rv_util_join(shot_b, fonts, RV_SPAWN_SHOT_B ".png");
    rv_util_write_png(shot_a, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H);
    rv_util_write_png(shot_b, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H);
    rv_util_join(path, fonts, RV_SPAWN_DB_NAME);
    write_fake_db(path);
}

typedef struct {
    rv_zip_entry entries[RV_SPAWN_ZIP_FILES_MAX];
    char names[RV_SPAWN_ZIP_FILES_MAX][BT_PATH_MAX];
    unsigned char *blocks[RV_SPAWN_ZIP_FILES_MAX];
    size_t count;
} rv_spawn_zip;

static void spawn_zip_collect(rv_spawn_zip *zip, const char *staging, const char *dir) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        bt_harness_fail(__FILE__, __LINE__, "cannot walk %s", dir);
        return;
    }
    const char *root = strrchr(staging, '/') + 1;
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char path[BT_PATH_MAX];
        rv_util_join(path, dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            spawn_zip_collect(zip, staging, path);
            continue;
        }
        if (zip->count >= RV_SPAWN_ZIP_FILES_MAX) {
            bt_harness_fail(__FILE__, __LINE__, "spawn staging holds too many files");
            break;
        }
        unsigned char *data = NULL;
        size_t len = 0;
        if (rv_util_file_bytes(path, &data, &len) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "cannot read %s", path);
            break;
        }
        size_t at = zip->count;
        snprintf(zip->names[at], sizeof zip->names[at], "%s/%s", root, path + strlen(staging) + 1);
        zip->entries[at].name = zip->names[at];
        zip->entries[at].data = data;
        zip->entries[at].len = len;
        zip->blocks[at] = data;
        zip->count++;
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
}

static void write_spawn_zip(const char *zip_path, const char *staging) {
    rv_spawn_zip zip = {0};
    spawn_zip_collect(&zip, staging, staging);
    rv_util_write_zip(zip_path, zip.entries, zip.count);
    for (size_t at = 0; at < zip.count; at++) {
        free(zip.blocks[at]);
    }
}

static int run_spawn_chain(const char *tag, const rv_bat *bat, unsigned long long timeout, int neg,
                           int usage, int with_run) {
    char dir[BT_PATH_MAX];
    rv_util_scratch(tag, dir);
    char staging[BT_PATH_MAX];
    rv_util_join(staging, dir, "blind-trainer");
    make_spawn_staging(staging, bat);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "dist.zip");
    write_spawn_zip(zip, staging);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    char text[512];
    spawn_cfg_json(text, sizeof text, timeout, neg, usage);
    rv_util_write(config, text);
    const char *argv[4] = {staging, zip, NULL, NULL};
    if (with_run) {
        argv[2] = "--run";
    }
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = dir;
    int code = 0;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_run(&env, &code, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_spawn_chain %s failed: %s", tag, err);
        return -1;
    }
    return code;
}

static void spawn_app_tree(const char *tag, const rv_bat *bat, char staging[BT_PATH_MAX],
                           char exe[BT_PATH_MAX]) {
    char dir[BT_PATH_MAX];
    rv_util_scratch(tag, dir);
    rv_util_join(staging, dir, "blind-trainer");
    make_spawn_staging(staging, bat);
    rv_util_join(exe, staging, RV_SPAWN_EXE);
}

BT_TEST(release_verify, run_completes_every_spawn_stage_with_a_batch_app) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    BT_CHECK_EQ(run_spawn_chain("chain-green", &bat, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                                RV_SPAWN_EXIT_USAGE, 1),
                RV_SPAWN_EXIT_OK);
}

BT_TEST(release_verify, run_times_out_the_first_smoke_stage_when_the_app_hangs) {
    rv_bat bat;
    bat_tiny(&bat, "if \"%~2\"==\"\" exit /b 2\r\n:spin\r\ngoto spin\r\n");
    BT_CHECK_EQ(run_spawn_chain("chain-timeout", &bat, RV_SPAWN_TIMEOUT_SHORT, RV_SPAWN_EXIT_NEG,
                                RV_SPAWN_EXIT_USAGE, 1),
                BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_fails_the_second_smoke_stage_when_the_app_rejects_a_config) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 1);
    BT_CHECK_EQ(run_spawn_chain("chain-p2", &bat, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                                RV_SPAWN_EXIT_USAGE, 1),
                BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_reports_the_db_stage_when_the_app_writes_no_database) {
    rv_bat bat;
    bat_build(&bat, 1, 0, 0);
    BT_CHECK_EQ(run_spawn_chain("chain-nodb", &bat, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                                RV_SPAWN_EXIT_USAGE, 1),
                BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_fails_the_n1_stage_when_the_neg_exit_code_is_wrong) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    BT_CHECK_EQ(run_spawn_chain("chain-n1", &bat, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_OK,
                                RV_SPAWN_EXIT_USAGE, 1),
                BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_fails_the_n2_stage_when_the_usage_exit_code_is_wrong) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    BT_CHECK_EQ(run_spawn_chain("chain-n2", &bat, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                                RV_SPAWN_EXIT_OK, 1),
                BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, smoke_stage_accepts_a_batch_app_with_and_without_a_config) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("smoke-green", &bat, staging, exe);
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    char config[BT_PATH_MAX];
    rv_util_join(config, staging, RV_SPAWN_CONFIG_FILE);
    char err[BT_RV_ERR_MAX];
    size_t shots = 0;
    BT_CHECK_EQ(bt_rv_smoke_stage(exe, staging, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, NULL, NULL,
                                  &shots, err, sizeof err),
                0);
    BT_CHECK_EQ(shots, 2U);
    shots = 0;
    BT_CHECK_EQ(bt_rv_smoke_stage(exe, staging, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, NULL,
                                  config, &shots, err, sizeof err),
                0);
    BT_CHECK_EQ(shots, 2U);
}

BT_TEST(release_verify, smoke_stage_reports_a_nonzero_app_exit) {
    rv_bat bat;
    bat_tiny(&bat, "exit /b 3\r\n");
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("smoke-exit", &bat, staging, exe);
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    size_t shots = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_smoke_stage(exe, staging, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, NULL, NULL,
                               &shots, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "exit code 3, expected 0") != NULL);
}

BT_TEST(release_verify, smoke_stage_reports_a_missing_shots_dir) {
    rv_bat bat;
    bat_tiny(&bat, "exit /b 0\r\n");
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("smoke-noop", &bat, staging, exe);
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    size_t shots = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_smoke_stage(exe, staging, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, NULL, NULL,
                               &shots, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot read shots dir") != NULL);
}

BT_TEST(release_verify, smoke_stage_names_a_blocked_sandbox_root) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("smoke-blocked", &bat, staging, exe);
    char blocker[BT_PATH_MAX];
    rv_util_join(blocker, staging, "blocker");
    rv_util_write(blocker, "in the way");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    size_t shots = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_smoke_stage(exe, blocker, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, NULL, NULL,
                               &shots, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "mkdir") != NULL);
}

BT_TEST(release_verify, n1_stage_passes_when_the_app_exits_with_the_neg_code) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("n1-green", &bat, staging, exe);
    char sandbox[BT_PATH_MAX];
    rv_util_join(sandbox, staging, "sandbox");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_n1_stage(exe, staging, sandbox, &cfg, NULL, err, sizeof err), 0);
    char config[BT_PATH_MAX];
    rv_util_join(config, staging, RV_SPAWN_CONFIG_FILE);
    BT_CHECK(!rv_util_exists(config));
}

BT_TEST(release_verify, n1_stage_rejects_an_unexpected_neg_exit_code) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("n1-mismatch", &bat, staging, exe);
    char sandbox[BT_PATH_MAX];
    rv_util_join(sandbox, staging, "sandbox");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_OK, RV_SPAWN_EXIT_USAGE, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_n1_stage(exe, staging, sandbox, &cfg, NULL, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "exit code 1, expected 0") != NULL);
}

BT_TEST(release_verify, n2_stage_passes_when_the_app_exits_with_the_usage_code) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("n2-green", &bat, staging, exe);
    char sandbox[BT_PATH_MAX];
    rv_util_join(sandbox, staging, "sandbox");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_n2_stage(exe, sandbox, &cfg, NULL, err, sizeof err), 0);
}

BT_TEST(release_verify, n2_stage_rejects_an_unexpected_usage_exit_code) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("n2-mismatch", &bat, staging, exe);
    char sandbox[BT_PATH_MAX];
    rv_util_join(sandbox, staging, "sandbox");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_OK, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_n2_stage(exe, sandbox, &cfg, NULL, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "exit code 2, expected 0") != NULL);
}

BT_TEST(release_verify, n1_spec_and_n2_stage_name_a_blocked_sandbox_root) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("stage-blocked", &bat, staging, exe);
    char blocker[BT_PATH_MAX];
    rv_util_join(blocker, staging, "blocker");
    rv_util_write(blocker, "in the way");
    bt_rv_release_cfg cfg;
    spawn_cfg(RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, &cfg);
    bt_rv_child_spec spec;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_n1_spec(exe, staging, blocker, &cfg, NULL, &spec, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "mkdir") != NULL);
    BT_CHECK(bt_rv_n2_stage(exe, blocker, &cfg, NULL, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "mkdir") != NULL);
}

BT_TEST(release_verify, check_shot_dir_rejects_an_undecodable_shot) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("shots-junk", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, RV_SPAWN_SHOT_A ".png");
    rv_util_write(path, "not a png");
    char text[512];
    snprintf(text, sizeof text,
             "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"%s\", \"config_file\": \"%s\", "
             "\"fonts_dir\": \"%s\", \"run_timeout_secs\": %d, \"neg_config_exit\": %d, "
             "\"usage_exit\": %d}, \"e2e\": {\"shots\": [\"%s\"]}}",
             RV_SPAWN_EXE, RV_SPAWN_CONFIG_FILE, RV_SPAWN_FONTS_DIR, RV_SPAWN_TIMEOUT_LONG,
             RV_SPAWN_EXIT_NEG, RV_SPAWN_EXIT_USAGE, RV_SPAWN_SHOT_A);
    bt_rv_release_cfg cfg;
    parse_spawn_cfg(text, &cfg);
    size_t count = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_check_shot_dir(dir, &cfg, RV_SPAWN_SHOT_W, RV_SPAWN_SHOT_H, &count, err,
                                  sizeof err) != 0);
    BT_CHECK(strstr(err, "not a decodable image") != NULL);
}

BT_TEST(release_verify, run_fails_the_zip_stage_when_the_packaged_window_config_is_unparsable) {
    rv_bat bat;
    bat_build(&bat, 1, 1, 0);
    char staging[BT_PATH_MAX];
    char exe[BT_PATH_MAX];
    spawn_app_tree("dims-bad", &bat, staging, exe);
    char config_path[BT_PATH_MAX];
    rv_util_join(config_path, staging, RV_SPAWN_CONFIG_FILE);
    rv_util_write(config_path, "not json");
    char dir[BT_PATH_MAX];
    rv_util_scratch("dims-bad-run", dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "dist.zip");
    write_spawn_zip(zip, staging);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    char text[512];
    spawn_cfg_json(text, sizeof text, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                   RV_SPAWN_EXIT_USAGE);
    rv_util_write(config, text);
    const char *argv[] = {staging, zip, NULL};
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = dir;
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_run(&env, &code, err, sizeof err), 0);
    BT_CHECK_EQ(code, BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_rejects_a_config_larger_than_the_read_cap) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("cfg-huge", dir);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    FILE *file = fopen(config, "wb");
    BT_CHECK(file != NULL);
    char chunk[RV_SPAWN_CFG_CHUNK];
    memset(chunk, '{', sizeof chunk);
    for (size_t at = 0; at < RV_SPAWN_CFG_CAP / RV_SPAWN_CFG_CHUNK + 1; at++) {
        fwrite(chunk, 1, sizeof chunk, file);
    }
    fclose(file);
    const char *argv[] = {"staging", "dist.zip", NULL};
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = dir;
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_run(&env, &code, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "too large") != NULL);
}

BT_TEST(release_verify, run_rejects_a_config_that_does_not_parse) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("cfg-bad", dir);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    rv_util_write(config, "not json");
    const char *argv[] = {"staging", "dist.zip", NULL};
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = dir;
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_run(&env, &code, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot parse config") != NULL);
}

BT_TEST(release_verify, run_rejects_a_staging_dir_without_a_folder_name) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("rootless", dir);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    char text[512];
    spawn_cfg_json(text, sizeof text, RV_SPAWN_TIMEOUT_LONG, RV_SPAWN_EXIT_NEG,
                   RV_SPAWN_EXIT_USAGE);
    rv_util_write(config, text);
    const char *argv[] = {"/", "dist.zip", NULL};
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = dir;
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_run(&env, &code, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "has no folder name") != NULL);
}
