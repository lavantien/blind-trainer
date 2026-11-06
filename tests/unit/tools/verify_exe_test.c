#include "common/bt_harness.h"
#include "common/paths.h"
#include "unit/tools/spawn_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void rm_scratch_root(const char *path);

static void scratch_root(const char *tag, char out[BT_PATH_MAX]) {
    char base[BT_PATH_MAX];
    DWORD n = GetTempPathA(BT_PATH_MAX, base);
    BT_CHECK(n > 0 && n < BT_PATH_MAX);
    snprintf(out, BT_PATH_MAX, "%sbt-verify-exe-%s-%lu", base, tag, GetCurrentProcessId());
    rm_scratch_root(out);
    CreateDirectoryA(out, NULL);
}

static void rm_scratch_root(const char *path) {
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
            rm_scratch_root(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
    RemoveDirectoryA(path);
}

static void copy_tree(const char *from, const char *to) {
    CreateDirectoryA(to, NULL);
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", from);
    WIN32_FIND_DATAA found;
    HANDLE scan = FindFirstFileA(pattern, &found);
    if (scan == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char child_from[BT_PATH_MAX];
        char child_to[BT_PATH_MAX];
        snprintf(child_from, sizeof child_from, "%s/%s", from, found.cFileName);
        snprintf(child_to, sizeof child_to, "%s/%s", to, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            copy_tree(child_from, child_to);
        } else {
            CopyFileA(child_from, child_to, FALSE);
        }
    } while (FindNextFileA(scan, &found));
    FindClose(scan);
}

static void repo_ref_ground(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/ref_ground", root);
}

static void clone_ref_ground(const char *tag, char out[BT_PATH_MAX]) {
    scratch_root(tag, out);
    char source[BT_PATH_MAX];
    repo_ref_ground(source);
    copy_tree(source, out);
}

static void overwrite_file(const char *path, const char *body) {
    FILE *file = fopen(path, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs(body, file);
        fclose(file);
    }
}

static void replace_in_file(const char *path, const char *needle, const char *replacement) {
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    static char text[1U << 20U];
    size_t len = 0;
    if (file != NULL) {
        len = fread(text, 1, sizeof text - 1, file);
        fclose(file);
    }
    text[len] = '\0';
    char *hit = strstr(text, needle);
    BT_CHECK(hit != NULL);
    if (hit == NULL) {
        return;
    }
    FILE *sink = fopen(path, "wb");
    BT_CHECK(sink != NULL);
    if (sink != NULL) {
        fwrite(text, 1, (size_t)(hit - text), sink);
        fputs(replacement, sink);
        fputs(hit + strlen(needle), sink);
        fclose(sink);
    }
}

static int spawn_verify(const char *ref_ground_value, const char *artifacts_value,
                        const char *config_value, char *out, size_t out_cap, char *err,
                        size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "verify.exe");
    char saved_rg[BT_PATH_MAX];
    char saved_artifacts[BT_PATH_MAX];
    char saved_config[BT_PATH_MAX];
    DWORD had_rg = GetEnvironmentVariableA("BT_REF_GROUND", saved_rg, BT_PATH_MAX);
    DWORD had_artifacts = GetEnvironmentVariableA("BT_ARTIFACTS", saved_artifacts, BT_PATH_MAX);
    DWORD had_config = GetEnvironmentVariableA("BT_CONFIG", saved_config, BT_PATH_MAX);
    SetEnvironmentVariableA("BT_REF_GROUND", ref_ground_value);
    SetEnvironmentVariableA("BT_ARTIFACTS", artifacts_value);
    SetEnvironmentVariableA("BT_CONFIG", config_value);
    const char *argv[] = {NULL};
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, NULL, 180000, &code, out, out_cap, err, err_cap);
    SetEnvironmentVariableA("BT_REF_GROUND", had_rg > 0 && had_rg < BT_PATH_MAX ? saved_rg : NULL);
    SetEnvironmentVariableA(
        "BT_ARTIFACTS", had_artifacts > 0 && had_artifacts < BT_PATH_MAX ? saved_artifacts : NULL);
    SetEnvironmentVariableA("BT_CONFIG",
                            had_config > 0 && had_config < BT_PATH_MAX ? saved_config : NULL);
    BT_CHECK_EQ(rc, 0);
    return code;
}

