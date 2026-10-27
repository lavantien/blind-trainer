#include "common/config.h"
#include "common/json.h"
#include "common/paths.h"
#include "data/paths.h"
#include <process.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    _putenv_s("BT_CONFIG", argv[1]);
    bt_json_value *config = bt_data_load_config();
    fprintf(stderr, "config=%p\n", (void *)config);
    if (config == NULL) {
        return 0;
    }
    const bt_json_value *paths = bt_json_get(config, "paths");
    if (paths == NULL || paths->type != BT_JSON_OBJECT) {
        fprintf(stderr, "no paths object\n");
        bt_json_free(config);
        return 0;
    }
    for (size_t i = 0; i < paths->as.object.count; i++) {
        fprintf(stderr, "paths.%s = ", paths->as.object.keys[i]);
        const bt_json_value *v = paths->as.object.values[i];
        if (v->type == BT_JSON_STRING) {
            fprintf(stderr, "%s\n", v->as.string);
        } else {
            fprintf(stderr, "<%d>\n", (int)v->type);
        }
    }
    const char *data_ga = NULL;
    int rc = bt_config_str(config, "paths.data_ga", &data_ga);
    fprintf(stderr, "bt_config_str rc=%d data_ga=%s\n", rc, rc == 0 ? data_ga : "(none)");
    bt_json_free(config);
    return 0;
}
