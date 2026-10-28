#include "release-verify/exec.h"
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
#include "unit/tools/spawn_util.h"

static const char *FULL_CONFIG_JSON =
    "{\n"
    "  \"release\": {\n"
    "    \"staging_dir\": \"artifacts/dist/blind-trainer\",\n"
    "    \"exe\": \"blind-trainer.exe\",\n"
    "    \"config_file\": \"config/config.json\",\n"
    "    \"fonts_dir\": \"assets/fonts\",\n"
    "    \"run_timeout_secs\": 120,\n"
    "    \"neg_config_exit\": 1,\n"
    "    \"usage_exit\": 2\n"
    "  },\n"
    "  \"e2e\": {\"shots\": [\"menu\", \"drill\", \"memo\", \"solve\", \"algs\", \"stats\", \"settings\", \"fatal\"]}\n"
    "}\n";

static const char *NO_EXE_CONFIG_JSON = "{\n"
                                        "  \"release\": {\n"
                                        "    \"staging_dir\": \"artifacts/dist/blind-trainer\",\n"
                                        "    \"config_file\": \"config/config.json\",\n"
                                        "    \"fonts_dir\": \"assets/fonts\",\n"
                                        "    \"run_timeout_secs\": 120,\n"
                                        "    \"usage_exit\": 2\n"
                                        "  },\n"
                                        "  \"e2e\": {\"shots\": [\"menu\"]}\n"
                                        "}\n";

static const char *NO_NEG_EXIT_CONFIG_JSON =
    "{\n"
    "  \"release\": {\n"
    "    \"staging_dir\": \"artifacts/dist/blind-trainer\",\n"
    "    \"exe\": \"blind-trainer.exe\",\n"
    "    \"config_file\": \"config/config.json\",\n"
    "    \"fonts_dir\": \"assets/fonts\",\n"
    "    \"run_timeout_secs\": 120,\n"
    "    \"usage_exit\": 2\n"
    "  },\n"
    "  \"e2e\": {\"shots\": [\"menu\"]}\n"
    "}\n";

static const char *NO_E2E_CONFIG_JSON = "{\n"
                                        "  \"release\": {\n"
                                        "    \"staging_dir\": \"artifacts/dist/blind-trainer\",\n"
                                        "    \"exe\": \"blind-trainer.exe\",\n"
                                        "    \"config_file\": \"config/config.json\",\n"
                                        "    \"fonts_dir\": \"assets/fonts\",\n"
                                        "    \"run_timeout_secs\": 120,\n"
                                        "    \"neg_config_exit\": 1,\n"
                                        "    \"usage_exit\": 2\n"
                                        "  }\n"
                                        "}\n";

static void parse_cfg(const char *text, bt_rv_release_cfg *cfg) {
    bt_json_value *value = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, strlen(text), &value, &json_err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "config json: %s", json_err.message);
        return;
    }
    char err[BT_RV_ERR_MAX];
    if (bt_rv_release_cfg_from(value, cfg, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "release cfg: %s", err);
    }
    bt_json_free(value);
}

static void make_staging(const char *dir) {
    char fonts[BT_PATH_MAX];
    char config_dir[BT_PATH_MAX];
    rv_util_join(fonts, dir, "assets/fonts");
    rv_util_join(config_dir, dir, "config");
    rv_util_mkdir_p(fonts);
    rv_util_mkdir_p(config_dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "blind-trainer.exe");
    rv_util_write(path, "fake exe");
    rv_util_join(path, dir, "config/config.json");
    rv_util_write(path, "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}");
    rv_util_join(path, dir, "assets/fonts/main.ttf");
    rv_util_write(path, "font");
    rv_util_join(path, dir, "assets/fonts/bold.ttf");
    rv_util_write(path, "font");
}

