#include "release-verify/zip.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "release-verify/inflate.h"

enum {
    RV_EOCD_MAX_BACK = 65557,
    RV_METHOD_STORE = 0,
    RV_METHOD_DEFLATE = 8,
    RV_CENTRAL_HEAD = 46,
};

typedef struct {
    char *name;
    char *path;
} rv_set_pair;

typedef struct {
    char name[BT_PATH_MAX];
    unsigned short method;
    uint32_t crc;
    uint32_t csize;
    uint32_t usize;
    uint32_t offset;
} rv_entry;

typedef struct {
    unsigned char *bytes;
    size_t len;
    rv_entry *entries;
    size_t count;
} rv_archive;

static void *rv_xmalloc(size_t size) {
    void *block = malloc(size > 0 ? size : 1);
    if (block == NULL) {
        fputs("bt-release-verify: out of memory\n", stderr);
        abort();
    }
    return block;
}

static char *rv_xstrdup(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = rv_xmalloc(len);
    memcpy(copy, text, len);
    return copy;
}

static uint16_t rv_u16(const unsigned char *at) {
    return (uint16_t)((unsigned)at[0] | ((unsigned)at[1] << 8U));
}

static uint32_t rv_u32(const unsigned char *at) {
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8U) | ((uint32_t)at[2] << 16U) |
           ((uint32_t)at[3] << 24U);
}

uint32_t bt_rv_crc32(const unsigned char *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t at = 0; at < len; at++) {
        crc ^= (uint32_t)data[at];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static int rv_is_file(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int rv_is_dir(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void rv_join(char out[BT_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
}

static int rv_read_bytes(const char *path, unsigned char **out, size_t *out_len, char *err,
                         size_t err_cap) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "cannot open %s: %s", path, strerror(errno));
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, err_cap, "cannot read %s: %s", path, strerror(errno));
        return -1;
    }
    unsigned char *data = rv_xmalloc((size_t)size);
    if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        snprintf(err, err_cap, "cannot read %s: short read", path);
        return -1;
    }
    fclose(file);
    *out = data;
    *out_len = (size_t)size;
    return 0;
}

static int rv_write_bytes(const char *path, const unsigned char *data, size_t len, char *err,
                          size_t err_cap) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        snprintf(err, err_cap, "write %s: %s", path, strerror(errno));
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, file) != len) {
        fclose(file);
        snprintf(err, err_cap, "write %s: short write", path);
        return -1;
    }
    fclose(file);
    return 0;
}

static void rv_mkdir_p(const char *path) {
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

static int rv_pair_cmp(const void *left, const void *right) {
    return strcmp(((const rv_set_pair *)left)->name, ((const rv_set_pair *)right)->name);
}

static rv_set_pair *rv_pairs_find(rv_set_pair *pairs, size_t count, const char *name) {
    for (size_t at = 0; at < count; at++) {
        if (strcmp(pairs[at].name, name) == 0) {
            return &pairs[at];
        }
    }
    return NULL;
}

static const char *rv_set_path(const bt_rv_file_set *set, const char *name) {
    for (size_t at = 0; at < set->count; at++) {
        if (strcmp(set->names[at], name) == 0) {
            return set->paths[at];
        }
    }
    return NULL;
}

static int rv_set_has(const bt_rv_file_set *set, const char *name) {
    return rv_set_path(set, name) != NULL;
}

static void rv_pairs_push(rv_set_pair **pairs, size_t *count, const char *name, const char *path) {
    rv_set_pair *grown = realloc(*pairs, (*count + 1) * sizeof *grown);
    if (grown == NULL) {
        fputs("bt-release-verify: out of memory\n", stderr);
        abort();
    }
    *pairs = grown;
    (*pairs)[*count].name = rv_xstrdup(name);
    (*pairs)[*count].path = rv_xstrdup(path);
    (*count)++;
}

static void rv_pairs_free(rv_set_pair *pairs, size_t count) {
    for (size_t at = 0; at < count; at++) {
        free(pairs[at].name);
        free(pairs[at].path);
    }
    free(pairs);
}

static int rv_collect_files(const char *dir, const char *staging, rv_set_pair **pairs,
                            size_t *count, char *err, size_t err_cap) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        snprintf(err, err_cap, "read %s: %lu", dir, GetLastError());
        return -1;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char path[BT_PATH_MAX];
        rv_join(path, dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (rv_collect_files(path, staging, pairs, count, err, err_cap) != 0) {
                FindClose(scan);
                return -1;
            }
            continue;
        }
        char name[BT_PATH_MAX];
        snprintf(name, sizeof name, "%s", path + strlen(staging) + 1);
        for (char *cursor = name; *cursor != '\0'; cursor++) {
            if (*cursor == '\\') {
                *cursor = '/';
            }
        }
        rv_pairs_push(pairs, count, name, path);
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    return 0;
}

