#include "release_verify_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "release-verify/inflate.h"
#include "release-verify/lib.h"
#include "release-verify/png.h"
#include "unit/core/death_test.h"

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
    for (size_t k = 0; k < 4; k++) {
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
    _putenv("RV_OOM_K=0");
    bt_death_expect_abort("release_verify_oom.scrub_array", "out of memory", oom_scrub_child);
    _putenv("RV_OOM_K=1");
    bt_death_expect_abort("release_verify_oom.scrub_entry", "out of memory", oom_scrub_child);
    _putenv("RV_OOM_K=");
}
