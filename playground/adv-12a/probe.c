#include "release-verify/exec.h"
#include "release-verify/inflate.h"
#include "release-verify/lib.h"
#include "release-verify/png.h"
#include "release-verify/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void mkdir_p(const char *path) {
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

static void rm_rf(const char *path) {
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
            rm_rf(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    RemoveDirectoryA(path);
}

static void join(char out[BT_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
}

static unsigned char *slurp(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc(size > 0 ? (size_t)size : 1);
    if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        free(data);
        return NULL;
    }
    fclose(file);
    *out_len = (size_t)size;
    return data;
}

static void spew(const char *path, const void *data, size_t len) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fwrite(data, 1, len, file);
    fclose(file);
}

static void put16(FILE *file, unsigned short value) {
    unsigned char raw[2] = {(unsigned char)value, (unsigned char)(value >> 8U)};
    fwrite(raw, 1, 2, file);
}

static void put32(FILE *file, unsigned int value) {
    unsigned char raw[4] = {(unsigned char)value, (unsigned char)(value >> 8U),
                            (unsigned char)(value >> 16U), (unsigned char)(value >> 24U)};
    fwrite(raw, 1, 4, file);
}

typedef struct {
    const char *name;
    const unsigned char *data;
    size_t len;
    unsigned short method;
    unsigned int crc;
    unsigned int csize;
    unsigned int usize;
} raw_entry;

static void write_raw_zip(const char *path, const raw_entry *entries, size_t count) {
    FILE *file = fopen(path, "wb");
    unsigned int offsets[64];
    for (size_t at = 0; at < count; at++) {
        offsets[at] = (unsigned int)ftell(file);
        put32(file, 0x04034b50);
        put16(file, 20);
        put16(file, 0);
        put16(file, entries[at].method);
        put16(file, 0);
        put16(file, 0);
        put32(file, entries[at].crc);
        put32(file, entries[at].csize);
        put32(file, entries[at].usize);
        put16(file, (unsigned short)strlen(entries[at].name));
        put16(file, 0);
        fwrite(entries[at].name, 1, strlen(entries[at].name), file);
        fwrite(entries[at].data, 1, entries[at].len, file);
    }
    unsigned int cd_start = (unsigned int)ftell(file);
    for (size_t at = 0; at < count; at++) {
        put32(file, 0x02014b50);
        put16(file, 20);
        put16(file, 20);
        put16(file, 0);
        put16(file, entries[at].method);
        put16(file, 0);
        put16(file, 0);
        put32(file, entries[at].crc);
        put32(file, entries[at].csize);
        put32(file, entries[at].usize);
        put16(file, (unsigned short)strlen(entries[at].name));
        put16(file, 0);
        put16(file, 0);
        put16(file, 0);
        put16(file, 0);
        put32(file, 0);
        put32(file, offsets[at]);
        fwrite(entries[at].name, 1, strlen(entries[at].name), file);
    }
    unsigned int cd_size = (unsigned int)ftell(file) - cd_start;
    put32(file, 0x06054b50);
    put16(file, 0);
    put16(file, 0);
    put16(file, (unsigned short)count);
    put16(file, (unsigned short)count);
    put32(file, cd_size);
    put32(file, cd_start);
    put16(file, 0);
    fclose(file);
}

static void make_staging(const char *dir, const char *exe_bytes, size_t exe_len) {
    char path[BT_PATH_MAX];
    join(path, dir, "config");
    mkdir_p(path);
    join(path, dir, "assets/fonts");
    mkdir_p(path);
    join(path, dir, "blind-trainer.exe");
    spew(path, exe_bytes, exe_len);
    join(path, dir, "config/config.json");
    spew(path, "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}",
         strlen("{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}"));
    join(path, dir, "assets/fonts/main.ttf");
    spew(path, "font main bytes", 15);
}

static int verdict(const char *label, int rc, const char *err) {
    printf("PROBE %s rc=%d err=%s\n", label, rc, rc == 0 ? "-" : err);
    return rc;
}

static void probe_roots(void) {
    const char *names[] = {"blind-trainer", "out/dist/blind-trainer", "out/dist/blind-trainer/",
                           "/",           "a/b/",                     "staging\\"};
    for (size_t at = 0; at < sizeof names / sizeof names[0]; at++) {
        char root[BT_PATH_MAX];
        char err[BT_RV_ERR_MAX];
        int rc = bt_rv_zip_root_from(names[at], root, err, sizeof err);
        printf("PROBE root %s rc=%d root=%s err=%s\n", names[at], rc, rc == 0 ? root : "-",
               rc == 0 ? "-" : err);
    }
}

