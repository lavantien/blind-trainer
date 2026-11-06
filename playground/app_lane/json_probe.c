#include "common/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *names[6] = {"bad_fps", "negative", "bad_vsync", "bad_section", "good", NULL};
    const char *texts[6] = {
        "{\"app\":{\"render\":{\"target_fps\":\"fast\"}}}",
        "{\"app\":{\"render\":{\"target_fps\":-1}}}",
        "{\"app\":{\"render\":{\"vsync\":\"on\"}}}",
        "{\"app\":{\"render\":60}}",
        "{\"app\":{\"render\":{\"target_fps\":144,\"vsync\":true}}}",
        NULL,
    };
    int only = argc > 1 ? atoi(argv[1]) : -1;
    for (int i = 0; texts[i] != NULL; i++) {
        if (only >= 0 && i != only) {
            continue;
        }
        bt_json_value *value = NULL;
        bt_json_error error;
        int rc = bt_json_parse(texts[i], strlen(texts[i]), &value, &error);
        printf("%s rc=%d at %zu:%zu msg=%s\n", names[i], rc, error.line, error.column,
               rc == 0 ? "-" : error.message);
        if (rc == 0) {
            bt_json_free(value);
        }
    }
    return 0;
}