BT_TEST(verify_exe, reports_an_unreadable_config) {
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, "artifacts", "Z:/no-such/verify-config.json", out, sizeof out,
                            err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "config unreadable") != NULL);
}

BT_TEST(verify_exe, reports_a_corpus_failure_on_an_empty_root) {
    char root[BT_PATH_MAX];
    scratch_root("empty", root);
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "cherry/edge.tsv") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_simcheck_header_change) {
    char root[BT_PATH_MAX];
    clone_ref_ground("simbad", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/simcheck.tsv", root);
    overwrite_file(path, "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnotes\n");
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "header changed") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_config_without_the_artifacts_section) {
    char root[BT_PATH_MAX];
    clone_ref_ground("noartifacts", root);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/minimal.json", root);
    overwrite_file(config, "{}");
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, NULL, config, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "config section missing: paths.artifacts") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_failing_vendor_row_as_an_offender) {
    char root[BT_PATH_MAX];
    clone_ref_ground("badrow", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/cherry/edge.tsv", root);
    replace_in_file(path, "R2 U' S R2 S' R2 U R2", "ZZ U' S R2 S' R2 U R2");
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "rowid 4") != NULL);
    rm_scratch_root(root);
}

static void plant_readonly_file(const char *path) {
    FILE *file = fopen(path, "wb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        fputs("blocked", file);
        fclose(file);
    }
    BT_CHECK(SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY) != 0);
}

BT_TEST(verify_exe, writes_all_three_reports_under_a_fresh_artifacts_root) {
    char artifacts[BT_PATH_MAX];
    scratch_root("reports", artifacts);
    char out[4096];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "rows pass") != NULL);
    code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
    const char *names[] = {"report.md", "report.json", "packs.md"};
    for (size_t at = 0; at < 3; at++) {
        char path[BT_PATH_MAX];
        snprintf(path, sizeof path, "%s/verify/%s", artifacts, names[at]);
        WIN32_FIND_DATAA found;
        HANDLE scan = FindFirstFileA(path, &found);
        BT_CHECK(scan != INVALID_HANDLE_VALUE);
        if (scan != INVALID_HANDLE_VALUE) {
            FindClose(scan);
        }
    }
    rm_scratch_root(artifacts);
}

BT_TEST(verify_exe, resolves_the_artifacts_root_from_the_config_when_env_is_unset) {
    char out[4096];
    char err[4096];
    int code = spawn_verify(NULL, NULL, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 0);
}

BT_TEST(verify_exe, reports_a_row_that_verifies_wrong) {
    char root[BT_PATH_MAX];
    clone_ref_ground("wrongpos", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/cherry/edge.tsv", root);
    replace_in_file(path, "UF-UB-UL", "UF-UB-UR");
    char out[2048];
    char err[8192];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "cherry/edge/2") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_readonly_packs_report) {
    char artifacts[BT_PATH_MAX];
    scratch_root("ropacks", artifacts);
    char dir_path[BT_PATH_MAX];
    snprintf(dir_path, sizeof dir_path, "%s/verify", artifacts);
    CreateDirectoryA(dir_path, NULL);
    char packs[BT_PATH_MAX];
    snprintf(packs, sizeof packs, "%s/packs.md", dir_path);
    plant_readonly_file(packs);
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "write") != NULL);
    SetFileAttributesA(packs, FILE_ATTRIBUTE_NORMAL);
    rm_scratch_root(artifacts);
}

