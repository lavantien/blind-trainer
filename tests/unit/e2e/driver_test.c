#include "common/bt_harness.h"

#include "common/json.h"
#include "common/paths.h"
#include "e2e/lib.h"
#include "e2e/png.h"

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TD_PATH_MAX 1024

static unsigned td_counter(void) {
    static unsigned counter = 0;
    counter += 1;
    return counter;
}

static void td_mkdir_p(const char *path) {
    char buf[TD_PATH_MAX];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + (buf[0] && buf[1] == ':' ? 3 : 1); *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            _mkdir(buf);
            *p = save;
        }
    }
    _mkdir(buf);
}

static const char *td_temp_dir(const char *tag) {
    static char dirs[8][TD_PATH_MAX];
    static size_t next = 0;
    char *dir = dirs[next % 8];
    next += 1;
    snprintf(dir, TD_PATH_MAX, "%s\\bt-e2e-tests\\%s-%d-%u", getenv("TEMP"), tag, _getpid(),
             td_counter());
    td_mkdir_p(dir);
    return dir;
}

static void td_write_file(const char *path, const unsigned char *data, size_t len) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("td_write_file: cannot open %s", path);
    }
    if (fwrite(data, 1, len, file) != len) {
        fclose(file);
        BT_ABORT("td_write_file: cannot write %s", path);
    }
    fclose(file);
}

static int td_file_exists(const char *path) {
    struct _stat info;
    return _stat(path, &info) == 0 && (info.st_mode & _S_IFREG) != 0;
}

static const unsigned char TD_PNG_3X2[] = {
    137, 80,  78,  71,  13,  10,  26, 10,  0,   0,   0,   13,  73,  72,  68, 82,  0,   0,
    0,   3,   0,   0,   0,   2,   8,   6,   0,   0,   0,   157, 116, 102, 26, 0,   0,   0,
    17,  73,  68,  65, 84, 120, 156, 99,  224, 228, 228, 252, 15,  195, 12,  200, 28, 0,
    77,  96,  6,   157, 65,  42,  41,  210, 0,   0,   0,   0,   73,  69,  78,  68,  174, 66,
    96,  130};
static const unsigned char TD_PNG_2X2[] = {
    137, 80,  78,  71,  13,  10,  26, 10,  0,   0,   0,   13,  73, 72,  68, 82,  0,  0,  0,
    2,   0,   0,   0,   2,   8,   6,  0,  0,  0,   114, 182, 13,  36, 0,  0,  0,  17, 73, 68,
    65,  84, 120, 156, 99,  224, 228, 228, 252, 15,  194, 12,  48, 6,  0,  33,  246, 4, 105,
    163, 5,   176, 172, 0,  0,  0,  0,  73,  69,  78,  68,  174, 66, 96,  130};

static bt_json_value *td_parse(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        BT_ABORT("td_parse: %s", error.message);
    }
    return value;
}

