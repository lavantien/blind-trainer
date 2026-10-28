#include "app/config.h"

#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "data/paths.h"

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

static bt_json_value *parse(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        BT_ABORT("json parse failed: %s", error.message);
    }
    return value;
}

BT_TEST(config, default_config_matches_fallbacks) {
    bt_app_config config;
    bt_app_config_defaults(&config);
    BT_CHECK_EQ(config.window.width, 1280);
    BT_CHECK_EQ(config.window.height, 800);
    BT_CHECK_EQ(strcmp(config.window.title, "blind trainer"), 0);
    BT_CHECK_EQ(config.memo.cadence_ms, 800);
    BT_CHECK_EQ(strcmp(config.memo.mode, "per-orbit"), 0);
    BT_CHECK_EQ(config.drill.reps, 6);
}

BT_TEST(config, from_json_reads_the_section_and_defaults_the_seed) {
    bt_json_value *value = parse("{\"app\":{\"window\":{\"width\":100,\"height\":200,"
                                 "\"title\":\"w\"},\"memo\":{\"cadence_ms\":250,"
                                 "\"mode\":\"syllables\"},\"drill\":{\"reps\":2}}}");
    bt_app_config config;
    char error[96];
    int rc = bt_app_config_from_json(value, &config, error, sizeof error);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(config.window.width, 100);
    BT_CHECK_EQ(config.window.height, 200);
    BT_CHECK_EQ(strcmp(config.window.title, "w"), 0);
    BT_CHECK_EQ(config.memo.cadence_ms, 250);
    BT_CHECK_EQ(strcmp(config.memo.mode, "syllables"), 0);
    BT_CHECK_EQ(config.drill.reps, 2);
    BT_CHECK_EQ(config.smoke_seed, 20261012);
    bt_json_free(value);
    bt_json_value *pinned = parse("{\"app\":{\"window\":{\"width\":1,\"height\":2,"
                                  "\"title\":\"w\"},\"memo\":{\"cadence_ms\":1,"
                                  "\"mode\":\"images\"},\"drill\":{\"reps\":1},"
                                  "\"smoke_seed\":99}}");
    rc = bt_app_config_from_json(pinned, &config, error, sizeof error);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(config.smoke_seed, 99);
    bt_json_free(pinned);
}

BT_TEST(config, committed_render_section_holds_no_capture_dwell_frames) {
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    bt_json_value *value = NULL;
    char error[192];
    BT_CHECK_EQ(bt_app_load_config_value(path, &value, error, sizeof error), 0);
    const bt_json_value *render = bt_json_get(bt_json_get(value, "app"), "render");
    BT_CHECK(render != NULL && render->type == BT_JSON_OBJECT);
    BT_CHECK(bt_json_get(render, "capture_dwell_frames") == NULL);
    BT_CHECK(bt_json_get(render, "target_fps") != NULL);
    BT_CHECK(bt_json_get(render, "vsync") != NULL);
    bt_json_free(value);
}

BT_TEST(config, from_json_rejects_malformed_and_missing_sections) {
    bt_app_config config;
    char error[96];
    bt_json_value *malformed = parse("{\"app\":{\"window\":null}}");
    int rc = bt_app_config_from_json(malformed, &config, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strncmp(error, "config app section malformed", 28) == 0);
    bt_json_free(malformed);
    bt_json_value *missing = parse("{}");
    rc = bt_app_config_from_json(missing, &config, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK_EQ(strcmp(error, "config app section missing"), 0);
    bt_json_free(missing);
}

BT_TEST(config, display_path_relativizes_only_inside_the_workspace) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char inside[BT_PATH_MAX];
    snprintf(inside, sizeof inside, "%s\\crates\\bt-app\\Cargo.toml", root);
    char shown[BT_PATH_MAX];
    bt_app_display_path(inside, shown, sizeof shown);
    BT_CHECK_EQ(strcmp(shown, "crates\\bt-app\\Cargo.toml"), 0);
    const char *outside = "/definitely-not-the-workspace/config.json";
    bt_app_display_path(outside, shown, sizeof shown);
    BT_CHECK_EQ(strcmp(shown, outside), 0);
}

BT_TEST(config, load_config_value_reports_read_and_parse_errors) {
    char temp[BT_PATH_MAX];
    snprintf(temp, sizeof temp, "%s\\bt-app-config-test-%d", getenv("TEMP"), _getpid());
    _mkdir(temp);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s\\config.json", temp);
    FILE *file = fopen(path, "wb");
    fputs("{\"app\":{\"memo\":{\"mode\":\"images\"}}}", file);
    fclose(file);
    bt_json_value *value = NULL;
    char error[192];
    int rc = bt_app_load_config_value(path, &value, error, sizeof error);
    BT_CHECK_EQ(rc, 0);
    bt_json_value *mode = bt_json_get(bt_json_get(bt_json_get(value, "app"), "memo"), "mode");
    BT_CHECK(mode != NULL && mode->type == BT_JSON_STRING);
    BT_CHECK_EQ(strcmp(mode->as.string, "images"), 0);
    bt_json_free(value);

    char absent[BT_PATH_MAX];
    snprintf(absent, sizeof absent, "%s\\absent.json", temp);
    rc = bt_app_load_config_value(absent, &value, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(error, "cannot read config") != NULL);

    file = fopen(path, "wb");
    fputs("{not json", file);
    fclose(file);
    rc = bt_app_load_config_value(path, &value, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(error, "cannot parse config") != NULL);
    remove(path);
    _rmdir(temp);
}
