#ifndef UNIT_TOOLS_RELEASE_VERIFY_TEST_UTIL_H
#define UNIT_TOOLS_RELEASE_VERIFY_TEST_UTIL_H

#include "common/bt_harness.h"
#include "common/paths.h"
#include "release-verify/zip.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static inline void rv_util_mkdir_p(const char *path) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            CreateDirectoryA(copy, NULL);
            *cursor = '/';
        }
    }
    CreateDirectoryA(copy, NULL);
}

static inline void rv_util_rm_rf(const char *path) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        RemoveDirectoryA(path);
        return;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char child[BT_PATH_MAX];
        snprintf(child, sizeof child, "%s/%s", path, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rv_util_rm_rf(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    RemoveDirectoryA(path);
}

static inline void rv_util_scratch(const char *tag, char out[BT_PATH_MAX]) {
    char cwd[BT_PATH_MAX];
    GetCurrentDirectoryA(sizeof cwd, cwd);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/release-verify-%s", cwd, tag);
    rv_util_rm_rf(out);
    rv_util_mkdir_p(out);
}

static inline void rv_util_join(char out[BT_PATH_MAX], const char *dir, const char *leaf) {
    snprintf(out, BT_PATH_MAX, "%s/%s", dir, leaf);
}

static inline void rv_util_write_bytes(const char *path, const unsigned char *data, size_t len) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "cannot write %s", path);
        return;
    }
    if (fwrite(data, 1, len, file) != len) {
        bt_harness_fail(__FILE__, __LINE__, "short write %s", path);
    }
    fclose(file);
}

static inline void rv_util_write(const char *path, const char *text) {
    rv_util_write_bytes(path, (const unsigned char *)text, strlen(text));
}

static inline int rv_util_file_bytes(const char *path, unsigned char **out, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc(size > 0 ? (size_t)size : 1);
    if (data == NULL || (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size)) {
        fclose(file);
        free(data);
        return -1;
    }
    fclose(file);
    *out = data;
    *out_len = (size_t)size;
    return 0;
}

static inline int rv_util_exists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static inline uint32_t rv_util_adler32(const unsigned char *data, size_t len) {
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t at = 0; at < len; at++) {
        a = (a + data[at]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

typedef struct {
    const char *name;
    const unsigned char *data;
    size_t len;
} rv_zip_entry;

static inline void rv_util_zip_append_local(FILE *file, const rv_zip_entry *entry,
                                            uint32_t *crc_out, uint32_t *offset_out) {
    uint32_t crc = bt_rv_crc32(entry->data, entry->len);
    long offset = ftell(file);
    unsigned char head[30];
    memset(head, 0, sizeof head);
    uint32_t sig = 0x04034b50;
    memcpy(head + 0, &sig, 4);
    uint16_t version = 20;
    memcpy(head + 4, &version, 2);
    uint16_t method = 0;
    memcpy(head + 8, &method, 2);
    uint32_t csize = (uint32_t)entry->len;
    memcpy(head + 14, &crc, 4);
    memcpy(head + 18, &csize, 4);
    memcpy(head + 22, &csize, 4);
    uint16_t name_len = (uint16_t)strlen(entry->name);
    memcpy(head + 26, &name_len, 2);
    fwrite(head, 1, sizeof head, file);
    fwrite(entry->name, 1, name_len, file);
    if (entry->len > 0) {
        fwrite(entry->data, 1, entry->len, file);
    }
    *crc_out = crc;
    *offset_out = (uint32_t)offset;
}

static inline void rv_util_write_zip(const char *path, const rv_zip_entry *entries, size_t count) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "cannot write zip %s", path);
        return;
    }
    uint32_t crcs[64];
    uint32_t offsets[64];
    for (size_t at = 0; at < count; at++) {
        rv_util_zip_append_local(file, &entries[at], &crcs[at], &offsets[at]);
    }
    long cd_start = ftell(file);
    for (size_t at = 0; at < count; at++) {
        unsigned char head[46];
        memset(head, 0, sizeof head);
        uint32_t sig = 0x02014b50;
        memcpy(head + 0, &sig, 4);
        uint16_t version = 20;
        memcpy(head + 4, &version, 2);
        memcpy(head + 6, &version, 2);
        uint32_t csize = (uint32_t)entries[at].len;
        memcpy(head + 16, &crcs[at], 4);
        memcpy(head + 20, &csize, 4);
        memcpy(head + 24, &csize, 4);
        uint16_t name_len = (uint16_t)strlen(entries[at].name);
        memcpy(head + 28, &name_len, 2);
        uint32_t offset = offsets[at];
        memcpy(head + 42, &offset, 4);
        fwrite(head, 1, sizeof head, file);
        fwrite(entries[at].name, 1, name_len, file);
    }
    long cd_size = ftell(file) - cd_start;
    unsigned char eocd[22];
    memset(eocd, 0, sizeof eocd);
    uint32_t sig = 0x06054b50;
    memcpy(eocd + 0, &sig, 4);
    uint16_t total = (uint16_t)count;
    memcpy(eocd + 8, &total, 2);
    memcpy(eocd + 10, &total, 2);
    uint32_t cd_size32 = (uint32_t)cd_size;
    memcpy(eocd + 12, &cd_size32, 4);
    uint32_t cd_start32 = (uint32_t)cd_start;
    memcpy(eocd + 16, &cd_start32, 4);
    fwrite(eocd, 1, sizeof eocd, file);
    fclose(file);
}