void bt_rv_file_set_free(bt_rv_file_set *set) {
    for (size_t at = 0; at < set->count; at++) {
        free(set->names[at]);
        free(set->paths[at]);
    }
    free(set->names);
    free(set->paths);
    set->names = NULL;
    set->paths = NULL;
    set->count = 0;
}

int bt_rv_staging_file_set(const char *staging, const char *exe, const char *config_file,
                           const char *fonts_dir, bt_rv_file_set *set, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.staging_file_set");
    memset(set, 0, sizeof *set);
    char exe_path[BT_PATH_MAX];
    rv_join(exe_path, staging, exe);
    if (!rv_is_file(exe_path)) {
        snprintf(err, err_cap, "staging missing %s", exe_path);
        BT_TRACE_FAIL("rv.staging_file_set", "%s", err);
        BT_TRACE_EXIT("rv.staging_file_set");
        return -1;
    }
    char config_path[BT_PATH_MAX];
    rv_join(config_path, staging, config_file);
    if (!rv_is_file(config_path)) {
        snprintf(err, err_cap, "staging missing %s", config_path);
        BT_TRACE_FAIL("rv.staging_file_set", "%s", err);
        BT_TRACE_EXIT("rv.staging_file_set");
        return -1;
    }
    char fonts_path[BT_PATH_MAX];
    rv_join(fonts_path, staging, fonts_dir);
    if (!rv_is_dir(fonts_path)) {
        snprintf(err, err_cap, "staging missing %s", fonts_path);
        BT_TRACE_FAIL("rv.staging_file_set", "%s", err);
        BT_TRACE_EXIT("rv.staging_file_set");
        return -1;
    }
    rv_set_pair *pairs = NULL;
    size_t count = 0;
    rv_pairs_push(&pairs, &count, exe, exe_path);
    rv_pairs_push(&pairs, &count, config_file, config_path);
    if (rv_collect_files(fonts_path, staging, &pairs, &count, err, err_cap) != 0) {
        rv_pairs_free(pairs, count);
        BT_TRACE_FAIL("rv.staging_file_set", "%s", err);
        BT_TRACE_EXIT("rv.staging_file_set");
        return -1;
    }
    qsort(pairs, count, sizeof *pairs, rv_pair_cmp);
    set->names = rv_xmalloc((count + 1) * sizeof *set->names);
    set->paths = rv_xmalloc((count + 1) * sizeof *set->paths);
    for (size_t at = 0; at < count; at++) {
        set->names[at] = pairs[at].name;
        set->paths[at] = pairs[at].path;
        pairs[at].name = NULL;
        pairs[at].path = NULL;
    }
    free(pairs);
    set->count = count;
    BT_TRACE_EXIT("rv.staging_file_set");
    return 0;
}

