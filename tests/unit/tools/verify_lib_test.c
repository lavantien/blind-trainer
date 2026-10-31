#include "unit/tools/spawn_util.h"
#include "verify_test_util.h"

#include <string.h>

#include "data/digest.h"
#include "verify/lib.h"

BT_TEST(verify_lib, crate_id_matches_package) {
    BT_CHECK_EQ(verify_crate_id(), "bt-verify");
}

BT_TEST(verify_lib, sheet_expectations_sum_to_the_corpus_total) {
    size_t sum = 0;
    for (size_t at = 0; at < VERIFY_SHEET_PIN_COUNT; at++) {
        sum += VERIFY_SHEET_EXPECTED_ROWS[at].rows;
    }
    BT_CHECK_EQ(sum, BT_DATA_TOTAL_ROWS);
}

BT_TEST(verify_lib, artifacts_dir_from_root_only_keeps_the_base_drive) {
    char out[BT_PATH_MAX];
    verify_artifacts_dir_from("/out", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/out/verify") == 0);
    verify_artifacts_dir_from("C:ga", "R:/ws", out);
    BT_CHECK(strcmp(out, "C:ga/verify") == 0);
    verify_artifacts_dir_from("rel", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/ws/rel/verify") == 0);
    verify_artifacts_dir_from("C:/rg/", "R:/ws", out);
    BT_CHECK(strcmp(out, "C:/rg/verify") == 0);
}

BT_TEST(verify_lib, mkdirs_skip_existing_drive_root) {
    char temp[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, temp);
    if (n == 0 || n >= BT_PATH_MAX || temp[1] != ':') {
        BT_ABORT("no temp drive root");
    }
    char base[BT_PATH_MAX];
    snprintf(base, BT_PATH_MAX, "%sbt-verify-pin-%lu", temp, GetCurrentProcessId());
    tv_rm_rf(base);
    char saved[BT_PATH_MAX];
    DWORD had = GetEnvironmentVariableA("BT_ARTIFACTS", saved, BT_PATH_MAX);
    SetEnvironmentVariableA("BT_ARTIFACTS", base);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "verify.exe");
    const char *argv[] = {NULL};
    char out_log[4096];
    char err_log[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 120000, &code, out_log, sizeof out_log, err_log,
                              sizeof err_log);
    SetEnvironmentVariableA("BT_ARTIFACTS", had > 0 && had < BT_PATH_MAX ? saved : NULL);
    BT_CHECK_EQ(rc, 0);
    char packs[BT_PATH_MAX];
    snprintf(packs, BT_PATH_MAX, "%s/verify/packs.md", base);
    FILE *file = fopen(packs, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fclose(file);
    }
    tv_rm_rf(base);
}
