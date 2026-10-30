#include "e2e/png.h"

#include "common/bt_trace.h"
#include "e2e/proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char BT_PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};

static unsigned int crc32_of(const unsigned char *data, size_t len, unsigned int crc) {
    crc ^= 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= (unsigned)data[i];
        for (int bit = 0; bit < 8; bit++) {
            unsigned int mask = (crc & 1U) ? 0xEDB88320U : 0U;
            crc = (crc >> 1U) ^ mask;
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static unsigned int read_be32(const unsigned char *bytes) {
    return ((unsigned)bytes[0] << 24U) | ((unsigned)bytes[1] << 16U) | ((unsigned)bytes[2] << 8U) |
           (unsigned)bytes[3];
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
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 8) {
        fclose(file);
        BT_TRACE_FAIL("e2e.png_dims", "shorter than the signature: %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    unsigned char *data = malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        BT_TRACE_FAIL("e2e.png_dims", "cannot read %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    fclose(file);
    if (memcmp(data, BT_PNG_SIGNATURE, 8) != 0) {
        free(data);
        BT_TRACE_FAIL("e2e.png_dims", "bad signature: %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    size_t at = 8;
    int ihdr_seen = 0;
    int idat_seen = 0;
    int iend_seen = 0;
    while (at + 8 <= (size_t)size && !iend_seen) {
        unsigned int chunk_len = read_be32(data + at);
        if (chunk_len > 0x7FFFFFFFU ||
            (unsigned long long)at + 12ULL + chunk_len > (unsigned long long)(size_t)size) {
            break;
        }
        const unsigned char *type = data + at + 4;
        unsigned int crc_actual = crc32_of(data + at + 4, (size_t)chunk_len + 4, 0);
        unsigned int crc_stored = read_be32(data + at + 8 + chunk_len);
        if (crc_actual != crc_stored) {
            break;
        }
        if (at == 8) {
            if (memcmp(type, "IHDR", 4) != 0 || chunk_len != 13) {
                break;
            }
            *width = read_be32(data + at + 8);
            *height = read_be32(data + at + 12);
            ihdr_seen = 1;
        } else if (memcmp(type, "IHDR", 4) == 0) {
            break;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            idat_seen = 1;
        } else if (memcmp(type, "IEND", 4) == 0) {
            iend_seen = 1;
        }
        at += 12 + (size_t)chunk_len;
    }
    free(data);
    if (!ihdr_seen || !idat_seen || !iend_seen) {
        BT_TRACE_FAIL("e2e.png_dims", "chunk walk failed: %s", path);
        BT_TRACE_EXIT("e2e.png_dims");
        return -1;
    }
    BT_TRACE_EXIT("e2e.png_dims");
    return 0;
}