static void td_parse_config(const char *text, bt_json_value **out, char *error, size_t cap) {
    const char *dir = td_temp_dir("e2e-config");
    char path[TD_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", dir);
    td_write_file(path, (const unsigned char *)text, strlen(text));
    if (bt_e2e_read_config(path, out, error, cap) != 0) {
        BT_ABORT("td_parse_config: %s", error);
    }
}

BT_TEST(driver, parse_args_takes_optional_binary_and_out_dir) {
    const char *none[] = {NULL};
    bt_e2e_args args;
    BT_CHECK_EQ(bt_e2e_parse_args(0, none, &args), 0);
    BT_CHECK_EQ(args.has_binary, 0);
    BT_CHECK_EQ(args.has_out_dir, 0);

    const char *one[] = {"bin.exe", NULL};
    BT_CHECK_EQ(bt_e2e_parse_args(1, one, &args), 0);
    BT_CHECK_EQ(args.has_binary, 1);
    BT_CHECK_EQ(args.has_out_dir, 0);
    BT_CHECK_EQ(strcmp(args.binary, "bin.exe"), 0);

    const char *two[] = {"bin.exe", "out", NULL};
    BT_CHECK_EQ(bt_e2e_parse_args(2, two, &args), 0);
    BT_CHECK_EQ(args.has_binary, 1);
    BT_CHECK_EQ(args.has_out_dir, 1);
    BT_CHECK_EQ(strcmp(args.binary, "bin.exe"), 0);
    BT_CHECK_EQ(strcmp(args.out_dir, "out"), 0);

    const char *three[] = {"a", "b", "c", NULL};
    BT_CHECK_EQ(bt_e2e_parse_args(3, three, &args), BT_E2E_PARSE_USAGE);
}

BT_TEST(driver, neg_config_exit_from_reads_the_release_key) {
    char error[BT_E2E_ERROR_MAX];
    long exit_code = 0;
    bt_json_value *ok = td_parse("{\"release\":{\"neg_config_exit\":1}}");
    BT_CHECK_EQ(bt_e2e_neg_config_exit_from(ok, &exit_code, error, sizeof error), 0);
    BT_CHECK_EQ(exit_code, 1);
    bt_json_free(ok);

    bt_json_value *missing = td_parse("{\"release\":{}}");
    BT_CHECK_EQ(bt_e2e_neg_config_exit_from(missing, &exit_code, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config release.neg_config_exit must be an integer"), 0);
    bt_json_free(missing);

    bt_json_value *malformed = td_parse("{\"release\":{\"neg_config_exit\":\"one\"}}");
    BT_CHECK_EQ(bt_e2e_neg_config_exit_from(malformed, &exit_code, error, sizeof error), 1);
    bt_json_free(malformed);
}

BT_TEST(driver, window_dims_from_reads_config_and_rejects_bad_values) {
    char error[BT_E2E_ERROR_MAX];
    bt_e2e_window_dims dims;
    bt_json_value *ok = td_parse(
        "{\"app\":{\"window\":{\"width\":1280,\"height\":800,\"title\":\"t\"}}}");
    BT_CHECK_EQ(bt_e2e_window_dims_from(ok, &dims, error, sizeof error), 0);
    BT_CHECK_EQ(dims.width, 1280u);
    BT_CHECK_EQ(dims.height, 800u);
    bt_json_free(ok);

    bt_json_value *missing = td_parse("{\"app\":{}}");
    BT_CHECK_EQ(bt_e2e_window_dims_from(missing, &dims, error, sizeof error), 1);
    bt_json_free(missing);

    bt_json_value *malformed =
        td_parse("{\"app\":{\"window\":{\"width\":\"wide\",\"height\":800}}}");
    BT_CHECK_EQ(bt_e2e_window_dims_from(malformed, &dims, error, sizeof error), 1);
    bt_json_free(malformed);

    bt_json_value *negative = td_parse("{\"app\":{\"window\":{\"width\":-1,\"height\":800}}}");
    BT_CHECK_EQ(bt_e2e_window_dims_from(negative, &dims, error, sizeof error), 1);
    bt_json_free(negative);
}

BT_TEST(driver, out_dir_from_prefers_env_then_config_then_default) {
    char out[BT_PATH_MAX];
    bt_json_value *config = td_parse("{\"paths\":{\"artifacts\":\"out\"}}");
    bt_e2e_out_dir_from("env-out", config, "/repo", out);
    BT_CHECK_EQ(strcmp(out, "/repo/env-out/e2e"), 0);
    bt_e2e_out_dir_from(NULL, config, "/repo", out);
    BT_CHECK_EQ(strcmp(out, "/repo/out/e2e"), 0);
    bt_json_free(config);
    bt_e2e_out_dir_from(NULL, NULL, "/repo", out);
    BT_CHECK_EQ(strcmp(out, "/repo/artifacts/e2e"), 0);
}

BT_TEST(driver, e2e_settings_reads_shots_and_timeout_from_config) {
    char error[BT_E2E_ERROR_MAX];
    bt_json_value *config = NULL;
    td_parse_config(
        "{\"e2e\":{\"shots\":[\"menu\",\"drill\",\"memo\",\"solve\",\"algs\",\"stats\","
        "\"settings\",\"fatal\"],\"timeout_secs\":180}}",
        &config, error, sizeof error);
    bt_e2e_settings settings;
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 0);
    BT_CHECK_EQ(settings.shots_count, (size_t)8);
    BT_CHECK_EQ(strcmp(settings.shots[0], "menu"), 0);
    BT_CHECK_EQ(strcmp(settings.shots[7], "fatal"), 0);
    BT_CHECK_EQ(settings.timeout_secs, 180ull);
    bt_e2e_free_settings(&settings);
    bt_json_free(config);
}

BT_TEST(driver, e2e_settings_names_the_missing_or_malformed_key) {
    char error[BT_E2E_ERROR_MAX];
    bt_json_value *config = NULL;
    bt_e2e_settings settings;

    td_parse_config("{\"app\":{}}", &config, error, sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e missing"), 0);
    bt_json_free(config);

    td_parse_config("{\"e2e\":{\"timeout_secs\":180}}", &config, error, sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e.shots must be an array of strings"), 0);
    bt_json_free(config);

    td_parse_config("{\"e2e\":{\"shots\":[\"menu\",7],\"timeout_secs\":180}}", &config, error,
                    sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e.shots must be an array of strings"), 0);
    bt_json_free(config);

    td_parse_config("{\"e2e\":{\"shots\":[\"menu\"]}}", &config, error, sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e.timeout_secs must be an unsigned integer"), 0);
    bt_json_free(config);

    td_parse_config("{\"e2e\":{\"shots\":[\"menu\"],\"timeout_secs\":\"soon\"}}", &config, error,
                    sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e.timeout_secs must be an unsigned integer"), 0);
    bt_json_free(config);

    td_parse_config("{\"e2e\":{\"shots\":[\"menu\"],\"timeout_secs\":-1}}", &config, error,
                    sizeof error);
    BT_CHECK_EQ(bt_e2e_settings_from(config, &settings, error, sizeof error), 1);
    BT_CHECK_EQ(strcmp(error, "config e2e.timeout_secs must be an unsigned integer"), 0);
    bt_json_free(config);
}

BT_TEST(driver, expected_names_appends_png_to_each_slug) {
    const char *shots[] = {"menu", "drill", "fatal"};
    char names[3][BT_E2E_NAME_MAX];
    size_t count = 0;
    BT_CHECK_EQ(bt_e2e_expected_names(shots, 3, names, &count), 0);
    BT_CHECK_EQ(count, (size_t)3);
    for (size_t i = 0; i < 3; i++) {
        char expected[BT_E2E_NAME_MAX];
        snprintf(expected, sizeof expected, "%s.png", shots[i]);
        int found = 0;
        for (size_t j = 0; j < count; j++) {
            if (strcmp(names[j], expected) == 0) {
                found = 1;
            }
        }
        BT_CHECK_EQ(found, 1);
    }
}

BT_TEST(driver, classify_splits_missing_and_unexpected) {
    const char *shots[] = {"menu", "drill", "memo"};
    char expected[3][BT_E2E_NAME_MAX];
    size_t expected_count = 0;
    BT_CHECK_EQ(bt_e2e_expected_names(shots, 3, expected, &expected_count), 0);
    const char *found[] = {"menu.png", "drill.png", "stray.png"};
    const char *expected_ptrs[3] = {expected[0], expected[1], expected[2]};
    char missing[3][BT_E2E_NAME_MAX];
    char unexpected[3][BT_E2E_NAME_MAX];
    size_t missing_count = 0;
    size_t unexpected_count = 0;
    bt_e2e_classify(found, 3, expected_ptrs, expected_count, missing, &missing_count, unexpected,
                    &unexpected_count);
    int memo_missing = 0;
    int menu_missing = 0;
    for (size_t i = 0; i < missing_count; i++) {
        if (strcmp(missing[i], "memo.png") == 0) {
            memo_missing = 1;
        }
        if (strcmp(missing[i], "menu.png") == 0) {
            menu_missing = 1;
        }
    }
    BT_CHECK_EQ(memo_missing, 1);
    BT_CHECK_EQ(menu_missing, 0);
    BT_CHECK_EQ(unexpected_count, (size_t)1);
    BT_CHECK_EQ(strcmp(unexpected[0], "stray.png"), 0);
}

BT_TEST(driver, clean_pngs_removes_only_png_files) {
    const char *dir = td_temp_dir("clean");
    char png[TD_PATH_MAX];
    char txt[TD_PATH_MAX];
    snprintf(png, sizeof png, "%s\\old.png", dir);
    snprintf(txt, sizeof txt, "%s\\keep.txt", dir);
    td_write_file(png, (const unsigned char *)"", 0);
    td_write_file(txt, (const unsigned char *)"", 0);
    char error[BT_E2E_ERROR_MAX];
    BT_CHECK_EQ(bt_e2e_clean_pngs(dir, error, sizeof error), 0);
    BT_CHECK_EQ(td_file_exists(png), 0);
    BT_CHECK_EQ(td_file_exists(txt), 1);
    char nested[TD_PATH_MAX];
    snprintf(nested, sizeof nested, "%s\\nested", dir);
    BT_CHECK_EQ(bt_e2e_clean_pngs(nested, error, sizeof error), 0);
}

BT_TEST(driver, png_names_lists_sorted_png_files) {
    const char *dir = td_temp_dir("names");
    char b[TD_PATH_MAX];
    char a[TD_PATH_MAX];
    char txt[TD_PATH_MAX];
    snprintf(b, sizeof b, "%s\\b.png", dir);
    snprintf(a, sizeof a, "%s\\a.png", dir);
    snprintf(txt, sizeof txt, "%s\\notes.txt", dir);
    td_write_file(b, (const unsigned char *)"", 0);
    td_write_file(a, (const unsigned char *)"", 0);
    td_write_file(txt, (const unsigned char *)"", 0);
    char error[BT_E2E_ERROR_MAX];
    char **names = NULL;
    size_t count = 0;
    BT_CHECK_EQ(bt_e2e_png_names(dir, &names, &count, error, sizeof error), 0);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK_EQ(strcmp(names[0], "a.png"), 0);
    BT_CHECK_EQ(strcmp(names[1], "b.png"), 0);
    bt_e2e_free_names(names, count);
}

BT_TEST(driver, check_shot_accepts_valid_png_and_rejects_defects) {
    const char *dir = td_temp_dir("shots");
    bt_e2e_window_dims dims = {3, 2};
    char good[TD_PATH_MAX];
    char wrong[TD_PATH_MAX];
    char empty[TD_PATH_MAX];
    char absent[TD_PATH_MAX];
    snprintf(good, sizeof good, "%s\\good.png", dir);
    snprintf(wrong, sizeof wrong, "%s\\wrong.png", dir);
    snprintf(empty, sizeof empty, "%s\\empty.png", dir);
    snprintf(absent, sizeof absent, "%s\\absent.png", dir);
    td_write_file(good, TD_PNG_3X2, sizeof TD_PNG_3X2);
    td_write_file(wrong, TD_PNG_2X2, sizeof TD_PNG_2X2);
    td_write_file(empty, (const unsigned char *)"", 0);
    char reason[BT_E2E_ERROR_MAX];
    BT_CHECK_EQ(bt_e2e_check_shot(good, dims, reason, sizeof reason), BT_E2E_SHOT_OK);
    BT_CHECK_EQ(bt_e2e_check_shot(wrong, dims, reason, sizeof reason), BT_E2E_SHOT_FAIL);
    BT_CHECK_EQ(bt_e2e_check_shot(empty, dims, reason, sizeof reason), BT_E2E_SHOT_FAIL);
    BT_CHECK_EQ(bt_e2e_check_shot(absent, dims, reason, sizeof reason), BT_E2E_SHOT_FAIL);
}

BT_TEST(driver, tail_keeps_last_lines) {
    unsigned char text[512];
    size_t len = 0;
    for (int line = 1; line <= 50; line++) {
        len += (size_t)snprintf((char *)text + len, sizeof text - len, "line %d\n", line);
    }
    char tailed[512];
    bt_e2e_tail(text, len, tailed, sizeof tailed);
    BT_CHECK(strncmp(tailed, "line 31", 7) == 0);
    BT_CHECK(strcmp(tailed + strlen(tailed) - 7, "line 50") == 0);
}
