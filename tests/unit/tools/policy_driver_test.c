#include "policy_test_util.h"
#include "unit/tools/spawn_util.h"

#include <errno.h>
#include <string.h>

#include "policy/lib.h"

static void abs_scratch(const char *tag, char out[POLICY_UTIL_PATH_MAX]) {
    char rel[POLICY_UTIL_PATH_MAX];
    policy_util_scratch(tag, rel);
    char native[POLICY_UTIL_PATH_MAX];
    snprintf(native, POLICY_UTIL_PATH_MAX, "%s", rel);
    for (char *cursor = native; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    wchar_t wide[POLICY_UTIL_PATH_MAX];
    MultiByteToWideChar(CP_UTF8, 0, native, -1, wide, POLICY_UTIL_PATH_MAX);
    wchar_t wide_out[BT_PATH_MAX];
    GetFullPathNameW(wide, BT_PATH_MAX, wide_out, NULL);
    WideCharToMultiByte(CP_UTF8, 0, wide_out, -1, out, POLICY_UTIL_PATH_MAX, NULL, NULL);
}

static void copy_exe(const char *dest) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "policy.exe");
    FILE *in = fopen(exe, "rb");
    if (in == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "cannot open %s: errno %d", exe, errno);
    }
    BT_CHECK(in != NULL);
    FILE *out = fopen(dest, "wb");
    BT_CHECK(out != NULL);
    if (in == NULL || out == NULL) {
        if (in != NULL) {
            fclose(in);
        }
        if (out != NULL) {
            fclose(out);
        }
        return;
    }
    char chunk[8192];
    size_t got = 0;
    while ((got = fread(chunk, 1, sizeof chunk, in)) > 0) {
        fwrite(chunk, 1, got, out);
    }
    fclose(in);
    fclose(out);
}

static void spawn_repo(const char *const argv[], int want_code, const char *want_out,
                       const char *want_err) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "policy.exe");
    char out[16384];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 120000, &code, out, sizeof out, err, sizeof err);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn rc %d diag '%s'", rc, bt_spawn_diag());
    }
    BT_CHECK_EQ(rc, 0);
    if (code != want_code) {
        bt_harness_fail(__FILE__, __LINE__, "code %d want %d out '%.600s' err '%.600s'", code,
                        want_code, out, err);
    }
    if (want_out != NULL && strstr(out, want_out) == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "want '%s' missing from out '%.600s'", want_out, out);
    }
    if (want_err != NULL && strstr(err, want_err) == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "want '%s' missing from err '%.600s'", want_err, err);
    }
}

static void repo_verdict_is_consistent(const char *const argv[]) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "policy.exe");
    char out[16384];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 120000, &code, out, sizeof out, err, sizeof err);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn rc %d diag '%s'", rc, bt_spawn_diag());
    }
    BT_CHECK_EQ(rc, 0);
    if (code == 0) {
        if (strstr(out, "bt-policy: ") != NULL) {
            bt_harness_fail(__FILE__, __LINE__, "exit 0 printed findings '%.600s'", out);
        }
        return;
    }
    if (code == 1) {
        if (strstr(out, "bt-policy: ") == NULL) {
            bt_harness_fail(__FILE__, __LINE__, "exit 1 without findings, out '%.600s'", out);
        }
        return;
    }
    bt_harness_fail(__FILE__, __LINE__, "verdict %d outside the contract, err '%.600s'", code, err);
}

BT_TEST(policy_driver, bare_run_over_the_repo_keeps_the_verdict_contract) {
    const char *argv[] = {NULL};
    repo_verdict_is_consistent(argv);
}

BT_TEST(policy_driver, every_mode_flag_over_the_repo_keeps_the_verdict_contract) {
    const char *comments[] = {"comments", NULL};
    const char *constants[] = {"constants", NULL};
    const char *sloc[] = {"sloc", NULL};
    repo_verdict_is_consistent(comments);
    repo_verdict_is_consistent(constants);
    repo_verdict_is_consistent(sloc);
}

