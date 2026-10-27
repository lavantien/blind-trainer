#include "app/views/state.h"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void write_config(const char *dir, const char *data_ga) {
    char text[TA_PATH_MAX];
    if (data_ga == NULL) {
        snprintf(text, sizeof text, "{}");
    } else {
        char escaped[TA_PATH_MAX];
        size_t at = 0;
        for (const char *cursor = data_ga; *cursor != '\0' && at + 3 < sizeof escaped; cursor++) {
            if (*cursor == '\\') {
                escaped[at++] = '\\';
            }
            escaped[at++] = *cursor;
        }
        escaped[at] = '\0';
        snprintf(text, sizeof text, "{\"paths\":{\"data_ga\":\"%s\"}}", escaped);
    }
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/config.json", dir);
    ta_write_file(path, text);
}

static bt_app_runtime_paths make_paths(const char *dir, const char *name) {
    bt_app_runtime_paths paths;
    memset(&paths, 0, sizeof paths);
    snprintf(paths.config_path, sizeof paths.config_path, "%s/%s", dir, name);
    return paths;
}

static const bt_app_runtime_paths *paths_with_config_owned(const char *dir) {
    static bt_app_runtime_paths paths;
    paths = make_paths(dir, "config.json");
    return &paths;
}

static const bt_app_runtime_paths *paths_without_config_owned(const char *dir) {
    static bt_app_runtime_paths paths;
    paths = make_paths(dir, "missing.json");
    return &paths;
}

static bt_app_views *views_with_data_ga(const char *syllables_json) {
    const char *dir = ta_temp_dir("view_state", "ga");
    char ga[TA_PATH_MAX];
    snprintf(ga, sizeof ga, "%s/ga", dir);
    char file[TA_PATH_MAX];
    snprintf(file, sizeof file, "%s/syllables.json", ga);
    ta_write_file(file, syllables_json);
    write_config(dir, ga);
    bt_app_views *views = bt_app_views_shared();
    bt_app_views_load_syllables(views, paths_with_config_owned(dir));
    return views;
}

BT_TEST(view_state, syllables_path_defaults_to_workspace_data_ga) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s\\data\\ga\\syllables.json", root);
    const char *dir = ta_temp_dir("view_state", "path-default");
    bt_app_runtime_paths paths = *paths_without_config_owned(dir);
    char out[BT_PATH_MAX];
    bt_views_syllables_path(&paths, out);
    BT_CHECK(strcmp(out, expected) == 0);
}

BT_TEST(view_state, syllables_path_honors_relative_data_ga) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s\\scratch/ga\\syllables.json", root);
    const char *dir = ta_temp_dir("view_state", "path-relative");
    write_config(dir, "scratch/ga");
    bt_app_runtime_paths paths = *paths_with_config_owned(dir);
    char out[BT_PATH_MAX];
    bt_views_syllables_path(&paths, out);
    BT_CHECK(strcmp(out, expected) == 0);
}

BT_TEST(view_state, syllables_path_keeps_absolute_data_ga) {
    char expected[TA_PATH_MAX];
    snprintf(expected, sizeof expected, "C:/ga-root\\syllables.json");
    const char *dir = ta_temp_dir("view_state", "path-absolute");
    write_config(dir, "C:/ga-root");
    bt_app_runtime_paths paths = *paths_with_config_owned(dir);
    char out[BT_PATH_MAX];
    bt_views_syllables_path(&paths, out);
    BT_CHECK(strcmp(out, expected) == 0);
}

BT_TEST(view_state, load_syllables_reads_cases_and_skips_empty) {
    bt_app_views *views = views_with_data_ga(
        "{\"cases\":[{\"orbit\":\"edge\",\"letters\":\"AB\",\"syllable\":\"tả\"},"
        "{\"orbit\":\"corner\",\"letters\":\"EF\",\"syllable\":\"\"}]}");
    BT_CHECK(views->syllables.loaded == 1);
    BT_CHECK(views->syllables.count == 1);
    BT_CHECK(strcmp(bt_syllable_book_for(&views->syllables, "edge", "AB"), "tả") == 0);
    BT_CHECK(bt_syllable_book_for(&views->syllables, "corner", "EF") == NULL);
    BT_CHECK(bt_syllable_book_for(&views->syllables, "edge", "CD") == NULL);
    bt_app_views_dispose_shared();
}

BT_TEST(view_state, load_syllables_lets_the_last_duplicate_win) {
    bt_app_views *views =
        views_with_data_ga("{\"cases\":[{\"orbit\":\"edge\",\"letters\":\"AB\",\"syllable\":\"x\"},"
                           "{\"orbit\":\"edge\",\"letters\":\"AB\",\"syllable\":\"y\"}]}");
    BT_CHECK(views->syllables.count == 1);
    BT_CHECK(strcmp(bt_syllable_book_for(&views->syllables, "edge", "AB"), "y") == 0);
    bt_app_views_dispose_shared();
}

BT_TEST(view_state, load_syllables_missing_file_leaves_the_book_empty) {
    const char *dir = ta_temp_dir("view_state", "missing");
    write_config(dir, "nowhere");
    bt_app_views *views = bt_app_views_shared();
    bt_app_views_load_syllables(views, paths_with_config_owned(dir));
    BT_CHECK(views->syllables.loaded == 0);
    BT_CHECK(views->syllables.count == 0);
    bt_app_views_dispose_shared();
}

BT_TEST(view_state, load_syllables_malformed_file_leaves_the_book_empty) {
    bt_app_views *views = views_with_data_ga("not json at all");
    BT_CHECK(views->syllables.loaded == 0);
    BT_CHECK(views->syllables.count == 0);
    bt_app_views_dispose_shared();
}

BT_TEST(view_state, now_day_divides_unix_secs_by_86400) {
    long long secs = bt_views_now_unix_secs();
    long long day = bt_views_now_day();
    BT_CHECK(day == secs / 86400);
    time_t wall = time(NULL);
    BT_CHECK(secs >= (long long)wall - 5 && secs <= (long long)wall + 5);
}

BT_TEST(view_state, time_seed_is_nonzero) {
    BT_CHECK(bt_views_time_seed() != 0);
}

BT_TEST(view_state, shared_views_reuse_and_reset) {
    bt_app_views *first = bt_app_views_shared();
    bt_app_views *second = bt_app_views_shared();
    BT_CHECK(first == second);
    first->net.frozen = 1;
    bt_app_views_dispose_shared();
    bt_app_views *fresh = bt_app_views_shared();
    BT_CHECK(fresh != first || fresh->net.frozen == 0);
    BT_CHECK(fresh->net.frozen == 0);
    bt_app_views_dispose_shared();
}

BT_TEST(view_state, load_syllables_skips_keys_whose_letters_are_not_two) {
    bt_app_views *views = views_with_data_ga(
        "{\"cases\":[{\"orbit\":\"edge\",\"letters\":\"ABX\",\"syllable\":\"z\"},"
        "{\"orbit\":\"edge\",\"letters\":\"A\",\"syllable\":\"w\"},"
        "{\"orbit\":\"edge\",\"letters\":\"AB\",\"syllable\":\"tả\"}]}");
    BT_CHECK(views->syllables.loaded == 1);
    BT_CHECK(views->syllables.count == 1);
    BT_CHECK(strcmp(bt_syllable_book_for(&views->syllables, "edge", "AB"), "tả") == 0);
    bt_app_views_dispose_shared();
}
