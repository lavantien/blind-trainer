#include "release-verify/lib.h"
#include "release-verify/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/release_verify_test_util.h"
#include "unit/tools/spawn_util.h"

#define ZIP_TEST_EXE "blind-trainer.exe"
#define ZIP_TEST_CONFIG "config/config.json"
#define ZIP_TEST_FONTS "assets/fonts"
#define ZIP_TEST_ROOT "blind-trainer"

static const unsigned char EXE_BYTES[] = "fake exe payload 0123456789";
static const unsigned char CONFIG_BYTES[] =
    "{\"app\": {\"window\": {\"width\": 1280, \"height\": 800}}}";
static const unsigned char FONT_MAIN[] = "font main bytes";
static const unsigned char FONT_BOLD[] = "font bold bytes";

typedef struct {
    int is_dir;
    const char *name;
    const unsigned char *data;
    size_t len;
} zip_entry;

static void zip_test_flatten(const zip_entry *entries, size_t count, rv_zip_entry *flat) {
    static const unsigned char empty[1] = {0};
    for (size_t at = 0; at < count && at < 64; at++) {
        flat[at].name = entries[at].name;
        flat[at].data = entries[at].len > 0 ? entries[at].data : empty;
        flat[at].len = entries[at].len;
    }
}

static void zip_test_write(const char *path, const zip_entry *entries, size_t count) {
    rv_zip_entry flat[64];
    zip_test_flatten(entries, count, flat);
    rv_util_write_zip(path, flat, count);
}

static void zip_test_write_stored_bad_crc(const char *path, const zip_entry *entries, size_t count,
                                          size_t bad_crc_at) {
    rv_zip_entry flat[64];
    zip_test_flatten(entries, count, flat);
    rv_util_write_zip_stored_bad_crc(path, flat, count, bad_crc_at);
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
        "assets/fonts/bold.ttf", "assets/fonts/condensed/italic.ttf",
        "assets/fonts/main.ttf", "blind-trainer.exe",
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
    BT_CHECK_EQ(
        bt_rv_normalize_entry("blind-trainer\\config\\config.json", ZIP_TEST_ROOT, out, sizeof out),
        1);
    BT_CHECK_EQ(out, "config/config.json");
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer/assets/fonts/main.ttf", ZIP_TEST_ROOT, out,
                                      sizeof out),
                1);
    BT_CHECK_EQ(out, "assets/fonts/main.ttf");
    BT_CHECK_EQ(bt_rv_normalize_entry("blind-trainer/", ZIP_TEST_ROOT, out, sizeof out), 0);
    BT_CHECK_EQ(
        bt_rv_normalize_entry("blind-trainer\\assets\\fonts\\", ZIP_TEST_ROOT, out, sizeof out), 0);
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

BT_TEST(release_verify, check_and_extract_reject_a_tampered_stored_crc) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-store-crc", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "store-crc.zip");
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
    };
    zip_test_write_stored_bad_crc(zip, entries, sizeof entries / sizeof entries[0], 2);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "crc") != NULL);
    bt_rv_file_set_free(&set);
    char dest[BT_PATH_MAX];
    rv_util_join(dest, staging_dir, "dest");
    size_t extracted = 0;
    BT_CHECK(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "crc") != NULL);
}

BT_TEST(release_verify, check_and_extract_reject_an_empty_entry_name) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-empty-name", staging_dir);
    make_staging(staging_dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, staging_dir, "empty-name.zip");
    static const unsigned char junk[] = "junk";
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
        {0, "", junk, sizeof junk - 1},
    };
    zip_test_write(zip, entries, sizeof entries / sizeof entries[0]);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "unexpected zip entry") != NULL);
    bt_rv_file_set_free(&set);
    char dest[BT_PATH_MAX];
    rv_util_join(dest, staging_dir, "dest");
    size_t extracted = 0;
    BT_CHECK(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "unsafe zip entry") != NULL);
}

enum {
    ZIP_TEST_PROBE_BYTES = 65536,
};