BT_TEST(policy_driver, unknown_mode_prints_usage_and_exits_2) {
    const char *argv[] = {"nonsense", NULL};
    spawn_repo(argv, 2, NULL, "usage: bt-policy");
}

BT_TEST(policy_driver, extra_arguments_print_usage_and_exits_2) {
    const char *argv[] = {"comments", "extra", NULL};
    spawn_repo(argv, 2, NULL, "usage: bt-policy");
}

static void selftest_spawn(const char *workdir, int want_code, const char *want_err) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "policy.exe");
    const char *argv[] = {"selftest", workdir, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn rc %d diag '%s'", rc, bt_spawn_diag());
    }
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, want_code);
    if (want_err != NULL) {
        BT_CHECK(strstr(err, want_err) != NULL);
    }
}

BT_TEST(policy_driver, selftest_ok_maps_success_to_zero) {
    char workdir[POLICY_UTIL_PATH_MAX];
    abs_scratch("driver-selftest", workdir);
    selftest_spawn(workdir, 0, NULL);
    char run_dir[POLICY_UTIL_PATH_MAX];
    BT_CHECK_EQ(policy_util_find_child(workdir, "pol-", run_dir), 1);
    policy_util_rm_rf(workdir);
}

BT_TEST(policy_driver, selftest_workdir_under_a_file_exits_2) {
    char workdir[POLICY_UTIL_PATH_MAX];
    abs_scratch("driver-badworkdir", workdir);
    char blocker[POLICY_UTIL_PATH_MAX];
    policy_util_join(blocker, workdir, "blocker.txt");
    policy_util_write(blocker, "x\n");
    char under[POLICY_UTIL_PATH_MAX];
    snprintf(under, POLICY_UTIL_PATH_MAX, "%s/sub", blocker);
    selftest_spawn(under, 2, "write ");
    policy_util_rm_rf(workdir);
}

static void huge_workdir(char out[BT_PATH_MAX], size_t len) {
    memset(out, 'd', len);
    out[0] = 'C';
    out[1] = ':';
    out[2] = '/';
    out[len] = '\0';
}

BT_TEST(policy_driver, selftest_workdir_past_the_dir_pattern_limit_exits_2) {
    static char huge[BT_PATH_MAX];
    huge_workdir(huge, 1023);
    selftest_spawn(huge, 2, "too long");
}

BT_TEST(policy_driver, selftest_workdir_past_the_run_dir_limit_exits_2) {
    static char huge[BT_PATH_MAX];
    huge_workdir(huge, 1020);
    selftest_spawn(huge, 2, "run dir too long");
}

static void build_fixture_ws(char root[POLICY_UTIL_PATH_MAX], const char *tag,
                             const char *allowlist) {
    abs_scratch(tag, root);
    char config_dir[POLICY_UTIL_PATH_MAX];
    policy_util_join(config_dir, root, "config");
    policy_util_mkdir_p(config_dir);
    char config_path[POLICY_UTIL_PATH_MAX];
    policy_util_join(config_path, config_dir, "config.json");
    policy_util_write(config_path, POLICY_SELFTEST_CONFIG);
    char allow_path[POLICY_UTIL_PATH_MAX];
    policy_util_join(allow_path, config_dir, "constants_allow.txt");
    policy_util_write(allow_path, allowlist);
    char src_dir[POLICY_UTIL_PATH_MAX];
    policy_util_join(src_dir, root, "src");
    policy_util_mkdir_p(src_dir);
    char dirty[POLICY_UTIL_PATH_MAX];
    policy_util_join(dirty, src_dir, "dirty.c");
    policy_util_write(dirty, POLICY_DIRTY_FIXTURE);
    char exe_slot[POLICY_UTIL_PATH_MAX];
    policy_util_join(exe_slot, root, "pol-bin");
    policy_util_mkdir_p(exe_slot);
    char exe_dest[POLICY_UTIL_PATH_MAX];
    policy_util_join(exe_dest, exe_slot, "policy.exe");
    copy_exe(exe_dest);
}

