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

static void scratch_simcheck(const char *tag, const char *body, char root_out[BT_PATH_MAX]) {
    char temp[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, temp);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(root_out, BT_PATH_MAX, "%sbt-simcheck-%s-%lu", temp, tag, GetCurrentProcessId());
    CreateDirectoryA(root_out, NULL);
    if (body != NULL) {
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s/simcheck.tsv", root_out);
        FILE *file = fopen(path, "wb");
        BT_CHECK(file != NULL);
        if (file != NULL) {
            fputs(body, file);
            fclose(file);
        }
    }
}

static void drop_scratch_simcheck(const char *root) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/simcheck.tsv", root);
    DeleteFileA(path);
    RemoveDirectoryA(root);
}

static int simcheck_error_is(const char *tag, const char *body, const char *needle) {
    char root[BT_PATH_MAX];
    scratch_simcheck(tag, body, root);
    verify_sim_map map;
    verify_sim_map_init(&map);
    char err[VERIFY_ERR_MAX];
    err[0] = '\0';
    int rc = verify_load_simcheck(root, &map, err, sizeof err);
    verify_sim_map_free(&map);
    drop_scratch_simcheck(root);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(err, needle) != NULL);
    return 1;
}

BT_TEST(verify_lib, simcheck_rejects_each_malformed_shape) {
    BT_CHECK(simcheck_error_is("absent", NULL, "simcheck.tsv"));
    BT_CHECK(simcheck_error_is(
        "header", "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnotes\n", "header changed"));
    BT_CHECK(simcheck_error_is("shortrow",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t2\tUF-UB-UL\t9\t12\tCYCLE_A\n",
                               "has 7 columns"));
    BT_CHECK(simcheck_error_is("solver",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "stranger\tedge\t2\tUF-UB-UL\t9\t12\tCYCLE_A\tclean\n",
                               "solver stranger is unknown"));
    BT_CHECK(simcheck_error_is("sheet",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tcenters\t2\tUF-UB-UL\t9\t12\tCYCLE_A\tclean\n",
                               "sheet centers is unknown"));
    BT_CHECK(simcheck_error_is("rowid",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\ttwo\tUF-UB-UL\t9\t12\tCYCLE_A\tclean\n",
                               "rowid two is not a number"));
    BT_CHECK(simcheck_error_is("qtm",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t2\tUF-UB-UL\t9\t1e3\tCYCLE_A\tclean\n",
                               "qtm 1e3 is not a number"));
    BT_CHECK(simcheck_error_is("empty", "", "simcheck.tsv is empty"));
    BT_CHECK(simcheck_error_is("rowoverflow",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t18446744073709551616\tUF-UB-UL\t9\t12\tCYCLE_A\tc\n",
                               "rowid 18446744073709551616 is not a number"));
    BT_CHECK(simcheck_error_is("qtmoverflow",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t2\tUF-UB-UL\t9\t99999999999999999999\tCYCLE_A\tc\n",
                               "qtm 99999999999999999999 is not a number"));
    BT_CHECK(simcheck_error_is("qtmwide",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t2\tUF-UB-UL\t9\t4294967296\tCYCLE_A\tc\n",
                               "qtm 4294967296 is not a number"));
    BT_CHECK(simcheck_error_is("rowidplus",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t+\tUF-UB-UL\t9\t12\tCYCLE_A\tclean\n",
                               "rowid + is not a number"));
    BT_CHECK(simcheck_error_is("qtmplus",
                               "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                               "cherry\tedge\t2\tUF-UB-UL\t9\t+\tCYCLE_A\tclean\n",
                               "qtm + is not a number"));
}

BT_TEST(verify_lib, simcheck_accepts_leading_plus_in_rowid_and_qtm) {
    char root[BT_PATH_MAX];
    scratch_simcheck("plus",
                     "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                     "eggins\tcorner\t+7\tUFR-UBR-UL\t9\t+21\tCYCLE_A\tclean\n",
                     root);
    verify_sim_map map;
    verify_sim_map_init(&map);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_load_simcheck(root, &map, err, sizeof err), 0);
    const verify_sim_row *row = verify_sim_map_get(&map, BT_SOLVER_EGGINS, BT_SHEET_CORNER, 7);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(row->qtm, 21U);
    }
    verify_sim_map_free(&map);
    drop_scratch_simcheck(root);
}

BT_TEST(verify_lib, simcheck_accepts_blank_lines_and_replaces_duplicates) {
    char root[BT_PATH_MAX];
    scratch_simcheck("dupes",
                     "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote\n"
                     "\n"
                     "cherry\tedge\t2\tUF-UB-UL\t9\t12\tCYCLE_A\tclean\n"
                     "cherry\tedge\t2\tUF-UB-UR\t9\t13\tCYCLE_A\treplaced\n",
                     root);
    verify_sim_map map;
    verify_sim_map_init(&map);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_load_simcheck(root, &map, err, sizeof err), 0);
    BT_CHECK_EQ(map.count, 1U);
    const verify_sim_row *row = verify_sim_map_get(&map, BT_SOLVER_CHERRY, BT_SHEET_EDGE, 2);
    BT_CHECK(row != NULL);
    if (row != NULL) {
        BT_CHECK_EQ(strcmp(row->position, "UF-UB-UR"), 0);
        BT_CHECK_EQ(row->qtm, 13U);
    }
    BT_CHECK(verify_sim_map_get(&map, BT_SOLVER_EGGINS, BT_SHEET_EDGE, 2) == NULL);
    verify_sim_map_free(&map);
    drop_scratch_simcheck(root);
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