int bt_rv_normalize_entry(const char *name, const char *root, char *out, size_t out_cap) {
    char slashed[BT_PATH_MAX];
    snprintf(slashed, sizeof slashed, "%s", name);
    for (char *cursor = slashed; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    size_t len = strlen(slashed);
    if (len > 0 && slashed[len - 1] == '/') {
        return 0;
    }
    char prefix[BT_PATH_MAX];
    snprintf(prefix, sizeof prefix, "%s/", root);
    size_t prefix_len = strlen(prefix);
    const char *front = slashed;
    if (strncmp(slashed, prefix, prefix_len) == 0) {
        front = slashed + prefix_len;
    }
    snprintf(out, out_cap, "%s", front);
    return 1;
}

static void rv_archive_free(rv_archive *archive) {
    free(archive->bytes);
    free(archive->entries);
    archive->bytes = NULL;
    archive->entries = NULL;
    archive->count = 0;
    archive->len = 0;
}

static int rv_archive_open(const char *path, rv_archive *archive, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.archive_open");
    memset(archive, 0, sizeof *archive);
    unsigned char *bytes = NULL;
    size_t len = 0;
    char inner[BT_RV_ERR_MAX];
    if (rv_read_bytes(path, &bytes, &len, inner, sizeof inner) != 0) {
        snprintf(err, err_cap, "cannot open zip: %s", inner);
        BT_TRACE_FAIL("rv.archive_open", "%s", err);
        BT_TRACE_EXIT("rv.archive_open");
        return -1;
    }
    size_t scan_from = len > RV_EOCD_MAX_BACK ? len - RV_EOCD_MAX_BACK : 0;
    size_t eocd = len;
    for (size_t at = len >= 22 ? len - 22 + 1 : 0; at-- > scan_from;) {
        if (rv_u32(bytes + at) == 0x06054b50) {
            eocd = at;
            break;
        }
    }
    if (eocd >= len) {
        free(bytes);
        snprintf(err, err_cap, "cannot open zip: end record missing");
        BT_TRACE_FAIL("rv.archive_open", "%s", err);
        BT_TRACE_EXIT("rv.archive_open");
        return -1;
    }
    size_t count = rv_u16(bytes + eocd + 10);
    uint32_t cd_size = rv_u32(bytes + eocd + 12);
    uint32_t cd_offset = rv_u32(bytes + eocd + 16);
    if ((size_t)cd_offset + cd_size > eocd) {
        free(bytes);
        snprintf(err, err_cap, "cannot open zip: central directory out of range");
        BT_TRACE_FAIL("rv.archive_open", "%s", err);
        BT_TRACE_EXIT("rv.archive_open");
        return -1;
    }
    rv_entry *entries = rv_xmalloc((count + 1) * sizeof *entries);
    size_t at = cd_offset;
    for (size_t index = 0; index < count; index++) {
        if (at + RV_CENTRAL_HEAD > eocd || rv_u32(bytes + at) != 0x02014b50) {
            free(bytes);
            free(entries);
            snprintf(err, err_cap, "cannot open zip: central directory truncated");
            BT_TRACE_FAIL("rv.archive_open", "%s", err);
            BT_TRACE_EXIT("rv.archive_open");
            return -1;
        }
        size_t name_len = rv_u16(bytes + at + 28);
        size_t extra_len = rv_u16(bytes + at + 30);
        size_t comment_len = rv_u16(bytes + at + 32);
        if (at + RV_CENTRAL_HEAD + name_len > eocd || name_len >= BT_PATH_MAX) {
            free(bytes);
            free(entries);
            snprintf(err, err_cap, "cannot open zip: entry name out of range");
            BT_TRACE_FAIL("rv.archive_open", "%s", err);
            BT_TRACE_EXIT("rv.archive_open");
            return -1;
        }
        rv_entry *entry = &entries[index];
        memcpy(entry->name, bytes + at + RV_CENTRAL_HEAD, name_len);
        entry->name[name_len] = '\0';
        entry->method = rv_u16(bytes + at + 10);
        entry->crc = rv_u32(bytes + at + 16);
        entry->csize = rv_u32(bytes + at + 20);
        entry->usize = rv_u32(bytes + at + 24);
        entry->offset = rv_u32(bytes + at + 42);
        at += RV_CENTRAL_HEAD + name_len + extra_len + comment_len;
    }
    archive->bytes = bytes;
    archive->len = len;
    archive->entries = entries;
    archive->count = count;
    BT_TRACE_EXIT("rv.archive_open");
    return 0;
}

static int rv_entry_bytes(const rv_archive *archive, const rv_entry *entry, unsigned char **out,
                          size_t *out_len, char *err, size_t err_cap) {
    size_t base = entry->offset;
    if (base + 30 > archive->len || rv_u32(archive->bytes + base) != 0x04034b50) {
        snprintf(err, err_cap, "zip entry %s unreadable: local header missing", entry->name);
        return -1;
    }
    size_t name_len = rv_u16(archive->bytes + base + 26);
    size_t extra_len = rv_u16(archive->bytes + base + 28);
    size_t data_at = base + 30 + name_len + extra_len;
    if (data_at + entry->csize > archive->len) {
        snprintf(err, err_cap, "zip entry %s unreadable: data truncated", entry->name);
        return -1;
    }
    const unsigned char *data = archive->bytes + data_at;
    if (entry->method == RV_METHOD_STORE) {
        if (entry->csize != entry->usize) {
            snprintf(err, err_cap, "zip entry %s unreadable: size mismatch", entry->name);
            return -1;
        }
        unsigned char *copy = rv_xmalloc(entry->usize);
        if (entry->usize > 0) {
            memcpy(copy, data, entry->usize);
        }
        if (bt_rv_crc32(copy, entry->usize) != entry->crc) {
            free(copy);
            snprintf(err, err_cap, "zip entry %s unreadable: crc mismatch", entry->name);
            return -1;
        }
        *out = copy;
        *out_len = entry->usize;
        return 0;
    }
    if (entry->method != RV_METHOD_DEFLATE) {
        snprintf(err, err_cap, "zip entry %s unreadable: unsupported method %u", entry->name,
                 entry->method);
        return -1;
    }
    char inner[96];
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    if (bt_rv_inflate_raw(data, entry->csize, &raw, &raw_len, inner, sizeof inner) != 0) {
        snprintf(err, err_cap, "zip entry %s unreadable: %s", entry->name, inner);
        return -1;
    }
    if (raw_len != entry->usize || bt_rv_crc32(raw, raw_len) != entry->crc) {
        free(raw);
        snprintf(err, err_cap, "zip entry %s unreadable: crc mismatch", entry->name);
        return -1;
    }
    *out = raw;
    *out_len = raw_len;
    return 0;
}

int bt_rv_check_zip(const char *zip_path, const char *root, const bt_rv_file_set *set,
                    size_t *count, char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.check_zip");
    *count = 0;
    rv_archive archive;
    if (rv_archive_open(zip_path, &archive, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.check_zip", "%s", err);
        BT_TRACE_EXIT("rv.check_zip");
        return -1;
    }
    rv_set_pair *seen = NULL;
    size_t seen_count = 0;
    int failed = 0;
    for (size_t at = 0; at < archive.count && !failed; at++) {
        char name[BT_PATH_MAX];
        if (bt_rv_normalize_entry(archive.entries[at].name, root, name, sizeof name) != 1) {
            continue;
        }
        int duplicate = 0;
        for (size_t probe = 0; probe < seen_count; probe++) {
            if (strcmp(seen[probe].name, name) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            snprintf(err, err_cap, "duplicate zip entry %s", name);
            failed = 1;
            break;
        }
        char index_text[32];
        snprintf(index_text, sizeof index_text, "%zu", at);
        rv_pairs_push(&seen, &seen_count, name, index_text);
    }
    if (!failed) {
        qsort(seen, seen_count, sizeof *seen, rv_pair_cmp);
        for (size_t at = 0; at < set->count && !failed; at++) {
            if (rv_pairs_find(seen, seen_count, set->names[at]) == NULL) {
                snprintf(err, err_cap, "zip missing %s", set->names[at]);
                failed = 1;
            }
        }
    }
    if (!failed) {
        for (size_t at = 0; at < seen_count && !failed; at++) {
            if (!rv_set_has(set, seen[at].name)) {
                snprintf(err, err_cap, "unexpected zip entry %s", seen[at].name);
                failed = 1;
            }
        }
    }
    if (!failed) {
        for (size_t at = 0; at < seen_count && !failed; at++) {
            size_t index = (size_t)strtoull(seen[at].path, NULL, 10);
            unsigned char *entry_data = NULL;
            size_t entry_len = 0;
            if (rv_entry_bytes(&archive, &archive.entries[index], &entry_data, &entry_len, err,
                               err_cap) != 0) {
                failed = 1;
                break;
            }
            const char *staged = rv_set_path(set, seen[at].name);
            if (staged == NULL) {
                free(entry_data);
                snprintf(err, err_cap, "staging missing %s", seen[at].name);
                failed = 1;
                break;
            }
            unsigned char *staged_data = NULL;
            size_t staged_len = 0;
            char inner[BT_RV_ERR_MAX];
            if (rv_read_bytes(staged, &staged_data, &staged_len, inner, sizeof inner) != 0) {
                free(entry_data);
                snprintf(err, err_cap, "read %s: %s", staged, inner);
                failed = 1;
                break;
            }
            int same = entry_len == staged_len &&
                       (entry_len == 0 || memcmp(entry_data, staged_data, entry_len) == 0);
            free(entry_data);
            free(staged_data);
            if (!same) {
                snprintf(err, err_cap, "byte mismatch in zip entry %s", seen[at].name);
                failed = 1;
            }
        }
    }
    if (!failed) {
        *count = seen_count;
    }
    rv_pairs_free(seen, seen_count);
    rv_archive_free(&archive);
    if (failed) {
        BT_TRACE_FAIL("rv.check_zip", "%s", err);
        BT_TRACE_EXIT("rv.check_zip");
        return -1;
    }
    BT_TRACE_EXIT("rv.check_zip");
    return 0;
}

int bt_rv_extract_zip(const char *zip_path, const char *dest, const char *root, size_t *count,
                      char *err, size_t err_cap) {
    BT_TRACE_ENTER("rv.extract_zip");
    *count = 0;
    rv_archive archive;
    if (rv_archive_open(zip_path, &archive, err, err_cap) != 0) {
        BT_TRACE_FAIL("rv.extract_zip", "%s", err);
        BT_TRACE_EXIT("rv.extract_zip");
        return -1;
    }
    int failed = 0;
    size_t extracted = 0;
    for (size_t at = 0; at < archive.count && !failed; at++) {
        char name[BT_PATH_MAX];
        if (bt_rv_normalize_entry(archive.entries[at].name, root, name, sizeof name) != 1) {
            continue;
        }
        size_t len = strlen(name);
        int unsafe = len == 0;
        const char *cursor = name;
        while (*cursor != '\0') {
            size_t part_len = strcspn(cursor, "/");
            if (part_len == 2 && strncmp(cursor, "..", 2) == 0) {
                unsafe = 1;
                break;
            }
            cursor += part_len;
            if (*cursor == '/') {
                cursor++;
            }
        }
        if (unsafe) {
            snprintf(err, err_cap, "unsafe zip entry %s", name);
            failed = 1;
            break;
        }
        unsigned char *data = NULL;
        size_t data_len = 0;
        if (rv_entry_bytes(&archive, &archive.entries[at], &data, &data_len, err, err_cap) != 0) {
            failed = 1;
            break;
        }
        char path[BT_PATH_MAX];
        rv_join(path, dest, name);
        char parent[BT_PATH_MAX];
        snprintf(parent, sizeof parent, "%s", path);
        char *slash = strrchr(parent, '/');
        char *backslash = strrchr(parent, '\\');
        char *cut = slash > backslash ? slash : backslash;
        if (cut != NULL) {
            *cut = '\0';
            rv_mkdir_p(parent);
        }
        if (rv_write_bytes(path, data, data_len, err, err_cap) != 0) {
            free(data);
            failed = 1;
            break;
        }
        free(data);
        extracted++;
    }
    rv_archive_free(&archive);
    if (failed) {
        BT_TRACE_FAIL("rv.extract_zip", "%s", err);
        BT_TRACE_EXIT("rv.extract_zip");
        return -1;
    }
    *count = extracted;
    BT_TRACE_EXIT("rv.extract_zip");
    return 0;
}
