#include <stdio.h>

#include "common.h"

int main(void) {
    tc_name_index names;
    tc_position_names(&names);
    for (size_t i = 0; i < 48; i++) {
        printf("%zu %s\n", i, names.sticker_keys[i]);
    }
    uint8_t s = tc_sticker_of_name(&names, "BUR");
    printf("BUR -> %u\n", s);
    return 0;
}