BT_TEST(release_verify, check_and_extract_round_trip_a_python_zipfile_deflate_archive) {
    char staging_dir[BT_PATH_MAX];
    rv_util_scratch("zip-python-deflate", staging_dir);
    make_staging(staging_dir);
    unsigned char *probe = malloc(ZIP_TEST_PROBE_BYTES);
    BT_CHECK(probe != NULL);
    if (probe == NULL) {
        return;
    }
    for (size_t at = 0; at < ZIP_TEST_PROBE_BYTES; at++) {
        probe[at] = (unsigned char)(((unsigned)at * 31U + 7U) & 0xFFU);
    }
    char probe_path[BT_PATH_MAX];
    rv_util_join(probe_path, staging_dir, "assets/fonts/probe.bin");
    rv_util_write_bytes(probe_path, probe, ZIP_TEST_PROBE_BYTES);
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    char zip[BT_PATH_MAX];
    snprintf(zip, sizeof zip, "%s/tests/unit/tools/fixtures/release_verify_deflate.zip", root);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(staging_dir, &set), 0);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err), 0);
    BT_CHECK_EQ(seen, 5U);
    bt_rv_file_set_free(&set);
    char dest[BT_PATH_MAX];
    rv_util_join(dest, staging_dir, "dest");
    size_t extracted = 0;
    BT_CHECK_EQ(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err), 0);
    BT_CHECK_EQ(extracted, 5U);
    char out_path[BT_PATH_MAX];
    rv_util_join(out_path, dest, "assets/fonts/probe.bin");
    unsigned char *out = NULL;
    size_t out_len = 0;
    BT_CHECK_EQ(rv_util_file_bytes(out_path, &out, &out_len), 0);
    BT_CHECK_EQ(out_len, (size_t)ZIP_TEST_PROBE_BYTES);
    BT_CHECK(memcmp(out, probe, ZIP_TEST_PROBE_BYTES) == 0);
    free(out);
    free(probe);
}

enum {
    ZIP_CRAFT_LOCAL_SIG = 0x04034b50,
    ZIP_CRAFT_CENTRAL_SIG = 0x02014b50,
    ZIP_CRAFT_EOCD_SIG = 0x06054b50,
    ZIP_CRAFT_CENTRAL_HEAD = 46,
    ZIP_CRAFT_EOCD_HEAD = 22,
    ZIP_CRAFT_METHOD_WEIRD = 12,
    ZIP_CRAFT_HUGE = 0x7FFF0000,
    ZIP_CRAFT_BIG_ENTRY = 70000,
    ZIP_CRAFT_JUNK_TAIL = 64,
    ZIP_CRAFT_INFLATE_RAW_WANT = 8,
};

static const unsigned char PROBE_BYTES[] = {
    0x07, 'c', 'r', 'a', 'f', 't', ' ', 'p', 'r', 'o', 'b', 'e', ' ', 'b', 'y', 't', 'e', 's',
};

static void craft_probe_staging(const char *dir) {
    make_staging(dir);
    char path[BT_PATH_MAX];
    rv_util_join(path, dir, "assets/fonts/probe.bin");
    rv_util_write_bytes(path, PROBE_BYTES, sizeof PROBE_BYTES);
}

static void craft_write_zip(const char *path, int deflate) {
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
        {0, "blind-trainer/assets/fonts/probe.bin", PROBE_BYTES, sizeof PROBE_BYTES},
    };
    if (deflate) {
        zip_test_write(path, entries, sizeof entries / sizeof entries[0]);
        return;
    }
    zip_test_write_stored_bad_crc(path, entries, sizeof entries / sizeof entries[0],
                                  sizeof entries / sizeof entries[0]);
}

static unsigned char *craft_read(const char *path, size_t *len) {
    unsigned char *bytes = NULL;
    if (rv_util_file_bytes(path, &bytes, len) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "cannot read %s", path);
        return NULL;
    }
    return bytes;
}

static unsigned craft_u16(const unsigned char *at) {
    return (unsigned)at[0] | ((unsigned)at[1] << 8U);
}

static void craft_put_u16(unsigned char *at, unsigned value) {
    at[0] = (unsigned char)value;
    at[1] = (unsigned char)(value >> 8U);
}

static void craft_put_u32(unsigned char *at, uint32_t value) {
    at[0] = (unsigned char)value;
    at[1] = (unsigned char)(value >> 8U);
    at[2] = (unsigned char)(value >> 16U);
    at[3] = (unsigned char)(value >> 24U);
}