static void spawn_fixture(const char *root, const char *config, const char *const argv[],
                          int want_code, const char *want_out, const char *want_err) {
    char exe[POLICY_UTIL_PATH_MAX];
    policy_util_join(exe, root, "pol-bin/policy.exe");
    char saved_config[POLICY_UTIL_PATH_MAX] = {0};
    const char *saved = getenv("BT_CONFIG");
    if (saved != NULL && saved[0] != '\0') {
        snprintf(saved_config, sizeof saved_config, "%s", saved);
    }
    BT_CHECK_EQ(_putenv_s("BT_CONFIG", config), 0);
    char out[16384];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 120000, &code, out, sizeof out, err, sizeof err);
    if (saved_config[0] != '\0') {
        BT_CHECK_EQ(_putenv_s("BT_CONFIG", saved_config), 0);
    } else {
        BT_CHECK_EQ(_putenv_s("BT_CONFIG", ""), 0);
    }
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn rc %d diag '%s'", rc, bt_spawn_diag());
    }
    BT_CHECK_EQ(rc, 0);
    if (code != want_code) {
        bt_harness_fail(__FILE__, __LINE__, "code %d want %d out '%.600s' err '%.600s'", code,
                        want_code, out, err);
    }
    if (want_out != NULL && strstr(out, want_out) == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "want '%s' missing from out '%.600s'", want_out, out);
    }
    if (want_err != NULL && strstr(err, want_err) == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "want '%s' missing from err '%.600s'", want_err, err);
    }
}

BT_TEST(policy_driver, fixture_findings_print_and_exit_1) {
    char root[POLICY_UTIL_PATH_MAX];
    build_fixture_ws(root, "driver-findings", "# allow\n");
    char config[POLICY_UTIL_PATH_MAX];
    policy_util_join(config, root, "config/config.json");
    const char *argv[] = {NULL};
    spawn_fixture(root, config, argv, 1, "bt-policy: src/dirty.c:", NULL);
    spawn_fixture(root, config, argv, 1, "line comment", NULL);
    spawn_fixture(root, config, argv, 1, "block comment", NULL);
    spawn_fixture(root, config, argv, 1, "literal 7 x2", NULL);
    policy_util_rm_rf(root);
}

BT_TEST(policy_driver, fixture_allowlist_silences_and_stale_reports) {
    char root[POLICY_UTIL_PATH_MAX];
    build_fixture_ws(root, "driver-allow", "src/dirty.c\t7\tlegacy table\nsrc/gone.c\t9\tnone\n");
    char config[POLICY_UTIL_PATH_MAX];
    policy_util_join(config, root, "config/config.json");
    const char *argv[] = {"constants", NULL};
    spawn_fixture(root, config, argv, 0, NULL, NULL);
    spawn_fixture(root, config, argv, 0, NULL, "stale allowlist entry: src/gone.c TAB 9");
    policy_util_rm_rf(root);
}

BT_TEST(policy_driver, missing_config_exits_2) {
    char root[POLICY_UTIL_PATH_MAX];
    build_fixture_ws(root, "driver-cfg-missing", "# allow\n");
    char missing[POLICY_UTIL_PATH_MAX];
    policy_util_join(missing, root, "absent-config.json");
    const char *argv[] = {NULL};
    spawn_fixture(root, missing, argv, 2, NULL, "read ");
    policy_util_rm_rf(root);
}

BT_TEST(policy_driver, unparsable_config_exits_2) {
    char root[POLICY_UTIL_PATH_MAX];
    build_fixture_ws(root, "driver-cfg-broken", "# allow\n");
    char broken[POLICY_UTIL_PATH_MAX];
    policy_util_join(broken, root, "broken-config.json");
    policy_util_write(broken, "{ not json");
    const char *argv[] = {NULL};
    spawn_fixture(root, broken, argv, 2, NULL, "parse ");
    policy_util_rm_rf(root);
}
