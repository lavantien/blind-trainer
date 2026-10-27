#include "common/paths.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void show(const char *label, const char *value) {
    printf("%s: [%s]\n", label, value);
}

static void join_case(const char *base, const char *push) {
    char out[BT_PATH_MAX];
    bt_paths_join(base, push, out);
    char label[256];
    snprintf(label, sizeof label, "join base=[%s] push=[%s]", base, push);
    show(label, out);
}

static void abs_case(const char *value) {
    char label[256];
    snprintf(label, sizeof label, "abs [%s]", value);
    printf("%s: %d\n", label, bt_paths_is_absolute(value));
}

int main(void) {
    abs_case("\\\\?\\C:\\x");
    abs_case("\\\\?\\C:");
    abs_case("\\\\?\\UNC\\srv\\sh\\x");
    abs_case("x:y");
    abs_case("//s/sh");
    abs_case("C:/");
    join_case("C:\\repo\\", "x");
    join_case("C:", "x");
    join_case("a", "");
    join_case("R:/ws", "/out");
    join_case("C:\\", "/out");
    join_case("\\\\srv\\sh", "/leaf");
    char big[700];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    join_case(big, big);
    printf("truncation length probe done\n");
    return fails;
}
