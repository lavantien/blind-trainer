#include "release-verify/lib.h"
#include "release-verify/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/release_verify_test_util.h"

#define ZIP_TEST_EXE "blind-trainer.exe"
#define ZIP_TEST_CONFIG "config/config.json"
#define ZIP_TEST_FONTS "assets/fonts"
#define ZIP_TEST_ROOT "blind-trainer"

static const unsigned char EXE_BYTES[] = "fake exe payload 0123456789";
static const unsigned char CONFIG_BYTES[] = "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}";
static const unsigned char FONT_MAIN[] = "font main bytes";
static const unsigned char FONT_BOLD[] = "font bold bytes";

typedef struct {
    int is_dir;
    const char *name;
    const unsigned char *data;
    size_t len;
} zip_entry;

static void zip_test_write(const char *path, const zip_entry *entries, size_t count) {
    rv_zip_entry flat[64];
    static const unsigned char empty[1] = {0};
    for (size_t at = 0; at < count && at < 64; at++) {
        flat[at].name = entries[at].name;
        flat[at].data = entries[at].len > 0 ? entries[at].data : empty;
        flat[at].len = entries[at].len;
    }
    rv_util_write_zip(path, flat, count);
}

static void make_staging(const char *dir) {
    char fonts[BT_PATH_MAX];
    char config_dir[BT_PATH_MAX];
    rv_util_join(fonts, dir, ZIP_TEST_FONTS);
    rv_util_join(config_dir, dir, "config");
    rv_util_mkdir_p(fonts);
    rv_util_mkdir_p(config_dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, ZIP_TEST_EXE);
    rv_util_write_bytes(path, EXE_BYTES, sizeof EXE_BYTES - 1);
    rv_util_join(path, dir, ZIP_TEST_CONFIG);
    rv_util_write_bytes(path, CONFIG_BYTES, sizeof CONFIG_BYTES - 1);
    rv_util_join(path, dir, "assets/fonts/main.ttf");
    rv_util_write_bytes(path, FONT_MAIN, sizeof FONT_MAIN - 1);
    rv_util_join(path, dir, "assets/fonts/bold.ttf");
    rv_util_write_bytes(path, FONT_BOLD, sizeof FONT_BOLD - 1);
}

static int staged_set(const char *dir, bt_rv_file_set *set) {
    char err[BT_RV_ERR_MAX];
    return bt_rv_staging_file_set(dir, ZIP_TEST_EXE, ZIP_TEST_CONFIG, ZIP_TEST_FONTS, set, err,
                                  sizeof err);
}

static const zip_entry *faithful_entries(size_t *count) {
    static const zip_entry entries[] = {
        {1, "blind-trainer/", NULL, 0},
        {1, "blind-trainer/assets/", NULL, 0},
        {1, "blind-trainer/assets/fonts/", NULL, 0},
        {1, "blind-trainer/config/", NULL, 0},
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
    };
    *count = sizeof entries / sizeof entries[0];
    return entries;
}