BT_TEST(verify_exe, reports_a_readonly_json_report) {
    char artifacts[BT_PATH_MAX];
    scratch_root("rojson", artifacts);
    char dir_path[BT_PATH_MAX];
    snprintf(dir_path, sizeof dir_path, "%s/verify", artifacts);
    CreateDirectoryA(dir_path, NULL);
    char report[BT_PATH_MAX];
    snprintf(report, sizeof report, "%s/report.json", dir_path);
    plant_readonly_file(report);
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "report.json") != NULL);
    SetFileAttributesA(report, FILE_ATTRIBUTE_NORMAL);
    rm_scratch_root(artifacts);
}

BT_TEST(verify_exe, reports_a_readonly_markdown_report) {
    char artifacts[BT_PATH_MAX];
    scratch_root("romd", artifacts);
    char dir_path[BT_PATH_MAX];
    snprintf(dir_path, sizeof dir_path, "%s/verify", artifacts);
    CreateDirectoryA(dir_path, NULL);
    char report[BT_PATH_MAX];
    snprintf(report, sizeof report, "%s/report.md", dir_path);
    plant_readonly_file(report);
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "report.md") != NULL);
    SetFileAttributesA(report, FILE_ATTRIBUTE_NORMAL);
    rm_scratch_root(artifacts);
}

BT_TEST(verify_exe, reports_a_blocked_artifacts_root) {
    char root[BT_PATH_MAX];
    clone_ref_ground("blocked", root);
    char base[BT_PATH_MAX];
    scratch_root("blocker", base);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker.txt", base);
    overwrite_file(blocker, "in the way");
    char artifacts[BT_PATH_MAX];
    snprintf(artifacts, sizeof artifacts, "%s/blocker.txt/verify", base);
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "write") != NULL);
    rm_scratch_root(root);
    DeleteFileA(blocker);
    rm_scratch_root(base);
}

BT_TEST(verify_exe, reports_registry_drift_as_a_pack_offender) {
    char root[BT_PATH_MAX];
    clone_ref_ground("drift", root);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/packs/registry.json", root);
    replace_in_file(path, "\"expected_rows\": 42", "\"expected_rows\": 41");
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "registry holds 41 rows, pin holds 42") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_missing_ref_ground_root) {
    char out[2048];
    char err[4096];
    int code =
        spawn_verify("Z:/no-such-ground", "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "no-such-ground") != NULL);
}

BT_TEST(verify_exe, reports_a_clone_without_the_packs_root) {
    char root[BT_PATH_MAX];
    clone_ref_ground("nopacks", root);
    char packs[BT_PATH_MAX];
    snprintf(packs, sizeof packs, "%s/packs", root);
    rm_scratch_root(packs);
    char out[2048];
    char err[4096];
    int code = spawn_verify(root, "artifacts", NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "packs") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_config_without_the_ref_ground_section) {
    char root[BT_PATH_MAX];
    scratch_root("norefg", root);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/minimal.json", root);
    overwrite_file(config, "{}");
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, "artifacts", config, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "paths.ref_ground") != NULL);
    rm_scratch_root(root);
}

BT_TEST(verify_exe, reports_a_config_without_the_packs_section) {
    char root[BT_PATH_MAX];
    scratch_root("nopacksc", root);
    char config[BT_PATH_MAX];
    snprintf(config, sizeof config, "%s/minimal.json", root);
    overwrite_file(config, "{\"paths\": {\"ref_ground\": \"ref_ground\"}}");
    char artifacts[BT_PATH_MAX];
    scratch_root("nopacksart", artifacts);
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, config, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "paths.packs") != NULL);
    rm_scratch_root(root);
    rm_scratch_root(artifacts);
}

BT_TEST(verify_exe, reports_a_truncated_backslash_artifacts_root) {
    char artifacts[2300];
    size_t used = (size_t)snprintf(artifacts, sizeof artifacts, "\\\\127.0.0.1\\no-such-share\\");
    while (used + 3 < sizeof artifacts) {
        artifacts[used++] = 'a';
        artifacts[used++] = '\\';
    }
    artifacts[used] = '\0';
    char out[2048];
    char err[4096];
    int code = spawn_verify(NULL, artifacts, NULL, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "write \\/127.0.0.1/no-such-share/a/a") != NULL);
}
