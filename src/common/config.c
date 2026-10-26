#include "common/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

#define BT_CONFIG_DEFAULT_PATH "config/config.json"

static bt_json_value *g_config;

static char *read_file(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    *len = (size_t)size;
    return text;
}

int bt_config_try_load(const char *path, bt_json_value **out) {
    BT_TRACE_ENTER("config.try_load");
    *out = NULL;
    size_t len = 0;
    char *text = read_file(path, &len);
    if (text == NULL) {
        BT_TRACE_FAIL("config.try_load", "cannot read %s", path);
        BT_TRACE_EXIT("config.try_load");
        return -1;
    }
    bt_json_error error;
    int rc = bt_json_parse(text, len, out, &error);
    free(text);
    if (rc != 0) {
        BT_TRACE_FAIL("config.try_load", "invalid json in %s at %zu:%zu: %s", path, error.line,
                      error.column, error.message);
    }
    BT_TRACE_EXIT("config.try_load");
    return rc;
}

const bt_json_value *bt_config_load(void) {
    BT_TRACE_ENTER("config.load");
    if (g_config != NULL) {
        BT_TRACE_EXIT("config.load");
        return g_config;
    }
    const char *override = getenv("BT_CONFIG");
    const char *path =
        (override != NULL && override[0] != '\0') ? override : BT_CONFIG_DEFAULT_PATH;
    bt_json_value *value = NULL;
    if (bt_config_try_load(path, &value) != 0) {
        fprintf(stderr, "config: fatal, cannot load %s\n", path);
        fflush(stderr);
        exit(1);
    }
    g_config = value;
    BT_TRACE_EXIT("config.load");
    return g_config;
}

void bt_config_reset(void) {
    BT_TRACE_ENTER("config.reset");
    bt_json_free(g_config);
    g_config = NULL;
    BT_TRACE_EXIT("config.reset");
}

const bt_json_value *bt_config_value(const bt_json_value *config, const char *dotted_path) {
    if (config == NULL || dotted_path == NULL) {
        return NULL;
    }
    const bt_json_value *current = config;
    const char *start = dotted_path;
    while (1) {
        const char *dot = strchr(start, '.');
        size_t span = dot == NULL ? strlen(start) : (size_t)(dot - start);
        if (span == 0 || current->type != BT_JSON_OBJECT) {
            return NULL;
        }
        char key[128];
        if (span >= sizeof key) {
            return NULL;
        }
        memcpy(key, start, span);
        key[span] = '\0';
        current = bt_json_get(current, key);
        if (current == NULL) {
            return NULL;
        }
        if (dot == NULL) {
            return current;
        }
        start = dot + 1;
    }
}

int bt_config_str(const bt_json_value *config, const char *dotted_path, const char **out) {
    const bt_json_value *value = bt_config_value(config, dotted_path);
    if (value == NULL || value->type != BT_JSON_STRING) {
        return -1;
    }
    *out = value->as.string;
    return 0;
}

int bt_config_int(const bt_json_value *config, const char *dotted_path, long long *out) {
    const bt_json_value *value = bt_config_value(config, dotted_path);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return -1;
    }
    double number = value->as.number;
    if (number != (double)(long long)number) {
        return -1;
    }
    *out = (long long)number;
    return 0;
}

int bt_config_f64(const bt_json_value *config, const char *dotted_path, double *out) {
    const bt_json_value *value = bt_config_value(config, dotted_path);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return -1;
    }
    *out = value->as.number;
    return 0;
}

int bt_config_bool(const bt_json_value *config, const char *dotted_path, int *out) {
    const bt_json_value *value = bt_config_value(config, dotted_path);
    if (value == NULL || value->type != BT_JSON_BOOL) {
        return -1;
    }
    *out = value->as.boolean;
    return 0;
}
