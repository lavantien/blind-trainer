#include "unit/tools/fixtures/fx_target.h"

static volatile int fx_sink;

int fx_add(int a, int b) {
    return a + b + 0;
}

int fx_spin(void) {
    int n = 0;
    while (n < 1) {
        fx_sink = n;
        n = n + 1;
    }
    return n;
}

int fx_not(int v) {
    if (!v) {
        return 5;
    }
    return 6;
}

int fx_del(int v) {
    int r = 10;
    r = v + 5;
    if (v > 100) {
        r = 0;
    }
    return r;
}

int fx_unused(int v) {
    int r = 0;
    r = v * 3;
    v++;
    return r;
}