static inline void rv_util_write_png(const char *path, uint32_t width, uint32_t height) {
    size_t row_bytes = (size_t)width + 1;
    size_t raw_len = row_bytes * height;
    unsigned char *raw = malloc(raw_len);
    if (raw == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "out of memory for %s", path);
        return;
    }
    for (uint32_t line = 0; line < height; line++) {
        unsigned char *row = raw + (size_t)line * row_bytes;
        row[0] = 0;
        memset(row + 1, 9, width);
    }
    size_t block_count = (raw_len + 65534) / 65535;
    size_t idat_len = 2 + block_count * 5 + raw_len + 4;
    unsigned char *idat = malloc(idat_len);
    if (idat == NULL) {
        free(raw);
        bt_harness_fail(__FILE__, __LINE__, "out of memory for %s", path);
        return;
    }
    size_t at = 0;
    idat[at++] = 0x78;
    idat[at++] = 0x01;
    size_t remaining = raw_len;
    const unsigned char *cursor = raw;
    while (remaining > 0) {
        size_t take = remaining > 65535 ? 65535 : remaining;
        idat[at++] = remaining - take == 0 ? 1 : 0;
        uint16_t len16 = (uint16_t)take;
        memcpy(idat + at, &len16, 2);
        at += 2;
        uint16_t nlen16 = (uint16_t)~take;
        memcpy(idat + at, &nlen16, 2);
        at += 2;
        memcpy(idat + at, cursor, take);
        at += take;
        cursor += take;
        remaining -= take;
    }
    uint32_t adler = rv_util_adler32(raw, raw_len);
    idat[at++] = (unsigned char)(adler >> 24);
    idat[at++] = (unsigned char)(adler >> 16);
    idat[at++] = (unsigned char)(adler >> 8);
    idat[at++] = (unsigned char)adler;
    free(raw);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        free(idat);
        bt_harness_fail(__FILE__, __LINE__, "cannot write png %s", path);
        return;
    }
    static const unsigned char signature[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    fwrite(signature, 1, 8, file);
    unsigned char ihdr[13];
    ihdr[0] = (unsigned char)(width >> 24);
    ihdr[1] = (unsigned char)(width >> 16);
    ihdr[2] = (unsigned char)(width >> 8);
    ihdr[3] = (unsigned char)width;
    ihdr[4] = (unsigned char)(height >> 24);
    ihdr[5] = (unsigned char)(height >> 16);
    ihdr[6] = (unsigned char)(height >> 8);
    ihdr[7] = (unsigned char)height;
    ihdr[8] = 8;
    ihdr[9] = 0;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    unsigned char type_and_data[17];
    memcpy(type_and_data, "IHDR", 4);
    memcpy(type_and_data + 4, ihdr, 13);
    unsigned char head[8];
    head[0] = 0;
    head[1] = 0;
    head[2] = 0;
    head[3] = 13;
    memcpy(head + 4, "IHDR", 4);
    fwrite(head, 1, 8, file);
    fwrite(ihdr, 1, 13, file);
    uint32_t crc = bt_rv_crc32(type_and_data, 17);
    unsigned char crc_be[4];
    crc_be[0] = (unsigned char)(crc >> 24);
    crc_be[1] = (unsigned char)(crc >> 16);
    crc_be[2] = (unsigned char)(crc >> 8);
    crc_be[3] = (unsigned char)crc;
    fwrite(crc_be, 1, 4, file);

    unsigned char idat_head[8];
    uint32_t idat_len32 = (uint32_t)idat_len;
    idat_head[0] = (unsigned char)(idat_len32 >> 24);
    idat_head[1] = (unsigned char)(idat_len32 >> 16);
    idat_head[2] = (unsigned char)(idat_len32 >> 8);
    idat_head[3] = (unsigned char)idat_len32;
    memcpy(idat_head + 4, "IDAT", 4);
    fwrite(idat_head, 1, 8, file);
    fwrite(idat, 1, idat_len, file);
    unsigned char *type_and_idat = malloc(idat_len + 4);
    memcpy(type_and_idat, "IDAT", 4);
    memcpy(type_and_idat + 4, idat, idat_len);
    uint32_t idat_crc = bt_rv_crc32(type_and_idat, idat_len + 4);
    free(type_and_idat);
    free(idat);
    crc_be[0] = (unsigned char)(idat_crc >> 24);
    crc_be[1] = (unsigned char)(idat_crc >> 16);
    crc_be[2] = (unsigned char)(idat_crc >> 8);
    crc_be[3] = (unsigned char)idat_crc;
    fwrite(crc_be, 1, 4, file);

    unsigned char iend_head[8];
    iend_head[0] = 0;
    iend_head[1] = 0;
    iend_head[2] = 0;
    iend_head[3] = 0;
    memcpy(iend_head + 4, "IEND", 4);
    fwrite(iend_head, 1, 8, file);
    unsigned char iend_tail[4];
    memcpy(iend_tail, "IEND", 4);
    uint32_t iend_crc = bt_rv_crc32(iend_tail, 4);
    crc_be[0] = (unsigned char)(iend_crc >> 24);
    crc_be[1] = (unsigned char)(iend_crc >> 16);
    crc_be[2] = (unsigned char)(iend_crc >> 8);
    crc_be[3] = (unsigned char)iend_crc;
    fwrite(crc_be, 1, 4, file);
    fclose(file);
}

#endif