static void write_dist_zip(const char *zip_path, const char *staging) {
    unsigned char *exe = NULL;
    unsigned char *config = NULL;
    unsigned char *main_font = NULL;
    unsigned char *bold_font = NULL;
    size_t exe_len = 0;
    size_t config_len = 0;
    size_t main_len = 0;
    size_t bold_len = 0;
    char path[BT_PATH_MAX];
    rv_util_join(path, staging, "blind-trainer.exe");
    if (rv_util_file_bytes(path, &exe, &exe_len) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "missing staged exe");
        return;
    }
    rv_util_join(path, staging, "config/config.json");
    if (rv_util_file_bytes(path, &config, &config_len) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "missing staged config");
        return;
    }
    rv_util_join(path, staging, "assets/fonts/main.ttf");
    if (rv_util_file_bytes(path, &main_font, &main_len) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "missing staged font");
        return;
    }
    rv_util_join(path, staging, "assets/fonts/bold.ttf");
    if (rv_util_file_bytes(path, &bold_font, &bold_len) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "missing staged font");
        return;
    }
    rv_zip_entry entries[4] = {
        {"blind-trainer\\blind-trainer.exe", exe, exe_len},
        {"blind-trainer\\config\\config.json", config, config_len},
        {"blind-trainer/assets/fonts/main.ttf", main_font, main_len},
        {"blind-trainer/assets/fonts/bold.ttf", bold_font, bold_len},
    };
    rv_util_write_zip(zip_path, entries, 4);
    free(exe);
    free(config);
    free(main_font);
    free(bold_font);
}

static bt_rv_env env_for(const char *root, const char *config, const char *const argv[]) {
    bt_rv_env env;
    env.argv = argv;
    env.config = config;
    env.vars = NULL;
    env.root = root;
    return env;
}

static int parse_ok(const char *const argv[], bt_rv_parsed_args *out) {
    int count = 0;
    while (argv[count] != NULL) {
        count++;
    }
    char err[BT_RV_ERR_MAX];
    return bt_rv_parse_args(count, argv, out, err, sizeof err);
}

BT_TEST(release_verify, parse_args_reads_two_positionals_and_the_run_flag) {
    bt_rv_parsed_args plain;
    const char *plain_argv[] = {"staging", "dist.zip", NULL};
    BT_CHECK_EQ(parse_ok(plain_argv, &plain), 0);
    BT_CHECK(!plain.run);
    bt_rv_parsed_args flagged;
    const char *flagged_argv[] = {"staging", "dist.zip", "--run", NULL};
    BT_CHECK_EQ(parse_ok(flagged_argv, &flagged), 0);
    BT_CHECK(flagged.run);
    bt_rv_parsed_args first;
    const char *first_argv[] = {"--run", "staging", "dist.zip", NULL};
    BT_CHECK_EQ(parse_ok(first_argv, &first), 0);
    BT_CHECK(first.run);
    BT_CHECK_EQ(first.staging, "staging");
    BT_CHECK_EQ(first.zip, "dist.zip");
}

BT_TEST(release_verify, parse_args_rejects_wrong_counts_and_unknown_flags) {
    bt_rv_parsed_args out;
    const char *none[] = {NULL};
    BT_CHECK(parse_ok(none, &out) != 0);
    const char *one[] = {"staging", NULL};
    BT_CHECK(parse_ok(one, &out) != 0);
    const char *three[] = {"a", "b", "c", NULL};
    BT_CHECK(parse_ok(three, &out) != 0);
    const char *unknown[] = {"a", "b", "--wat", NULL};
    BT_CHECK(parse_ok(unknown, &out) != 0);
    const char *twice[] = {"a", "b", "--run", "--run", NULL};
    BT_CHECK_EQ(parse_ok(twice, &out), 0);
}