BT_TEST(release_verify, staging_file_set_lists_exe_config_and_whole_fonts_tree) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("zip-set", dir);
    make_staging(dir);
    char nested[BT_PATH_MAX];
    rv_util_join(nested, dir, "assets/fonts/condensed");
    rv_util_mkdir_p(nested);
    char italic[BT_PATH_MAX];
    rv_util_join(italic, nested, "italic.ttf");
    rv_util_write(italic, "italic");
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(dir, &set), 0);
    const char *wanted[] = {
        "assets/fonts/bold.ttf",
        "assets/fonts/condensed/italic.ttf",
        "assets/fonts/main.ttf",
        "blind-trainer.exe",
        "config/config.json",
    };
    BT_CHECK_EQ(set.count, 5U);
    for (size_t at = 0; at < set.count; at++) {
        BT_CHECK_EQ(set.names[at], wanted[at]);
    }
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, ZIP_TEST_EXE);
    BT_CHECK_EQ(set.paths[3], path);
    rv_util_join(path, dir, "config/config.json");
    BT_CHECK_EQ(set.paths[4], path);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, staging_file_set_rejects_missing_layout_pieces) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("zip-set-missing", dir);
    make_staging(dir);
    bt_rv_file_set set;
    char err[BT_RV_ERR_MAX];
    char exe_path[BT_PATH_MAX];
    rv_util_join(exe_path, dir, ZIP_TEST_EXE);
    BT_CHECK(DeleteFileA(exe_path));
    BT_CHECK(bt_rv_staging_file_set(dir, ZIP_TEST_EXE, ZIP_TEST_CONFIG, ZIP_TEST_FONTS, &set, err,
                                    sizeof err) != 0);
    BT_CHECK(strstr(err, "missing") != NULL && strstr(err, ZIP_TEST_EXE) != NULL);
    rv_util_write_bytes(exe_path, EXE_BYTES, sizeof EXE_BYTES - 1);
    char config_path[BT_PATH_MAX];
    rv_util_join(config_path, dir, ZIP_TEST_CONFIG);
    BT_CHECK(DeleteFileA(config_path));
    BT_CHECK(bt_rv_staging_file_set(dir, ZIP_TEST_EXE, ZIP_TEST_CONFIG, ZIP_TEST_FONTS, &set, err,
                                    sizeof err) != 0);
    BT_CHECK(strstr(err, ZIP_TEST_CONFIG) != NULL);
    rv_util_write_bytes(config_path, CONFIG_BYTES, sizeof CONFIG_BYTES - 1);
    char fonts_path[BT_PATH_MAX];
    rv_util_join(fonts_path, dir, ZIP_TEST_FONTS);
    rv_util_rm_rf(fonts_path);
    BT_CHECK(bt_rv_staging_file_set(dir, ZIP_TEST_EXE, ZIP_TEST_CONFIG, ZIP_TEST_FONTS, &set, err,
                                    sizeof err) != 0);
    BT_CHECK(strstr(err, ZIP_TEST_FONTS) != NULL);
}

BT_TEST(release_verify, normalize_entry_maps_backslashes_root_prefix_and_directory_markers) {
    char out[BT_PATH_MAX];
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer\\config\\config.json", ZIP_TEST_ROOT, out,
                                      sizeof out),
                1);
    BT_CHECK_EQ(out, "config/config.json");
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer/assets/fonts/main.ttf", ZIP_TEST_ROOT, out,
                                      sizeof out),
                1);
    BT_CHECK_EQ(out, "assets/fonts/main.ttf");
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer/", ZIP_TEST_ROOT, out, sizeof out), 0);
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer\\assets\\fonts\\", ZIP_TEST_ROOT, out,
                                      sizeof out),
                0);
    BT_CHECK_EQ(bt_rv_normalize_entry("other/readme.txt", ZIP_TEST_ROOT, out, sizeof out), 1);
    BT_CHECK_EQ(out, "other/readme.txt");
    BT_CHECK_EQ(bt_rv_normalize_entry(ZIP_TEST_ROOT, ZIP_TEST_ROOT, out, sizeof out), 1);
    BT_CHECK_EQ(out, ZIP_TEST_ROOT);
}

static int check_zip_on(const char *zip, const char *root, const bt_rv_file_set *set, size_t *count,
                        char *err, size_t err_cap) {
    return bt_rv_check_zip(zip, root, set, count, err, err_cap);
}