static void probe_manifest(const char *manifest) {
    size_t manifest_len = 0;
    unsigned char *text = slurp(manifest, &manifest_len);
    if (text == NULL) {
        printf("PROBE manifest missing\n");
        return;
    }
    unsigned char *grown = realloc(text, manifest_len + 1);
    if (grown == NULL) {
        free(text);
        printf("PROBE manifest oom\n");
        return;
    }
    text = grown;
    text[manifest_len] = '\0';
    char *cursor = (char *)text;
    int failures = 0;
    while (*cursor != '\0') {
        char *line = cursor;
        char *end = strchr(line, '\n');
        cursor = end != NULL ? end + 1 : line + strlen(line);
        if (end != NULL) {
            *end = '\0';
        }
        size_t line_len = strlen(line);
        while (line_len > 0 && (line[line_len - 1] == '\r' || line[line_len - 1] == '\n')) {
            line[--line_len] = '\0';
        }
        if (line_len == 0) {
            continue;
        }
        char kind[8] = {0};
        char input_path[BT_PATH_MAX] = {0};
        char expected_path[BT_PATH_MAX] = {0};
        if (sscanf(line, "%7[^\t]\t%1023[^\t]\t%1023[^\t]", kind, input_path, expected_path) != 3) {
            printf("PROBE manifest bad line %s\n", line);
            failures++;
            continue;
        }
        size_t input_len = 0;
        size_t expected_len = 0;
        unsigned char *input = slurp(input_path, &input_len);
        unsigned char *expected = slurp(expected_path, &expected_len);
        if (input == NULL || expected == NULL) {
            printf("PROBE case %s input missing\n", input_path);
            free(input);
            free(expected);
            failures++;
            continue;
        }
        unsigned char *out = NULL;
        size_t out_len = 0;
        char err[BT_RV_ERR_MAX];
        int rc;
        if (strcmp(kind, "zlib") == 0) {
            rc = bt_rv_inflate_zlib(input, input_len, &out, &out_len, err, sizeof err);
        } else {
            rc = bt_rv_inflate_raw(input, input_len, &out, &out_len, err, sizeof err);
        }
        int same = rc == 0 && out_len == expected_len &&
                   (out_len == 0 || memcmp(out, expected, out_len) == 0);
        if (!same) {
            failures++;
            printf("PROBE case %s %s FAIL rc=%d out_len=%zu expected_len=%zu err=%s\n", kind,
                   input_path, rc, out_len, expected_len, rc == 0 ? "-" : err);
        }
        free(out);
        free(input);
        free(expected);
    }
    free(text);
    printf("PROBE manifest failures=%d\n", failures);
}

static void probe_store_bad_crc(const char *scratch) {
    rm_rf(scratch);
    char staging[BT_PATH_MAX];
    join(staging, scratch, "blind-trainer");
    make_staging(staging, "fake exe payload 0123456789", 27);
    char zip[BT_PATH_MAX];
    join(zip, scratch, "bad-crc.zip");
    static const unsigned char config_bytes[] =
        "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}";
    static const unsigned char font_main[] = "font main bytes";
    raw_entry entries[3] = {
        {"blind-trainer\\blind-trainer.exe", (const unsigned char *)"fake exe payload 0123456789",
         27, 0, 0, 27, 27},
        {"blind-trainer\\config\\config.json", config_bytes, sizeof config_bytes - 1, 0, 0,
         sizeof config_bytes - 1, sizeof config_bytes - 1},
        {"blind-trainer/assets/fonts/main.ttf", font_main, sizeof font_main - 1, 0, 0,
         sizeof font_main - 1, sizeof font_main - 1},
    };
    for (size_t at = 0; at < 3; at++) {
        entries[at].crc = bt_rv_crc32(entries[at].data, entries[at].len) ^ 1U;
    }
    write_raw_zip(zip, entries, 3);
    bt_rv_file_set set;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_staging_file_set(staging, "blind-trainer.exe", "config/config.json", "assets/fonts",
                               &set, err, sizeof err) != 0) {
        printf("PROBE store_bad_crc staging rc=-1 err=%s\n", err);
        return;
    }
    size_t count = 0;
    int rc = bt_rv_check_zip(zip, "blind-trainer", &set, &count, err, sizeof err);
    printf("PROBE store_bad_crc check_zip rc=%d count=%zu err=%s\n", rc, count,
           rc == 0 ? "-" : err);
    bt_rv_file_set_free(&set);
    char dest[BT_PATH_MAX];
    join(dest, scratch, "dest");
    size_t extracted = 0;
    rc = bt_rv_extract_zip(zip, dest, "blind-trainer", &extracted, err, sizeof err);
    printf("PROBE store_bad_crc extract rc=%d extracted=%zu err=%s\n", rc, extracted,
           rc == 0 ? "-" : err);
}

