#include "common/json.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const char *body = "{\"memguard\":{\"pid_dir\":\"\xc2\xa0\"}}";
    bt_json_value *config = NULL;
    bt_json_error error;
    int rc = bt_json_parse(body, strlen(body), &config, &error);
    printf("parse rc %d\n", rc);
    if (rc != 0) {
        printf("error %s at %zu:%zu\n", error.message, error.line, error.column);
        return 1;
    }
    const bt_json_value *value = config;
    (void)value;
    bt_json_value *dir = bt_json_get(config, "memguard");
    dir = bt_json_get(dir, "pid_dir");
    printf("type %d string [%s]\n", dir->type, dir->as.string);
    for (const unsigned char *cursor = (const unsigned char *)dir->as.string; *cursor != 0;
         cursor++) {
        printf("%02x ", *cursor);
    }
    printf("\n");
    bt_json_free(config);
    return 0;
}