static long craft_central_of(const unsigned char *bytes, size_t len, const char *name) {
    size_t eocd = len - ZIP_CRAFT_EOCD_HEAD;
    size_t count = craft_u16(bytes + eocd + 10);
    size_t at = ((size_t)bytes[eocd + 16]) | ((size_t)bytes[eocd + 17] << 8U) |
                ((size_t)bytes[eocd + 18] << 16U) | ((size_t)bytes[eocd + 19] << 24U);
    for (size_t index = 0; index < count; index++) {
        unsigned name_len = craft_u16(bytes + at + 28);
        if (strlen(name) == name_len &&
            memcmp(bytes + at + ZIP_CRAFT_CENTRAL_HEAD, name, name_len) == 0) {
            return (long)at;
        }
        at += ZIP_CRAFT_CENTRAL_HEAD + name_len + craft_u16(bytes + at + 30) +
              craft_u16(bytes + at + 32);
    }
    return -1;
}

static int craft_check(const char *zip, const char *dir, size_t *seen, char *err) {
    bt_rv_file_set set;
    if (staged_set(dir, &set) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "staged set failed");
        return -2;
    }
    int rc = check_zip_on(zip, ZIP_TEST_ROOT, &set, seen, err, BT_RV_ERR_MAX);
    bt_rv_file_set_free(&set);
    return rc;
}

BT_TEST(release_verify, archive_open_names_a_missing_end_record) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-no-eocd", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    craft_put_u32(bytes + len - ZIP_CRAFT_EOCD_HEAD, 0);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "end record missing") != NULL);
}

BT_TEST(release_verify, archive_open_rejects_a_central_directory_out_of_range) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-cd-range", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    craft_put_u32(bytes + len - ZIP_CRAFT_EOCD_HEAD + 16, ZIP_CRAFT_HUGE);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "central directory out of range") != NULL);
}

BT_TEST(release_verify, archive_open_rejects_a_truncated_central_directory) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-cd-trunc", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    size_t eocd = len - ZIP_CRAFT_EOCD_HEAD;
    craft_put_u32(bytes + eocd + 16, (uint32_t)(eocd - 10));
    craft_put_u32(bytes + eocd + 12, 0);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "central directory truncated") != NULL);
}

BT_TEST(release_verify, archive_open_rejects_a_broken_central_signature) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-cd-sig", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u32(bytes + central, 0);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "central directory truncated") != NULL);
}

BT_TEST(release_verify, archive_open_rejects_an_entry_name_field_beyond_the_cap) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-name-cap", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u16(bytes + central + 28, BT_PATH_MAX + 76);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "entry name out of range") != NULL);
}

BT_TEST(release_verify, entry_bytes_rejects_a_missing_local_header) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-local", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u32(bytes + central + 42, (uint32_t)(len - ZIP_CRAFT_EOCD_HEAD));
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "local header missing") != NULL);
}

BT_TEST(release_verify, entry_bytes_rejects_truncated_entry_data) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-trunc-data", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u32(bytes + central + 20, ZIP_CRAFT_HUGE);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "data truncated") != NULL);
}

BT_TEST(release_verify, entry_bytes_rejects_a_stored_size_mismatch) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-size", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u32(bytes + central + 24, (uint32_t)(sizeof PROBE_BYTES + 1));
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "size mismatch") != NULL);
}

BT_TEST(release_verify, entry_bytes_rejects_an_unsupported_method) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-method", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u16(bytes + central + 10, ZIP_CRAFT_METHOD_WEIRD);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "unsupported method 12") != NULL);
}

BT_TEST(release_verify, entry_bytes_reports_an_inflate_failure) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-inflate", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    craft_put_u16(bytes + central + 10, 8);
    craft_put_u32(bytes + central + 24, ZIP_CRAFT_INFLATE_RAW_WANT);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "unreadable: inflate") != NULL ||
             strstr(err, "invalid block type") != NULL);
}

BT_TEST(release_verify, entry_bytes_rejects_a_deflate_crc_mismatch) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-crc", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 1);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        return;
    }
    long central = craft_central_of(bytes, len, "blind-trainer/assets/fonts/probe.bin");
    BT_CHECK(central >= 0);
    if (central < 0) {
        free(bytes);
        return;
    }
    uint32_t crc = 0;
    memcpy(&crc, bytes + central + 16, 4);
    craft_put_u32(bytes + central + 16, crc ^ 1U);
    rv_util_write_bytes(zip, bytes, len);
    free(bytes);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(craft_check(zip, dir, &seen, err) != 0);
    BT_CHECK(strstr(err, "crc mismatch") != NULL);
}

