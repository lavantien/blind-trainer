#include "app/config.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BT_APP_DEFAULT_WINDOW_WIDTH 1280
#define BT_APP_DEFAULT_WINDOW_HEIGHT 800
#define BT_APP_DEFAULT_WINDOW_TITLE "blind trainer"
#define BT_APP_DEFAULT_MEMO_CADENCE_MS 800
#define BT_APP_DEFAULT_MEMO_MODE "per-orbit"
#define BT_APP_DEFAULT_DRILL_REPS 6
#define BT_APP_DEFAULT_SMOKE_SEED 20261012

void bt_app_config_defaults(bt_app_config *config) {
    BT_TRACE_ENTER("app.config_defaults");
    config->window.width = BT_APP_DEFAULT_WINDOW_WIDTH;
    config->window.height = BT_APP_DEFAULT_WINDOW_HEIGHT;
    snprintf(config->window.title, sizeof config->window.title, "%s",
             BT_APP_DEFAULT_WINDOW_TITLE);
    config->memo.cadence_ms = BT_APP_DEFAULT_MEMO_CADENCE_MS;
    snprintf(config->memo.mode, sizeof config->memo.mode, "%s", BT_APP_DEFAULT_MEMO_MODE);
    config->drill.reps = BT_APP_DEFAULT_DRILL_REPS;
    config->smoke_seed = BT_APP_DEFAULT_SMOKE_SEED;
    BT_TRACE_EXIT("app.config_defaults");
}

static int field_uint(const bt_json_value *section, const char *name, unsigned int *out,
                      char *error, size_t error_len) {
    const bt_json_value *field = bt_json_get(section, name);
    if (field == NULL || field->type != BT_JSON_NUMBER || !bt_json_is_integer(field) ||
        field->integer < 0 || field->integer > 0xFFFFFFFFLL) {
        snprintf(error, error_len, "config app section malformed: field %s", name);
        BT_TRACE_FAIL("app.config_from_json", "field %s invalid", name);
        return -1;
    }
    *out = (unsigned int)field->integer;
    return 0;
}

static int field_text(const bt_json_value *section, const char *name, char *out,
                      size_t out_len, char *error, size_t error_len) {
    const bt_json_value *field = bt_json_get(section, name);
    if (field == NULL || field->type != BT_JSON_STRING ||
        strlen(field->as.string) >= out_len) {
        snprintf(error, error_len, "config app section malformed: field %s", name);
        BT_TRACE_FAIL("app.config_from_json", "field %s invalid", name);
        return -1;
    }
    snprintf(out, out_len, "%s", field->as.string);
    return 0;
}

static int window_from(const bt_json_value *section, bt_app_window_cfg *window, char *error,
                       size_t error_len) {
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_len, "config app section malformed: window");
        BT_TRACE_FAIL("app.config_from_json", "window section invalid");
        return -1;
    }
    if (field_uint(section, "width", &window->width, error, error_len) != 0 ||
        field_uint(section, "height", &window->height, error, error_len) != 0 ||
        field_text(section, "title", window->title, sizeof window->title, error,
                   error_len) != 0) {
        return -1;
    }
    return 0;
}

static int memo_from(const bt_json_value *section, bt_app_memo_cfg *memo, char *error,
                     size_t error_len) {
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_len, "config app section malformed: memo");
        BT_TRACE_FAIL("app.config_from_json", "memo section invalid");
        return -1;
    }
    if (field_uint(section, "cadence_ms", &memo->cadence_ms, error, error_len) != 0 ||
        field_text(section, "mode", memo->mode, sizeof memo->mode, error, error_len) != 0) {
        return -1;
    }
    return 0;
}

static int drill_from(const bt_json_value *section, bt_app_drill_cfg *drill, char *error,
                      size_t error_len) {
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_len, "config app section malformed: drill");
        BT_TRACE_FAIL("app.config_from_json", "drill section invalid");
        return -1;
    }
    return field_uint(section, "reps", &drill->reps, error, error_len);
}

int bt_app_config_from_json(const bt_json_value *value, bt_app_config *out, char *error,
                            size_t error_len) {
    BT_TRACE_ENTER("app.config_from_json");
    const bt_json_value *section = bt_json_get(value, "app");
    if (section == NULL) {
        snprintf(error, error_len, "config app section missing");
        BT_TRACE_FAIL("app.config_from_json", "app section missing");
        BT_TRACE_EXIT("app.config_from_json");
        return -1;
    }
    bt_app_config parsed;
    bt_app_config_defaults(&parsed);
    if (window_from(bt_json_get(section, "window"), &parsed.window, error, error_len) != 0 ||
        memo_from(bt_json_get(section, "memo"), &parsed.memo, error, error_len) != 0 ||
        drill_from(bt_json_get(section, "drill"), &parsed.drill, error, error_len) != 0) {
        BT_TRACE_EXIT("app.config_from_json");
        return -1;
    }
    const bt_json_value *seed = bt_json_get(section, "smoke_seed");
    if (seed != NULL) {
        if (seed->type != BT_JSON_NUMBER || !bt_json_is_integer(seed) || seed->integer < 0) {
            snprintf(error, error_len, "config app section malformed: field smoke_seed");
            BT_TRACE_FAIL("app.config_from_json", "field smoke_seed invalid");
            BT_TRACE_EXIT("app.config_from_json");
            return -1;
        }
        parsed.smoke_seed = (unsigned long long)seed->integer;
    }
    *out = parsed;
    BT_TRACE_EXIT("app.config_from_json");
    return 0;
}

const char *bt_app_display_path(const char *path, char *out, size_t out_len) {
    BT_TRACE_ENTER("app.display_path");
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) == 0 && path[root_len] == '\\') {
        snprintf(out, out_len, "%s", path + root_len + 1);
    } else {
        snprintf(out, out_len, "%s", path);
    }
    BT_TRACE_EXIT("app.display_path");
    return out;
}

int bt_app_load_config_value(const char *path, bt_json_value **out, char *error,
                             size_t error_len) {
    BT_TRACE_ENTER("app.load_config_value");
    char shown[BT_PATH_MAX];
    bt_app_display_path(path, shown, sizeof shown);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(error, error_len, "cannot read config %s", shown);
        BT_TRACE_FAIL("app.load_config_value", "cannot read %s", shown);
        BT_TRACE_EXIT("app.load_config_value");
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        snprintf(error, error_len, "cannot read config %s", shown);
        BT_TRACE_FAIL("app.load_config_value", "out of memory for %s", shown);
        BT_TRACE_EXIT("app.load_config_value");
        return -1;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';
    bt_json_error json_error;
    int rc = bt_json_parse(text, read, out, &json_error);
    free(text);
    if (rc != 0) {
        snprintf(error, error_len, "cannot parse config %s: %s", shown, json_error.message);
        BT_TRACE_FAIL("app.load_config_value", "cannot parse %s: %s", shown,
                      json_error.message);
        BT_TRACE_EXIT("app.load_config_value");
        return -1;
    }
    BT_TRACE_EXIT("app.load_config_value");
    return 0;
}
