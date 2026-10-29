#include "common/bt_harness.h"
#include "common/config.h"

#ifdef _WIN32
#include <stdlib.h>
#define BT_SETENV(name, value) _putenv_s(name, value)
#define BT_UNSETENV(name) _putenv_s(name, "")
#else
#include <stdlib.h>
#define BT_SETENV(name, value) setenv(name, value, 1)
#define BT_UNSETENV(name) unsetenv(name)
#endif

static void write_fixture(const char *name, const char *body) {
    FILE *file = bt_test_open_scratch(name);
    BT_CHECK(file != NULL);
    fputs(body, file);
    fclose(file);
}

static const char *FIXTURE = "{"
                             "\"app\": {\"reps\": 6, \"ratio\": 0.25, \"on\": true, "
                             "\"title\": \"blind trainer\", \"list\": [1, 2]}, "
                             "\"top\": 41"
                             "}";

static const char *FIXTURE_ALT = "{"
                                 "\"app\": {\"reps\": 7, \"ratio\": 0.25, \"on\": true, "
                                 "\"title\": \"blind trainer\", \"list\": [1, 2]}, "
                                 "\"top\": 41"
                                 "}";

static void load_fixture(void) {
    write_fixture("config_fixture.json", FIXTURE);
    char path[256];
    snprintf(path, sizeof path, "build/test-tmp/config_fixture.json");
    BT_SETENV("BT_CONFIG", path);
    bt_config_reset();
}

static void clear_config_env(void) {
    BT_UNSETENV("BT_CONFIG");
    bt_config_reset();
}

BT_TEST(config, loads_hub_with_typed_accessors) {
    load_fixture();
    const bt_json_value *cfg = bt_config_load();
    BT_CHECK(cfg != NULL);
    long long reps = 0;
    BT_CHECK_EQ(bt_config_int(cfg, "app.reps", &reps), 0);
    BT_CHECK_EQ(reps, 6);
    double ratio = 0;
    BT_CHECK_EQ(bt_config_f64(cfg, "app.ratio", &ratio), 0);
    BT_CHECK(ratio == 0.25);
    int on = 0;
    BT_CHECK_EQ(bt_config_bool(cfg, "app.on", &on), 0);
    BT_CHECK_EQ(on, 1);
    const char *title = NULL;
    BT_CHECK_EQ(bt_config_str(cfg, "app.title", &title), 0);
    BT_CHECK(title != NULL && strcmp(title, "blind trainer") == 0);
    long long top = 0;
    BT_CHECK_EQ(bt_config_int(cfg, "top", &top), 0);
    BT_CHECK_EQ(top, 41);
}

BT_TEST(config, wrong_type_and_missing_paths_report_not_found) {
    load_fixture();
    const bt_json_value *cfg = bt_config_load();
    const char *text = NULL;
    BT_CHECK_EQ(bt_config_str(cfg, "app.reps", &text), -1);
    long long number = 0;
    BT_CHECK_EQ(bt_config_int(cfg, "app.title", &number), -1);
    BT_CHECK_EQ(bt_config_int(cfg, "app.missing", &number), -1);
    BT_CHECK_EQ(bt_config_int(cfg, "no.such.path", &number), -1);
    int flag = 0;
    BT_CHECK_EQ(bt_config_bool(cfg, "top", &flag), -1);
    const bt_json_value *list = bt_config_value(cfg, "app.list");
    BT_CHECK(list != NULL);
    BT_CHECK_EQ(list->type, BT_JSON_ARRAY);
}

BT_TEST(config, load_is_cached_until_reset) {
    load_fixture();
    const bt_json_value *first = bt_config_load();
    const bt_json_value *second = bt_config_load();
    BT_CHECK(first == second);
    long long reps = 0;
    BT_CHECK_EQ(bt_config_int(first, "app.reps", &reps), 0);
    BT_CHECK_EQ(reps, 6);
    bt_config_reset();
    write_fixture("config_fixture.json", FIXTURE_ALT);
    const bt_json_value *third = bt_config_load();
    BT_CHECK(third != NULL);
    BT_CHECK_EQ(bt_config_int(third, "app.reps", &reps), 0);
    BT_CHECK_EQ(reps, 7);
}

BT_TEST(config, try_load_rejects_missing_file_and_bad_json) {
    bt_json_value *value = NULL;
    BT_CHECK_EQ(bt_config_try_load("build/test-tmp/definitely-absent.json", &value), -1);
    write_fixture("config_bad.json", "{\"a\": }");
    BT_CHECK_EQ(bt_config_try_load("build/test-tmp/config_bad.json", &value), -1);
}

BT_TEST(config, default_hub_loads_without_env_override) {
    clear_config_env();
    const bt_json_value *cfg = bt_config_load();
    BT_CHECK(cfg != NULL);
    long long width = 0;
    BT_CHECK_EQ(bt_config_int(cfg, "app.window.width", &width), 0);
    BT_CHECK_EQ(width, 1280);
    long long ring_bytes = 0;
    BT_CHECK_EQ(bt_config_int(cfg, "trace.ring_bytes", &ring_bytes), 0);
    BT_CHECK_EQ(ring_bytes, 65536);
    clear_config_env();
}