static void probe_empty_name(const char *scratch) {
    rm_rf(scratch);
    char staging[BT_PATH_MAX];
    join(staging, scratch, "blind-trainer");
    make_staging(staging, "fake exe payload 0123456789", 27);
    char zip[BT_PATH_MAX];
    join(zip, scratch, "empty-name.zip");
    static const unsigned char config_bytes[] =
        "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}";
    static const unsigned char font_main[] = "font main bytes";
    static const unsigned char junk[] = "junk";
    raw_entry entries[4] = {
        {"", junk, sizeof junk - 1, 0, bt_rv_crc32(junk, sizeof junk - 1), 4, 4},
        {"blind-trainer\\blind-trainer.exe", (const unsigned char *)"fake exe payload 0123456789",
         27, 0, bt_rv_crc32((const unsigned char *)"fake exe payload 0123456789", 27), 27, 27},
        {"blind-trainer\\config\\config.json", config_bytes, sizeof config_bytes - 1, 0,
         bt_rv_crc32(config_bytes, sizeof config_bytes - 1), sizeof config_bytes - 1,
         sizeof config_bytes - 1},
        {"blind-trainer/assets/fonts/main.ttf", font_main, sizeof font_main - 1, 0,
         bt_rv_crc32(font_main, sizeof font_main - 1), sizeof font_main - 1,
         sizeof font_main - 1},
    };
    write_raw_zip(zip, entries, 4);
    bt_rv_file_set set;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_staging_file_set(staging, "blind-trainer.exe", "config/config.json", "assets/fonts",
                               &set, err, sizeof err) != 0) {
        printf("PROBE empty_name staging rc=-1 err=%s\n", err);
        return;
    }
    size_t count = 0;
    int rc = bt_rv_check_zip(zip, "blind-trainer", &set, &count, err, sizeof err);
    printf("PROBE empty_name check_zip rc=%d count=%zu err=%s\n", rc, count, rc == 0 ? "-" : err);
    bt_rv_file_set_free(&set);
    char dest[BT_PATH_MAX];
    join(dest, scratch, "dest");
    size_t extracted = 0;
    rc = bt_rv_extract_zip(zip, dest, "blind-trainer", &extracted, err, sizeof err);
    printf("PROBE empty_name extract rc=%d extracted=%zu err=%s\n", rc, extracted,
           rc == 0 ? "-" : err);
}

static void probe_real_zip(const char *zip_path, const char *staging) {
    bt_rv_file_set set;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_staging_file_set(staging, "blind-trainer.exe", "config/config.json", "assets/fonts",
                               &set, err, sizeof err) != 0) {
        printf("PROBE real_zip staging rc=-1 err=%s\n", err);
        return;
    }
    size_t count = 0;
    int rc = bt_rv_check_zip(zip_path, "blind-trainer", &set, &count, err, sizeof err);
    printf("PROBE real_zip check_zip rc=%d count=%zu err=%s\n", rc, count, rc == 0 ? "-" : err);
    bt_rv_file_set_free(&set);
    if (rc != 0) {
        return;
    }
    char dest[BT_PATH_MAX];
    snprintf(dest, sizeof dest, "%s-extract", zip_path);
    rm_rf(dest);
    size_t extracted = 0;
    rc = bt_rv_extract_zip(zip_path, dest, "blind-trainer", &extracted, err, sizeof err);
    printf("PROBE real_zip extract rc=%d extracted=%zu err=%s\n", rc, extracted,
           rc == 0 ? "-" : err);
    if (rc != 0) {
        return;
    }
    const char *leaves[] = {"blind-trainer.exe", "config/config.json", "assets/fonts/main.ttf",
                            "assets/fonts/bold.ttf"};
    for (size_t at = 0; at < sizeof leaves / sizeof leaves[0]; at++) {
        char staged_path[BT_PATH_MAX];
        char extracted_path[BT_PATH_MAX];
        join(staged_path, staging, leaves[at]);
        join(extracted_path, dest, leaves[at]);
        size_t staged_len = 0;
        size_t extracted_len = 0;
        unsigned char *staged = slurp(staged_path, &staged_len);
        unsigned char *got = slurp(extracted_path, &extracted_len);
        int same = staged != NULL && got != NULL && staged_len == extracted_len &&
                   memcmp(staged, got, staged_len) == 0;
        printf("PROBE real_zip roundtrip %s same=%d\n", leaves[at], same);
        free(staged);
        free(got);
    }
    rm_rf(dest);
}

