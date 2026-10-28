#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void show(const char *key) {
    const char *value = getenv(key);
    if (value == NULL) {
        printf("%s=<unset>\n", key);
        return;
    }
    if (strlen(value) > 12) {
        printf("%s=%.12s...len=%zu\n", key, value, strlen(value));
    } else {
        printf("%s=%s\n", key, value);
    }
}

int main(void) {
    show("APPDATA");
    show("BT_CONFIG");
    show("ADV21A_PAD_00");
    show("ADV21A_PAD_15");
    fflush(stdout);
    return 0;
}
