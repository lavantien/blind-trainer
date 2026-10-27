#include "unit/tools/fixtures/fx_target.h"

int main(void) {
    if (fx_add(1, 2) != 3) {
        return 1;
    }
    if (fx_spin() != 1) {
        return 2;
    }
    if (fx_not(0) != 5) {
        return 3;
    }
    if (fx_del(2) != 7) {
        return 4;
    }
    return 0;
}
