#include "e2e/png.h"

#include "common/bt_trace.h"
#include "e2e/proc.h"

#include <stdio.h>
#include <string.h>

static const unsigned char BT_PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};

static unsigned int read_be32(const unsigned char *bytes) {
    return ((unsigned int)bytes[0] << 24) | ((unsigned int)bytes[1] << 16) |
           ((unsigned int)bytes[2] << 8) | (unsigned int)bytes[3];
}

int bt_png_dims(const char *path, unsigned int *width, unsigned int *height) {
    BT_TRACE_ENTER("e2e.png_dims");
    wchar_t path_w[BT_E2E_PATH_WMAX];
    if (bt_e2e_utf8_to_wide(path, path_w, BT_E2E_PATH_WMAX) != 0) {
        BT_TRACE_FAIL("e2e.png_dims", "utf8 conversion failed for %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    FILE *file = _wfopen(path_w, L"rb");
    if (file == NULL) {
        BT_TRACE_FAIL("e2e.png_dims", "cannot open %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    unsigned char header[24];
    size_t read_len = fread(header, 1, sizeof header, file);
    fclose(file);
    if (read_len < sizeof header || memcmp(header, BT_PNG_SIGNATURE, 8) != 0 ||
        memcmp(header + 12, "IHDR", 4) != 0) {
        BT_TRACE_FAIL("e2e.png_dims", "not a png: %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    *width = read_be32(header + 16);
    *height = read_be32(header + 20);
    BT_TRACE_EXIT("e2e.png_dims");
    return 0;
}
