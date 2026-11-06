#include "release_verify_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "release-verify/exec.h"
#include "release-verify/inflate.h"
#include "release-verify/lib.h"
#include "release-verify/png.h"
#include "unit/core/death_test.h"

enum {
    OOM_STAGE_ALLOC_SLOTS = 6,
    OOM_STAGING_ALLOC_SLOTS = 11,
};

static void oom_scratch(const char *tag, char out[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
    snprintf(base, sizeof base, "%s", "build/test-tmp/release-verify-oom");
    rv_util_mkdir_p(base);
    snprintf(out, BT_PATH_MAX, "%s/%s", base, tag);
    rv_util_rm_rf(out);
    rv_util_mkdir_p(out);
}

BT_TEST(release_verify_oom, png_check_names_its_read_buffer_starvations) {
    char root[BT_PATH_MAX];
    oom_scratch("png", root);
    char tiny[BT_PATH_MAX];
    rv_util_join(tiny, root, "tiny.png");
    rv_util_write_png(tiny, 4, 4);
    unsigned int width = 0;
    unsigned int height = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(0);
    int rc = bt_rv_png_check(tiny, &width, &height, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    bt_alloc_fail_arm(1);
    rc = bt_rv_png_check(tiny, &width, &height, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    rv_util_rm_rf(root);
}

BT_TEST(release_verify_oom, png_check_names_its_growth_starvation) {
    char root[BT_PATH_MAX];
    oom_scratch("png-big", root);
    char big[BT_PATH_MAX];
    rv_util_join(big, root, "big.png");
    rv_util_write_png(big, (4U << 20U) + 64U, 1U);
    unsigned int width = 0;
    unsigned int height = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(1);
    int rc = bt_rv_png_check(big, &width, &height, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    rv_util_rm_rf(root);
}

BT_TEST(release_verify_oom, inflate_names_its_sink_starvation) {
    static const unsigned char raw[] = "sink starvation probe";
    unsigned char deflated[128];
    size_t dlen = rv_util_deflate_fixed(raw, sizeof raw - 1, deflated, sizeof deflated);
    unsigned char stream[160];
    size_t at = 0;
    stream[at++] = 0x78;
    stream[at++] = 0x01;
    memcpy(stream + at, deflated, dlen);
    at += dlen;
    uint32_t adler = rv_util_adler32(raw, sizeof raw - 1);
    stream[at++] = (unsigned char)(adler >> 24U);
    stream[at++] = (unsigned char)(adler >> 16U);
    stream[at++] = (unsigned char)(adler >> 8U);
    stream[at++] = (unsigned char)adler;
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(0);
    int rc = bt_rv_inflate_zlib(stream, at, &out, &out_len, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
}

static size_t oom_zlib_wrap(const unsigned char *deflated, size_t dlen, const unsigned char *raw,
                            size_t raw_len, unsigned char *stream) {
    size_t at = 0;
    stream[at++] = 0x78;
    stream[at++] = 0x01;
    memcpy(stream + at, deflated, dlen);
    at += dlen;
    uint32_t adler = rv_util_adler32(raw, raw_len);
    stream[at++] = (unsigned char)(adler >> 24U);
    stream[at++] = (unsigned char)(adler >> 16U);
    stream[at++] = (unsigned char)(adler >> 8U);
    stream[at++] = (unsigned char)adler;
    return at;
}

BT_TEST(release_verify_oom, inflate_names_its_literal_growth_starvation) {
    size_t raw_len = RV_SINK_PREALLOC + 8;
    size_t cap = raw_len * 2 + 64;
    unsigned char *raw = malloc(raw_len);
    unsigned char *deflated = malloc(cap);
    unsigned char *stream = malloc(cap + 8);
    if (raw == NULL || deflated == NULL || stream == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "out of memory for the literal starvation probe");
        free(raw);
        free(deflated);
        free(stream);
        return;
    }
    memset(raw, 'A', raw_len);
    size_t dlen = rv_util_deflate_fixed(raw, raw_len, deflated, cap);
    size_t at = oom_zlib_wrap(deflated, dlen, raw, raw_len, stream);
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(1);
    int rc = bt_rv_inflate_zlib(stream, at, &out, &out_len, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    free(raw);
    free(deflated);
    free(stream);
}

BT_TEST(release_verify_oom, inflate_names_its_stored_growth_starvation) {
    size_t first_len = RV_SINK_PREALLOC - 1;
    size_t raw_len = first_len + 2;
    unsigned char *raw = malloc(raw_len);
    unsigned char *stream = malloc(raw_len + 32);
    if (raw == NULL || stream == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "out of memory for the stored starvation probe");
        free(raw);
        free(stream);
        return;
    }
    memset(raw, 'S', first_len);
    memset(raw + first_len, 'T', 2);
    stream[0] = 0x78;
    stream[1] = 0x01;
    rv_bitwriter writer = {.data = stream, .len = 2, .cap = raw_len + 32, .bits = 0, .cnt = 0};
    rv_bits_put_lsb(&writer, 0U, 3);
    stream[writer.len++] = (unsigned char)(writer.bits & 0xFFU);
    writer.bits = 0;
    writer.cnt = 0;
    uint16_t len16 = (uint16_t)first_len;
    uint16_t nlen16 = (uint16_t)~len16;
    stream[writer.len++] = (unsigned char)(len16 & 0xFFU);
    stream[writer.len++] = (unsigned char)(len16 >> 8U);
    stream[writer.len++] = (unsigned char)(nlen16 & 0xFFU);
    stream[writer.len++] = (unsigned char)(nlen16 >> 8U);
    memcpy(stream + writer.len, raw, first_len);
    writer.len += first_len;
    rv_bits_put_lsb(&writer, 1U, 3);
    stream[writer.len++] = (unsigned char)(writer.bits & 0xFFU);
    len16 = 2;
    nlen16 = (uint16_t)~len16;
    stream[writer.len++] = (unsigned char)(len16 & 0xFFU);
    stream[writer.len++] = (unsigned char)(len16 >> 8U);
    stream[writer.len++] = (unsigned char)(nlen16 & 0xFFU);
    stream[writer.len++] = (unsigned char)(nlen16 >> 8U);
    stream[writer.len++] = 'T';
    stream[writer.len++] = 'T';
    uint32_t adler = rv_util_adler32(raw, raw_len);
    stream[writer.len++] = (unsigned char)(adler >> 24U);
    stream[writer.len++] = (unsigned char)(adler >> 16U);
    stream[writer.len++] = (unsigned char)(adler >> 8U);
    stream[writer.len++] = (unsigned char)adler;
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(1);
    int rc = bt_rv_inflate_zlib(stream, writer.len, &out, &out_len, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    free(raw);
    free(stream);
}

BT_TEST(release_verify_oom, inflate_names_its_match_growth_starvation) {
    size_t literal_count = RV_SINK_PREALLOC - 6;
    size_t match_len = 258;
    size_t raw_len = literal_count + match_len;
    size_t cap = literal_count * 2 + 64;
    unsigned char *raw = malloc(raw_len);
    unsigned char *deflated = malloc(cap);
    unsigned char *stream = malloc(cap + 8);
    if (raw == NULL || deflated == NULL || stream == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "out of memory for the match starvation probe");
        free(raw);
        free(deflated);
        free(stream);
        return;
    }
    memset(raw, 'A', raw_len);
    rv_bitwriter writer = {.data = deflated, .len = 0, .cap = cap, .bits = 0, .cnt = 0};
    rv_bits_put_lsb(&writer, 3U, 3);
    for (size_t at = 0; at < literal_count; at++) {
        rv_fixed_lit(&writer, 'A');
    }
    rv_fixed_lit(&writer, 285);
    rv_bits_put(&writer, 0U, 5);
    rv_fixed_lit(&writer, 256);
    if (writer.cnt > 0) {
        deflated[writer.len++] = (unsigned char)(writer.bits & 0xFFU);
    }
    size_t at = oom_zlib_wrap(deflated, writer.len, raw, raw_len, stream);
    unsigned char *out = NULL;
    size_t out_len = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(1);
    int rc = bt_rv_inflate_zlib(stream, at, &out, &out_len, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    free(raw);
    free(deflated);
    free(stream);
}

BT_TEST(release_verify_oom, run_names_a_config_read_starvation) {
    char root[BT_PATH_MAX];
    oom_scratch("run", root);
    char staging[BT_PATH_MAX];
    rv_util_join(staging, root, "staging");
    rv_util_mkdir_p(staging);
    char zip_path[BT_PATH_MAX];
    rv_util_join(zip_path, root, "app.zip");
    rv_util_write(zip_path, "stub");
    char config[BT_PATH_MAX];
    rv_util_join(config, root, "config.json");
    rv_util_write(config, "{}");
    bt_rv_env env = {
        .argv = (const char *const *)(const char *[]){staging, zip_path, NULL},
        .config = config,
        .vars = NULL,
        .root = root,
    };
    int exit_code = 0;
    char err[BT_RV_ERR_MAX];
    bt_alloc_fail_arm(0);
    int rc = bt_rv_run(&env, &exit_code, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(err, "cannot read config") != NULL);
    rv_util_rm_rf(root);
}

static void oom_zip_child(void) {
    char root[BT_PATH_MAX];
    oom_scratch("zip-death", root);
    char zip_path[BT_PATH_MAX];
    rv_util_join(zip_path, root, "app.zip");
    static const unsigned char body[5] = "data";
    const rv_zip_entry entries[3] = {
        {"blind-trainer.exe", body, sizeof body},
        {"config/config.json", body, sizeof body},
        {"assets/fonts/x.ttf", body, sizeof body},
    };
    rv_util_write_zip(zip_path, entries, 3);
    bt_rv_file_set set = {0};
    size_t count = 0;
    char err[BT_RV_ERR_MAX];
    const char *raw = getenv("RV_OOM_K");
    size_t k = raw != NULL ? (size_t)strtoul(raw, NULL, 10) : 0;
    bt_alloc_fail_arm(k);
    bt_rv_check_zip(zip_path, root, &set, &count, err, sizeof err);
}

BT_TEST(release_verify_oom, zip_check_aborts_when_its_allocations_starve) {
    char setting[32];
    for (size_t k = 0; k < 10; k++) {
        snprintf(setting, sizeof setting, "RV_OOM_K=%zu", k);
        _putenv(setting);
        bt_death_expect_abort("release_verify_oom.zip_check", "out of memory", oom_zip_child);
    }
    _putenv("RV_OOM_K=");
}

static void oom_scrub_child(void) {
    const char *vars[3] = {"PATH=C:\\windows", "BT_RV_PROBE=1", NULL};
    char **env = NULL;
    const char *raw = getenv("RV_OOM_K");
    size_t k = raw != NULL ? (size_t)strtoul(raw, NULL, 10) : 0;
    bt_alloc_fail_arm(k);
    bt_rv_scrub_env(vars, &env);
}

BT_TEST(release_verify_oom, scrub_env_aborts_when_its_allocations_starve) {
    for (size_t k = 0; k < 6; k++) {
        char setting[32];
        snprintf(setting, sizeof setting, "RV_OOM_K=%zu", k);
        _putenv(setting);
        bt_death_expect_abort("release_verify_oom.scrub_env", "out of memory", oom_scrub_child);
    }
    _putenv("RV_OOM_K=");
}

static void oom_env_set_child(void) {
    const char *args[] = {"a", NULL};
    const char *vars[] = {"V=1", NULL};
    bt_rv_env_pair extras[] = {{.key = "NEW", .value = "x"}};
    bt_rv_child_spec spec;
    const char *raw = getenv("RV_OOM_K");
    size_t k = raw != NULL ? (size_t)strtoul(raw, NULL, 10) : 0;
    bt_alloc_fail_arm(k);
    bt_rv_stage_spec("prog", args, NULL, vars, extras, 1, &spec);
}

BT_TEST(release_verify_oom, stage_spec_aborts_when_its_env_growth_starves) {
    for (size_t k = 0; k < OOM_STAGE_ALLOC_SLOTS; k++) {
        char setting[32];
        snprintf(setting, sizeof setting, "RV_OOM_K=%zu", k);
        _putenv(setting);
        bt_death_expect_abort("release_verify_oom.env_set", "out of memory", oom_env_set_child);
    }
    _putenv("RV_OOM_K=");
}

static void oom_staging_child(void) {
    char root[BT_PATH_MAX];
    oom_scratch("staging-death", root);
    char exe_file[BT_PATH_MAX];
    rv_util_join(exe_file, root, "app.exe");
    rv_util_write(exe_file, "x");
    char config_dir[BT_PATH_MAX];
    rv_util_join(config_dir, root, "config");
    rv_util_mkdir_p(config_dir);
    char config_file[BT_PATH_MAX];
    rv_util_join(config_file, config_dir, "config.json");
    rv_util_write(config_file, "x");
    char fonts_dir[BT_PATH_MAX];
    rv_util_join(fonts_dir, root, "fonts");
    rv_util_mkdir_p(fonts_dir);
    char font_file[BT_PATH_MAX];
    rv_util_join(font_file, fonts_dir, "f.ttf");
    rv_util_write(font_file, "x");
    bt_rv_file_set set = {0};
    char err[BT_RV_ERR_MAX];
    const char *raw = getenv("RV_OOM_K");
    size_t k = raw != NULL ? (size_t)strtoul(raw, NULL, 10) : 0;
    bt_alloc_fail_arm(k);
    bt_rv_staging_file_set(root, "app.exe", "config/config.json", "fonts", &set, err, sizeof err);
}

BT_TEST(release_verify_oom, staging_file_set_aborts_when_its_pairs_pushes_starve) {
    for (size_t k = 0; k < OOM_STAGING_ALLOC_SLOTS; k++) {
        char setting[32];
        snprintf(setting, sizeof setting, "RV_OOM_K=%zu", k);
        _putenv(setting);
        bt_death_expect_abort("release_verify_oom.staging_files", "out of memory",
                              oom_staging_child);
    }
    _putenv("RV_OOM_K=");
}
