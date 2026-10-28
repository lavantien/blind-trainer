#include "e2e/lib.h"
#include "e2e/png.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char TD_PNG_3X2[] = {
    137, 80,  78,  71,  13,  10, 26, 10,  0,   0,   0,  13, 73,  72, 68, 82, 0,   0,
    0,   3,   0,   0,   0,   2,   8,   6,  0,   0,   0,   157, 116, 102, 26, 0,  0,  0,
    17,  73,  68,  65,  84, 120, 156, 99, 224, 228, 228, 252, 15, 195, 12,  200, 28, 0,
    77,  96,  6,   157, 65,  42,  41,  210, 0,   0,   0,   0,   73,  69,  78,  68, 174, 66,
    96,  130,
};

int main(void) {
    bt_trace_init(65536, 8, 256);
    const char *dir = getenv("TEMP");
    char path[1024];
    unsigned char corrupt[sizeof TD_PNG_3X2];
    memcpy(corrupt, TD_PNG_3X2, sizeof corrupt);
    corrupt[45] ^= 0xFF;
    snprintf(path, sizeof path, "%s\\adv21b_corrupt_idat.png", dir);
    FILE *file = fopen(path, "wb");
    fwrite(corrupt, 1, sizeof corrupt, file);
    fclose(file);
    for (int round = 1; round <= 3; round++) {
        unsigned int width = 0;
        unsigned int height = 0;
        int dims_ok = bt_png_dims(path, &width, &height);
        char reason[BT_E2E_ERROR_MAX];
        memset(reason, 0, sizeof reason);
        bt_e2e_window_dims dims = {3, 2};
        bt_e2e_shot_verdict verdict = bt_e2e_check_shot(path, dims, reason, sizeof reason);
        unsigned int width2 = 0;
        unsigned int height2 = 0;
        int dims_ok2 = bt_png_dims(path, &width2, &height2);
        printf("round %d: dims1=%d %ux%u verdict=%d reason=[%s] dims2=%d %ux%u\n", round, dims_ok,
               width, height, verdict, reason, dims_ok2, width2, height2);
    }
    for (size_t i = 33; i < sizeof corrupt; i += 7) {
        unsigned char mutated[sizeof TD_PNG_3X2];
        memcpy(mutated, TD_PNG_3X2, sizeof mutated);
        mutated[i] ^= 0xFF;
        FILE *out = fopen(path, "wb");
        fwrite(mutated, 1, sizeof mutated, out);
        fclose(out);
        char reason[BT_E2E_ERROR_MAX];
        memset(reason, 0, sizeof reason);
        bt_e2e_window_dims dims = {3, 2};
        bt_e2e_shot_verdict verdict = bt_e2e_check_shot(path, dims, reason, sizeof reason);
        if (verdict == 0) {
            printf("byte %zu flipped: PASS reason-empty\n", i);
        }
    }
    file = fopen(path, "wb");
    fwrite(TD_PNG_3X2, 1, sizeof TD_PNG_3X2, file);
    fclose(file);
    char reason[BT_E2E_ERROR_MAX];
    memset(reason, 0, sizeof reason);
    bt_e2e_window_dims dims = {3, 2};
    printf("restored verdict=%d\n", bt_e2e_check_shot(path, dims, reason, sizeof reason));
    bt_trace_shutdown();
    return 0;
}
