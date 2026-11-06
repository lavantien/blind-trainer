#include "core/alg.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    err[0] = '\0';
    bool ok = bt_alg_parse("R Q", &alg, err, sizeof err);
    printf("ok=%d err='%s'\n", (int)ok, err);
    bt_alg_init(&alg);
    err[0] = '\0';
    ok = bt_alg_parse("Q", &alg, err, sizeof err);
    printf("ok=%d err='%s'\n", (int)ok, err);
    return 0;
}