static void probe_bad_deflate(const char *scratch) {
    rm_rf(scratch);
    char staging[BT_PATH_MAX];
    join(staging, scratch, "blind-trainer");
    make_staging(staging, "fake exe payload 0123456789", 27);
    char zip[BT_PATH_MAX];
    join(zip, scratch, "bad-deflate.zip");
    static const unsigned char config_bytes[] =
        "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}";
    static const unsigned char font_main[] = "font main bytes";
    unsigned char corrupt[28];
    memcpy(corrupt, "fake exe payload 0123456789", 27);
    corrupt[26] ^= 0xFFU;
    raw_entry entries[3] = {
        {"blind-trainer\\blind-trainer.exe", corrupt, 27, 8,
         bt_rv_crc32((const unsigned char *)"fake exe payload 0123456789", 27), 27, 27},
        {"blind-trainer\\config\\config.json", config_bytes, sizeof config_bytes - 1, 0,
         bt_rv_crc32(config_bytes, sizeof config_bytes - 1), sizeof config_bytes - 1,
         sizeof config_bytes - 1},
        {"blind-trainer/assets/fonts/main.ttf", font_main, sizeof font_main - 1, 0,
         bt_rv_crc32(font_main, sizeof font_main - 1), sizeof font_main - 1,
         sizeof font_main - 1},
    };
    write_raw_zip(zip, entries, 3);
    bt_rv_file_set set;
    char err[BT_RV_ERR_MAX];
    if (bt_rv_staging_file_set(staging, "blind-trainer.exe", "config/config.json", "assets/fonts",
                               &set, err, sizeof err) != 0) {
        printf("PROBE bad_deflate staging rc=-1 err=%s\n", err);
        return;
    }
    size_t count = 0;
    int rc = bt_rv_check_zip(zip, "blind-trainer", &set, &count, err, sizeof err);
    verdict("bad_deflate check_zip", rc, err);
    (void)count;
    bt_rv_file_set_free(&set);
}

static void probe_shot_priority(const char *dir) {
    bt_rv_release_cfg cfg;
    memset(&cfg, 0, sizeof cfg);
    snprintf(cfg.shots[0], sizeof cfg.shots[0], "menu");
    snprintf(cfg.shots[1], sizeof cfg.shots[1], "memo");
    cfg.shot_count = 2;
    size_t count = 0;
    char err[BT_RV_ERR_MAX];
    int rc = bt_rv_check_shot_dir(dir, &cfg, 10, 10, &count, err, sizeof err);
    printf("PROBE shot_priority rc=%d err=%s\n", rc, rc == 0 ? "-" : err);
}

static void probe_crc_table(void) {
    unsigned char one[] = "123456789";
    printf("PROBE crc32_123456789=%08x\n", bt_rv_crc32(one, 9));
    printf("PROBE crc32_empty=%08x\n", bt_rv_crc32(one, 0));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: probe <command> [args]\n");
        return 2;
    }
    if (strcmp(argv[1], "roots") == 0) {
        probe_roots();
    } else if (strcmp(argv[1], "manifest") == 0 && argc == 3) {
        probe_manifest(argv[2]);
    } else if (strcmp(argv[1], "store_bad_crc") == 0 && argc == 3) {
        probe_store_bad_crc(argv[2]);
    } else if (strcmp(argv[1], "empty_name") == 0 && argc == 3) {
        probe_empty_name(argv[2]);
    } else if (strcmp(argv[1], "real_zip") == 0 && argc == 4) {
        probe_real_zip(argv[2], argv[3]);
    } else if (strcmp(argv[1], "bad_deflate") == 0 && argc == 3) {
        probe_bad_deflate(argv[2]);
    } else if (strcmp(argv[1], "shot_priority") == 0 && argc == 3) {
        probe_shot_priority(argv[2]);
    } else if (strcmp(argv[1], "crc_table") == 0) {
        probe_crc_table();
    } else {
        fprintf(stderr, "probe: unknown command\n");
        return 2;
    }
    return 0;
}