BT_TEST(release_verify, release_cfg_from_reads_every_tunable) {
    bt_rv_release_cfg cfg;
    parse_cfg(FULL_CONFIG_JSON, &cfg);
    BT_CHECK_EQ(cfg.exe, "blind-trainer.exe");
    BT_CHECK_EQ(cfg.config_file, "config/config.json");
    BT_CHECK_EQ(cfg.fonts_dir, "assets/fonts");
    BT_CHECK_EQ(cfg.db_file, BT_RV_DB_FILE_FALLBACK);
    BT_CHECK_EQ(cfg.run_timeout_secs, 120ULL);
    BT_CHECK_EQ(cfg.neg_config_exit, 1);
    BT_CHECK_EQ(cfg.usage_exit, 2);
    BT_CHECK_EQ(cfg.shot_count, 8U);
    const char *wanted[] = {"menu", "drill", "memo", "solve", "algs", "stats", "settings", "fatal"};
    for (size_t at = 0; at < cfg.shot_count; at++) {
        BT_CHECK_EQ(cfg.shots[at], wanted[at]);
    }
    BT_CHECK_EQ(cfg.staging_dir, "artifacts/dist/blind-trainer");
}

BT_TEST(release_verify, release_cfg_honors_an_explicit_db_file) {
    bt_rv_release_cfg cfg;
    parse_cfg(
        "{\"paths\": {\"db_file\": \"custom.db\"},"
        " \"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": 120, \"neg_config_exit\": 1, \"usage_exit\": 2},"
        " \"e2e\": {\"shots\": [\"menu\"]}}",
        &cfg);
    BT_CHECK_EQ(cfg.db_file, "custom.db");
}

BT_TEST(release_verify, read_db_user_version_reads_the_sqlite_header) {
    unsigned char bytes[100];
    memset(bytes, 0, sizeof bytes);
    memcpy(bytes, "SQLite format 3\0", 16);
    unsigned int version = 0;
    char err[BT_RV_ERR_MAX];
    bytes[60] = 0;
    bytes[61] = 0;
    bytes[62] = 0;
    bytes[63] = 7;
    BT_CHECK_EQ(bt_rv_read_db_user_version(bytes, sizeof bytes, &version, err, sizeof err), 0);
    BT_CHECK_EQ(version, 7U);
    bytes[60] = 0xFF;
    bytes[61] = 0xFF;
    bytes[62] = 0xFF;
    bytes[63] = 0xFF;
    BT_CHECK_EQ(bt_rv_read_db_user_version(bytes, sizeof bytes, &version, err, sizeof err), 0);
    BT_CHECK_EQ(version, 4294967295U);
    bytes[0] = 'X';
    BT_CHECK(bt_rv_read_db_user_version(bytes, sizeof bytes, &version, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "not a sqlite database") != NULL);
    bytes[0] = 'S';
    BT_CHECK(bt_rv_read_db_user_version(bytes, 61, &version, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "not a sqlite database") != NULL);
}

