#include "common/paths.h"

#include <stdio.h>
#include <string.h>

static void show_join(const char *base, const char *push) {
    char out[BT_PATH_MAX];
    out[0] = '\0';
    bt_paths_join(base, push, out);
    printf("join base=[%s] push=[%s] -> [%s]\n", base, push, out);
}

static void show_abs(const char *path) {
    printf("abs [%s] -> %d\n", path, bt_paths_is_absolute(path));
}

int main(void) {
    show_abs("\\\\?\\C:\\x");
    show_abs("\\\\?\\C:");
    show_abs("//?/C:/x");
    show_abs("\\\\?\\UNC\\srv\\share\\x");
    show_join("R:/ws/", "leaf");
    show_join("R:/ws", "");
    show_join("C:", "/out");
    show_join("\\\\srv", "/out");
    show_join("C:ga", "/out");
    show_join("\\\\srv\\share", "/out");
    show_join("\\\\srv\\share\\data/", "/out");
    show_join("R:/ws", "C:x");
    show_join("rel/base", "/out");
    show_join("R:/ws", "C:\\x/y");
    char long_push[1400];
    memset(long_push, 'a', sizeof long_push - 1);
    long_push[sizeof long_push - 1] = '\0';
    long_push[0] = 'L';
    show_join("R:/ws", long_push);
    char out[BT_PATH_MAX];
    bt_paths_join("R:/ws", long_push, out);
    printf("long join output length %zu cap %d\n", strlen(out), BT_PATH_MAX);
    return 0;
}
