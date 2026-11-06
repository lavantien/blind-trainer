#include "common/json.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *text =
        "{\"cases\": [{\"orbit\": \"edge\", \"letters\": \"ab\", \"count\": 3}], \"adjacent\": [], "
        "\"cooccurring\": []}";
    bt_json_value *value = NULL;
    bt_json_error error;
    int rc = bt_json_parse(text, strlen(text), &value, &error);
    printf("rc=%d line=%zu col=%zu msg=%s\n", rc, error.line, error.column, error.message);
    if (rc == 0) {
        printf("root keys cases=%d adjacent=%d cooc=%d\n", bt_json_get(value, "cases") != NULL,
               bt_json_get(value, "adjacent") != NULL, bt_json_get(value, "cooccurring") != NULL);
        bt_json_free(value);
    }
    return 0;
}