static size_t migration_file_count(const char *root) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/db/migrations/*.sql", root);
    WIN32_FIND_DATAA found;
    size_t count = 0;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        count++;
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    return count;
}

BT_TEST(release_verify, latest_user_version_pins_the_migration_file_count) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    BT_CHECK_EQ(migration_file_count(root), (size_t)BT_RV_LATEST_USER_VERSION);
}

BT_TEST(release_verify, db_stage_accepts_a_fully_migrated_db_and_rejects_a_stale_one) {
    char sandbox[BT_PATH_MAX];
    rv_util_scratch("db-stage", sandbox);
    char app_dir[BT_PATH_MAX];
    snprintf(app_dir, sizeof app_dir, "%s/appdata/%s", sandbox, BT_APP_DIR_NAME);
    rv_util_mkdir_p(app_dir);
    char db_path[BT_PATH_MAX];
    rv_util_join(db_path, app_dir, BT_RV_DB_FILE_FALLBACK);
    unsigned char bytes[100];
    memset(bytes, 0, sizeof bytes);
    memcpy(bytes, "SQLite format 3\0", 16);
    bytes[60] = (unsigned char)(BT_RV_LATEST_USER_VERSION >> 24U);
    bytes[61] = (unsigned char)(BT_RV_LATEST_USER_VERSION >> 16U);
    bytes[62] = (unsigned char)(BT_RV_LATEST_USER_VERSION >> 8U);
    bytes[63] = (unsigned char)BT_RV_LATEST_USER_VERSION;
    rv_util_write_bytes(db_path, bytes, sizeof bytes);
    bt_rv_release_cfg cfg;
    parse_cfg(FULL_CONFIG_JSON, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_db_stage(sandbox, &cfg, err, sizeof err), 0);
    bytes[60] = 0;
    bytes[61] = 0;
    bytes[62] = 0;
    bytes[63] = 0;
    rv_util_write_bytes(db_path, bytes, sizeof bytes);
    BT_CHECK(bt_rv_db_stage(sandbox, &cfg, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "expected") != NULL);
    BT_CHECK(DeleteFileA(db_path));
    BT_CHECK(bt_rv_db_stage(sandbox, &cfg, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot read") != NULL);
}

BT_TEST(release_verify, release_cfg_from_names_the_missing_key_or_section) {
    bt_json_value *value = bt_json_new_null();
    bt_rv_release_cfg cfg;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_release_cfg_from(value, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config release section missing");
    bt_json_free(value);
    bt_json_error json_err;
    BT_CHECK_EQ(bt_json_parse(NO_EXE_CONFIG_JSON, strlen(NO_EXE_CONFIG_JSON), &value, &json_err),
                0);
    BT_CHECK(bt_rv_release_cfg_from(value, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config release.exe missing");
    bt_json_free(value);
    BT_CHECK_EQ(
        bt_json_parse(NO_NEG_EXIT_CONFIG_JSON, strlen(NO_NEG_EXIT_CONFIG_JSON), &value, &json_err),
        0);
    BT_CHECK(bt_rv_release_cfg_from(value, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config release.neg_config_exit must be an exit code integer");
    bt_json_free(value);
    BT_CHECK_EQ(bt_json_parse(NO_E2E_CONFIG_JSON, strlen(NO_E2E_CONFIG_JSON), &value, &json_err),
                0);
    BT_CHECK(bt_rv_release_cfg_from(value, &cfg, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config e2e.shots missing");
    bt_json_free(value);
}

BT_TEST(release_verify, release_cfg_from_rejects_bad_value_types) {
    const char *cases[] = {
        "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": \"soon\", \"neg_config_exit\": 1,"
        " \"usage_exit\": 2}, \"e2e\": {\"shots\": [\"menu\"]}}",
        "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": 120, \"neg_config_exit\": 1,"
        " \"usage_exit\": \"two\"}, \"e2e\": {\"shots\": [\"menu\"]}}",
        "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": 120, \"neg_config_exit\": 1,"
        " \"usage_exit\": 2}, \"e2e\": {\"shots\": [7, \"menu\"]}}",
        "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": 0, \"neg_config_exit\": 1,"
        " \"usage_exit\": 2}, \"e2e\": {\"shots\": [\"menu\"]}}",
        "{\"release\": {\"staging_dir\": \"s\", \"exe\": \"e.exe\", \"config_file\": \"c.json\","
        " \"fonts_dir\": \"f\", \"run_timeout_secs\": 120, \"neg_config_exit\": \"one\","
        " \"usage_exit\": 2}, \"e2e\": {\"shots\": [\"menu\"]}}",
    };
    const char *needles[] = {
        "run_timeout_secs", "usage_exit", "e2e.shots", "run_timeout_secs", "neg_config_exit",
    };
    for (size_t at = 0; at < sizeof cases / sizeof cases[0]; at++) {
        bt_json_value *value = NULL;
        bt_json_error json_err;
        BT_CHECK_EQ(bt_json_parse(cases[at], strlen(cases[at]), &value, &json_err), 0);
        bt_rv_release_cfg cfg;
        char err[BT_RV_ERR_MAX];
        BT_CHECK(bt_rv_release_cfg_from(value, &cfg, err, sizeof err) != 0);
        BT_CHECK(strstr(err, needles[at]) != NULL);
        bt_json_free(value);
    }
}

BT_TEST(release_verify, n1_spec_passes_a_shots_dir_so_the_flag_is_not_usage_rejected) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("n1-spec", dir);
    char extract[BT_PATH_MAX];
    char config_dir[BT_PATH_MAX];
    rv_util_join(extract, dir, "extract");
    rv_util_join(config_dir, extract, "config");
    rv_util_mkdir_p(config_dir);
    char config_path[BT_PATH_MAX];
    rv_util_join(config_path, extract, "config/config.json");
    rv_util_write(config_path, "{}");
    bt_rv_release_cfg cfg;
    parse_cfg(FULL_CONFIG_JSON, &cfg);
    bt_rv_child_spec spec;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_n1_spec("app.exe", extract, dir, &cfg, NULL, &spec, err, sizeof err), 0);
    BT_CHECK_EQ(spec.arg_count, 2U);
    BT_CHECK_EQ(spec.args[0], "--smoke");
    BT_CHECK(strcmp(spec.args[1] + strlen(spec.args[1]) - 5, "shots") == 0);
    char wanted[BT_PATH_MAX];
    snprintf(wanted, sizeof wanted, "%s/config/config.json", extract);
    BT_CHECK_EQ(bt_rv_env_get(&spec, "BT_CONFIG"), wanted);
    BT_CHECK(!rv_util_exists(config_path));
    bt_rv_child_spec_free(&spec);
}

BT_TEST(release_verify, n1_stage_names_a_missing_extracted_config_before_spawning) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("n1-missing", dir);
    char extract[BT_PATH_MAX];
    rv_util_join(extract, dir, "extract");
    rv_util_mkdir_p(extract);
    bt_rv_release_cfg cfg;
    parse_cfg(FULL_CONFIG_JSON, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_n1_stage("nowhere.exe", extract, dir, &cfg, NULL, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot delete") != NULL);
}

BT_TEST(release_verify, n1_stage_deletes_the_config_then_requires_the_neg_exit) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("n1-spawn", dir);
    char extract[BT_PATH_MAX];
    char config_dir[BT_PATH_MAX];
    rv_util_join(extract, dir, "extract");
    rv_util_join(config_dir, extract, "config");
    rv_util_mkdir_p(config_dir);
    char config_path[BT_PATH_MAX];
    rv_util_join(config_path, extract, "config/config.json");
    rv_util_write(config_path, "{}");
    bt_rv_release_cfg cfg;
    parse_cfg(FULL_CONFIG_JSON, &cfg);
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_n1_stage("nowhere.exe", extract, dir, &cfg, NULL, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "did not run") != NULL);
    BT_CHECK(!rv_util_exists(config_path));
}

BT_TEST(release_verify, packaged_window_dims_reads_the_staged_config) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("dims", dir);
    make_staging(dir);
    unsigned int width = 0;
    unsigned int height = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(
        bt_rv_packaged_window_dims(dir, "config/config.json", &width, &height, err, sizeof err), 0);
    BT_CHECK_EQ(width, 1280U);
    BT_CHECK_EQ(height, 800U);
    char config_path[BT_PATH_MAX];
    rv_util_join(config_path, dir, "config/config.json");
    rv_util_write(config_path, "{\"app\": {}}");
    BT_CHECK(bt_rv_packaged_window_dims(dir, "config/config.json", &width, &height, err,
                                        sizeof err) != 0);
    BT_CHECK(strstr(err, "app.window.width") != NULL);
}

BT_TEST(release_verify, zip_root_from_uses_the_staging_folder_name) {
    char root[BT_PATH_MAX];
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_zip_root_from("out/dist/blind-trainer", root, err, sizeof err), 0);
    BT_CHECK_EQ(root, "blind-trainer");
    BT_CHECK(bt_rv_zip_root_from("/", root, err, sizeof err) != 0);
}

BT_TEST(release_verify, check_exit_classifies_each_outcome) {
    bt_rv_outcome exit_zero = {BT_RV_OUTCOME_EXIT, 0, ""};
    bt_rv_outcome exit_one = {BT_RV_OUTCOME_EXIT, 1, ""};
    bt_rv_outcome exit_two = {BT_RV_OUTCOME_EXIT, 2, ""};
    bt_rv_outcome timeout = {BT_RV_OUTCOME_TIMEOUT, 0, ""};
    bt_rv_outcome spawn = {BT_RV_OUTCOME_SPAWN, 0, "nope"};
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_check_exit(&exit_zero, 0, err, sizeof err), 0);
    BT_CHECK_EQ(bt_rv_check_exit(&exit_one, 1, err, sizeof err), 0);
    BT_CHECK(bt_rv_check_exit(&exit_two, 1, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "exit code 2") != NULL);
    BT_CHECK(bt_rv_check_exit(&timeout, 0, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "timed out") != NULL);
    BT_CHECK(bt_rv_check_exit(&spawn, 0, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "did not run") != NULL);
}

static void two_shot_cfg(bt_rv_release_cfg *cfg) {
    parse_cfg(FULL_CONFIG_JSON, cfg);
    cfg->shot_count = 2;
}

BT_TEST(release_verify, check_shot_dir_requires_the_exact_shot_set_at_the_right_dims) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("shots", dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "menu.png");
    rv_util_write_png(path, 1280, 800);
    rv_util_join(path, dir, "drill.png");
    rv_util_write_png(path, 1280, 800);
    bt_rv_release_cfg cfg;
    two_shot_cfg(&cfg);
    size_t count = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_check_shot_dir(dir, &cfg, 1280, 800, &count, err, sizeof err), 0);
    BT_CHECK_EQ(count, 2U);
    snprintf(cfg.shots[2], sizeof cfg.shots[2], "memo");
    cfg.shot_count = 3;
    BT_CHECK(bt_rv_check_shot_dir(dir, &cfg, 1280, 800, &count, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "missing") != NULL && strstr(err, "memo.png") != NULL);
    cfg.shot_count = 2;
    rv_util_join(path, dir, "stray.txt");
    rv_util_write(path, "");
    BT_CHECK(bt_rv_check_shot_dir(dir, &cfg, 1280, 800, &count, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "unexpected") != NULL && strstr(err, "stray.txt") != NULL);
    BT_CHECK(DeleteFileA(path));
    BT_CHECK(bt_rv_check_shot_dir(dir, &cfg, 640, 480, &count, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "640x480") != NULL && strstr(err, "1280x800") != NULL);
}

BT_TEST(release_verify, check_shot_dir_names_every_shot_missing_from_an_empty_dir) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("shots-empty", dir);
    bt_rv_release_cfg cfg;
    two_shot_cfg(&cfg);
    cfg.shot_count = 1;
    size_t count = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_check_shot_dir(dir, &cfg, 10, 10, &count, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "missing") != NULL && strstr(err, "menu.png") != NULL);
    char absent[BT_PATH_MAX];
    rv_util_scratch("shots-absent", absent);
    char none[BT_PATH_MAX];
    rv_util_join(none, absent, "none");
    BT_CHECK(bt_rv_check_shot_dir(none, &cfg, 10, 10, &count, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot read shots dir") != NULL);
}

BT_TEST(release_verify, stage_spec_scrubs_bt_vars_and_applies_extras) {
    const char *args[] = {"--smoke", "shots", NULL};
    const char *vars[] = {"BT_CONFIG=leak.json", "PATH=c:/bin", NULL};
    bt_rv_env_pair extras[] = {
        {"APPDATA", "C:/sandbox"},
        {"BT_CONFIG", "C:/dist/config/config.json"},
    };
    bt_rv_child_spec spec;
    bt_rv_stage_spec("dist/blind-trainer.exe", args, "foreign", vars, extras, 2, &spec);
    BT_CHECK_EQ(spec.program, "dist/blind-trainer.exe");
    BT_CHECK_EQ(spec.arg_count, 2U);
    BT_CHECK_EQ(spec.args[1], "shots");
    BT_CHECK_EQ(spec.cwd, "foreign");
    BT_CHECK_EQ(bt_rv_env_get(&spec, "BT_CONFIG"), "C:/dist/config/config.json");
    BT_CHECK_EQ(bt_rv_env_get(&spec, "APPDATA"), "C:/sandbox");
    BT_CHECK_EQ(bt_rv_env_get(&spec, "PATH"), "c:/bin");
    bt_rv_child_spec_free(&spec);
}

BT_TEST(release_verify, run_rejects_wrong_arg_count_with_the_usage_line) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("run-usage", dir);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "config.json");
    const char *argv[] = {"only", NULL};
    bt_rv_env env = env_for(dir, config, argv);
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_run(&env, &code, err, sizeof err) != 0);
    BT_CHECK_EQ(err, BT_RV_USAGE_LINE);
}

BT_TEST(release_verify, run_names_the_missing_release_section) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("run-no-release", dir);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "config.json");
    rv_util_write(config, "{\"app\": {}}");
    const char *argv[] = {"staging", "dist.zip", NULL};
    bt_rv_env env = env_for(dir, config, argv);
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_run(&env, &code, err, sizeof err) != 0);
    BT_CHECK_EQ(err, "config release section missing");
}

static int run_dist(const char *tag, const char *const extra[]) {
    char dir[BT_PATH_MAX];
    rv_util_scratch(tag, dir);
    char staging[BT_PATH_MAX];
    rv_util_join(staging, dir, "blind-trainer");
    make_staging(staging);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "blind-trainer-v0.0.0-windows-x64.zip");
    write_dist_zip(zip, staging);
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    rv_util_write(config, FULL_CONFIG_JSON);
    const char *argv[8] = {staging, zip, NULL, NULL, NULL, NULL, NULL, NULL};
    size_t count = 2;
    for (size_t at = 0; extra[at] != NULL && at < 5; at++) {
        argv[count++] = extra[at];
    }
    bt_rv_env env = env_for(dir, config, argv);
    int code = 0;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_run(&env, &code, err, sizeof err) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_dist %s failed: %s", tag, err);
        return -1;
    }
    return code;
}

BT_TEST(release_verify, run_passes_the_zip_stage_without_run_flag) {
    const char *none[] = {NULL};
    BT_CHECK_EQ(run_dist("run-pass", none), 0);
}

BT_TEST(release_verify, run_fails_the_whole_tool_when_the_zip_is_tampered) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("run-tamper", dir);
    char staging[BT_PATH_MAX];
    rv_util_join(staging, dir, "blind-trainer");
    make_staging(staging);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "dist.zip");
    write_dist_zip(zip, staging);
    char font_path[BT_PATH_MAX];
    rv_util_join(font_path, staging, "assets/fonts/main.ttf");
    rv_util_write(font_path, "different bytes");
    char config[BT_PATH_MAX];
    rv_util_join(config, dir, "verify-config.json");
    rv_util_write(config, FULL_CONFIG_JSON);
    const char *argv[] = {staging, zip, NULL};
    bt_rv_env env = env_for(dir, config, argv);
    int code = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_run(&env, &code, err, sizeof err), 0);
    BT_CHECK_EQ(code, BT_RV_EXIT_FAIL);
}

BT_TEST(release_verify, run_reports_the_spawn_failure_when_the_packaged_exe_is_not_runnable) {
    const char *run_flag[] = {"--run", NULL};
    BT_CHECK_EQ(run_dist("run-spawn", run_flag), BT_RV_EXIT_FAIL);
}