BT_TEST(release_verify, check_zip_accepts_a_faithful_archive_with_archive_quirks) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-ok", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "ok.zip");
    size_t count = 0;
    const zip_entry *entries = faithful_entries(&count);
    zip_test_write(zip, entries, count);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err), 0);
    BT_CHECK_EQ(seen, 4U);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, check_zip_flags_duplicate_normalized_entries) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-dup", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "dup.zip");
    const zip_entry entries[] = {
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer\\assets\\fonts\\main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
    };
    zip_test_write(zip, entries, sizeof entries / sizeof entries[0]);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "duplicate") != NULL && strstr(err, "assets/fonts/main.ttf") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, check_zip_flags_entries_missing_from_staging) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-extra", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "extra.zip");
    size_t faithful_count = 0;
    const zip_entry *faithful = faithful_entries(&faithful_count);
    zip_entry entries[16];
    memcpy(entries, faithful, faithful_count * sizeof entries[0]);
    static const unsigned char extra[] = "extra";
    entries[faithful_count].is_dir = 0;
    entries[faithful_count].name = "blind-trainer/readme.txt";
    entries[faithful_count].data = extra;
    entries[faithful_count].len = sizeof extra - 1;
    zip_test_write(zip, entries, faithful_count + 1);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "readme.txt") != NULL && strstr(err, "unexpected") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, check_zip_flags_staged_files_missing_from_zip) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-missing", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "missing.zip");
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
    };
    zip_test_write(zip, entries, sizeof entries / sizeof entries[0]);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "missing") != NULL && strstr(err, "assets/fonts/bold.ttf") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, check_zip_flags_tampered_entry_bytes) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-tamper", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "tamper.zip");
    static const unsigned char tampered[] = "tampered font";
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", tampered, sizeof tampered - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
    };
    zip_test_write(zip, entries, sizeof entries / sizeof entries[0]);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "assets/fonts/main.ttf") != NULL && strstr(err, "byte") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, check_zip_rejects_unreadable_archive) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-absent", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "absent.zip");
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot open zip") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, extract_zip_writes_staging_layout_without_the_root_folder) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("extract-ok", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "extract-ok.zip");
    size_t count = 0;
    const zip_entry *entries = faithful_entries(&count);
    zip_test_write(zip, entries, count);
    char dest[BT_PATH_MAX];
    rv_util_scratch("extract-dest", dest);
    size_t extracted = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err), 0);
    BT_CHECK_EQ(extracted, 4U);
    unsigned char *bytes = NULL;
    size_t len = 0;
    char path[BT_PATH_MAX];
    rv_util_join(path, dest, ZIP_TEST_EXE);
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    BT_CHECK_EQ(len, sizeof EXE_BYTES - 1);
    BT_CHECK(memcmp(bytes, EXE_BYTES, len) == 0);
    free(bytes);
    rv_util_join(path, dest, ZIP_TEST_CONFIG);
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    BT_CHECK_EQ(len, sizeof CONFIG_BYTES - 1);
    BT_CHECK(memcmp(bytes, CONFIG_BYTES, len) == 0);
    free(bytes);
    rv_util_join(path, dest, "assets/fonts/main.ttf");
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    BT_CHECK_EQ(len, sizeof FONT_MAIN - 1);
    BT_CHECK(memcmp(bytes, FONT_MAIN, len) == 0);
    free(bytes);
    rv_util_join(path, dest, "assets/fonts/bold.ttf");
    BT_CHECK_EQ(rv_util_file_bytes(path, &bytes, &len), 0);
    BT_CHECK_EQ(len, sizeof FONT_BOLD - 1);
    BT_CHECK(memcmp(bytes, FONT_BOLD, len) == 0);
    free(bytes);
    rv_util_join(path, dest, ZIP_TEST_ROOT);
    BT_CHECK(!rv_util_exists(path));
}

BT_TEST(release_verify, extract_zip_rejects_parent_escape_entries) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("extract-escape", dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "escape.zip");
    static const unsigned char evil[] = "evil";
    const zip_entry entries[] = {
        {0, "blind-trainer/../evil.txt", evil, sizeof evil - 1},
    };
    zip_test_write(zip, entries, sizeof entries / sizeof entries[0]);
    char dest[BT_PATH_MAX];
    rv_util_join(dest, dir, "dest");
    size_t extracted = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "evil.txt") != NULL && strstr(err, "unsafe") != NULL);
    char evil_path[BT_PATH_MAX];
    rv_util_join(evil_path, dir, "evil.txt");
    BT_CHECK(!rv_util_exists(evil_path));
}