BT_TEST(release_verify, archive_scan_walks_a_big_archive_with_a_junk_tail) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-big", dir);
    make_staging(dir);
    unsigned char *big = malloc(ZIP_CRAFT_BIG_ENTRY);
    BT_CHECK(big != NULL);
    if (big == NULL) {
        return;
    }
    for (size_t at = 0; at < ZIP_CRAFT_BIG_ENTRY; at++) {
        big[at] = (unsigned char)((at * 7U + 11U) & 0xFFU);
    }
    char big_path[BT_PATH_MAX];
    rv_util_join(big_path, dir, "assets/fonts/probe.bin");
    rv_util_write_bytes(big_path, big, ZIP_CRAFT_BIG_ENTRY);
    const zip_entry entries[] = {
        {0, "blind-trainer\\blind-trainer.exe", EXE_BYTES, sizeof EXE_BYTES - 1},
        {0, "blind-trainer\\config\\config.json", CONFIG_BYTES, sizeof CONFIG_BYTES - 1},
        {0, "blind-trainer/assets/fonts/main.ttf", FONT_MAIN, sizeof FONT_MAIN - 1},
        {0, "blind-trainer/assets/fonts/bold.ttf", FONT_BOLD, sizeof FONT_BOLD - 1},
        {0, "blind-trainer/assets/fonts/probe.bin", big, ZIP_CRAFT_BIG_ENTRY},
    };
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    zip_test_write_stored_bad_crc(zip, entries, sizeof entries / sizeof entries[0],
                                  sizeof entries / sizeof entries[0]);
    size_t len = 0;
    unsigned char *bytes = craft_read(zip, &len);
    BT_CHECK(bytes != NULL);
    if (bytes == NULL) {
        free(big);
        return;
    }
    unsigned char *grown = realloc(bytes, len + ZIP_CRAFT_JUNK_TAIL);
    BT_CHECK(grown != NULL);
    if (grown == NULL) {
        free(bytes);
        free(big);
        return;
    }
    memset(grown + len, 0xAA, ZIP_CRAFT_JUNK_TAIL);
    rv_util_write_bytes(zip, grown, len + ZIP_CRAFT_JUNK_TAIL);
    free(grown);
    free(big);
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK_EQ(craft_check(zip, dir, &seen, err), 0);
    BT_CHECK_EQ(seen, 5U);
}

BT_TEST(release_verify, check_zip_names_a_staged_file_that_vanished) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-vanish", dir);
    craft_probe_staging(dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    craft_write_zip(zip, 0);
    bt_rv_file_set set;
    BT_CHECK_EQ(staged_set(dir, &set), 0);
    char probe_path[BT_PATH_MAX];
    rv_util_join(probe_path, dir, "assets/fonts/probe.bin");
    BT_CHECK(DeleteFileA(probe_path));
    size_t seen = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(check_zip_on(zip, ZIP_TEST_ROOT, &set, &seen, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "read ") != NULL && strstr(err, "probe.bin") != NULL);
    bt_rv_file_set_free(&set);
}

BT_TEST(release_verify, extract_zip_names_a_missing_archive) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-missing", dir);
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "absent.zip");
    size_t extracted = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_extract_zip(zip, dir, ZIP_TEST_ROOT, &extracted, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot open zip") != NULL);
}

BT_TEST(release_verify, extract_zip_names_a_write_failure_on_a_blocked_path) {
    char dir[BT_PATH_MAX];
    rv_util_scratch("craft-blocked", dir);
    rv_zip_entry entry = {ZIP_TEST_ROOT "/blocker", PROBE_BYTES, sizeof PROBE_BYTES};
    char zip[BT_PATH_MAX];
    rv_util_join(zip, dir, "craft.zip");
    rv_util_write_zip(zip, &entry, 1);
    char dest[BT_PATH_MAX];
    rv_util_join(dest, dir, "dest");
    rv_util_mkdir_p(dest);
    char blocker[BT_PATH_MAX];
    rv_util_join(blocker, dest, "blocker");
    rv_util_mkdir_p(blocker);
    size_t extracted = 0;
    char err[BT_RV_ERR_MAX];
    BT_CHECK(bt_rv_extract_zip(zip, dest, ZIP_TEST_ROOT, &extracted, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "write ") != NULL);
}
