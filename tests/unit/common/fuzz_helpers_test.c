#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "fuzz/fuzz_common.h"

static int noop_body(const uint8_t *data, size_t size) {
    (void)data;
    (void)size;
    return 0;
}

static char save_config[BT_PATH_MAX];
static char save_ground[BT_PATH_MAX];
static char save_artifacts[BT_PATH_MAX];

static void env_snapshot(void) {
    const char *config = getenv("BT_CONFIG");
    const char *ground = getenv("BT_REF_GROUND");
    const char *artifacts = getenv("BT_ARTIFACTS");
    snprintf(save_config, sizeof save_config, "%s", config != NULL ? config : "");
    snprintf(save_ground, sizeof save_ground, "%s", ground != NULL ? ground : "");
    snprintf(save_artifacts, sizeof save_artifacts, "%s", artifacts != NULL ? artifacts : "");
}

static void env_restore(void) {
    if (save_config[0] != '\0') {
        char setting[BT_PATH_MAX];
        snprintf(setting, sizeof setting, "BT_CONFIG=%s", save_config);
        _putenv(setting);
    } else {
        _putenv("BT_CONFIG=");
    }
    if (save_ground[0] != '\0') {
        char setting[BT_PATH_MAX];
        snprintf(setting, sizeof setting, "BT_REF_GROUND=%s", save_ground);
        _putenv(setting);
    } else {
        _putenv("BT_REF_GROUND=");
    }
    if (save_artifacts[0] != '\0') {
        char setting[BT_PATH_MAX];
        snprintf(setting, sizeof setting, "BT_ARTIFACTS=%s", save_artifacts);
        _putenv(setting);
    } else {
        _putenv("BT_ARTIFACTS=");
    }
}

static void write_config(const char *seed, const char *runs, const char *secs, char *out,
                         size_t cap) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    snprintf(out, cap, "%s\\build\\test-tmp\\fuzz-helpers-config.json", root);
    char body[256];
    snprintf(body, sizeof body, "{\"fuzz\":{\"seed\":%s,\"runs\":%s,\"secs\":%s}}", seed, runs,
             secs);
    FILE *file = fopen(out, "w");
    BT_CHECK(file != NULL);
    fputs(body, file);
    fclose(file);
}

BT_TEST(fuzz_helpers, seed_and_bounds_fall_back_when_the_hub_values_are_not_positive) {
    env_snapshot();
    char path[BT_PATH_MAX];
    write_config("-5", "-1", "-3", path, sizeof path);
    char setting[BT_PATH_MAX];
    snprintf(setting, sizeof setting, "BT_CONFIG=%s", path);
    _putenv(setting);
    BT_CHECK_EQ((long long)fuzz_seed(), 20261012LL);
    fuzz_bounds bounds;
    fuzz_bounds_init(&bounds);
    BT_CHECK_EQ((long long)bounds.runs, 5000LL);
    BT_CHECK_EQ((long long)bounds.secs, 30LL);
    env_restore();
}

BT_TEST(fuzz_helpers, seed_from_covers_short_and_long_inputs) {
    uint8_t short_input[3] = {1, 2, 3};
    uint8_t long_input[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint64_t from_short = bt_fuzz_seed_from(short_input, sizeof short_input);
    uint64_t from_long = bt_fuzz_seed_from(long_input, sizeof long_input);
    uint64_t from_empty = bt_fuzz_seed_from(short_input, 0);
    BT_CHECK(from_short != from_long);
    BT_CHECK(from_short != from_empty);
    BT_CHECK(from_long != from_empty);
}

BT_TEST(fuzz_helpers, repo_root_strips_trailing_ref_ground_and_passes_absolutes) {
    env_snapshot();
    char setting[BT_PATH_MAX];
    snprintf(setting, sizeof setting, "BT_REF_GROUND=C:\\abs\\probe\\ref_ground");
    _putenv(setting);
    char root[BT_PATH_MAX];
    fuzz_repo_root(root);
    BT_CHECK_EQ(root, "C:\\abs\\probe");
    _putenv("BT_REF_GROUND=C:\\abs\\probe");
    fuzz_repo_root(root);
    BT_CHECK_EQ(root, "C:\\abs\\probe");
    _putenv("BT_REF_GROUND=relative");
    char workspace[BT_PATH_MAX];
    bt_paths_workspace_root(workspace);
    fuzz_repo_root(root);
    BT_CHECK_EQ(root, workspace);
    env_restore();
}

BT_TEST(fuzz_helpers, artifacts_dir_falls_back_to_the_workspace_root_without_the_env) {
    env_snapshot();
    _putenv("BT_ARTIFACTS=");
    char out[BT_PATH_MAX];
    fuzz_artifacts_dir(out);
    char workspace[BT_PATH_MAX];
    bt_paths_workspace_root(workspace);
    char expected[BT_PATH_MAX];
    snprintf(expected, sizeof expected, "%s\\artifacts", workspace);
    BT_CHECK_EQ(out, expected);
    env_restore();
}

BT_TEST(fuzz_helpers, replay_dir_reports_a_missing_corpus) {
    fuzz_bounds bounds;
    bounds.active = true;
    bounds.runs = 10;
    bounds.done = 0;
    bounds.secs = 30;
    bounds.start = time(NULL);
    size_t ran = 0;
    BT_CHECK(!fuzz_replay_dir("no-such-corpus-xyz", noop_body, &bounds, &ran));
    BT_CHECK_EQ((long long)ran, 0LL);
}

static void make_dir(const char *path) {
    CreateDirectoryA(path, NULL);
}

BT_TEST(fuzz_helpers, replay_dir_skips_empty_and_locked_files_and_honors_the_runs_bound) {
    env_snapshot();
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char base[BT_PATH_MAX];
    snprintf(base, sizeof base, "%s\\build\\test-tmp\\fuzz-replay-root", root);
    make_dir(base);
    char tests[BT_PATH_MAX];
    snprintf(tests, sizeof tests, "%s\\tests", base);
    make_dir(tests);
    char fuzz[BT_PATH_MAX];
    snprintf(fuzz, sizeof fuzz, "%s\\fuzz", tests);
    make_dir(fuzz);
    char corpus[BT_PATH_MAX];
    snprintf(corpus, sizeof corpus, "%s\\corpus", fuzz);
    make_dir(corpus);
    char probe[BT_PATH_MAX];
    snprintf(probe, sizeof probe, "%s\\helper-probe", corpus);
    make_dir(probe);
    char empty[BT_PATH_MAX];
    snprintf(empty, sizeof empty, "%s\\empty.bin", probe);
    FILE *file = fopen(empty, "wb");
    BT_CHECK(file != NULL);
    fclose(file);
    char locked[BT_PATH_MAX];
    snprintf(locked, sizeof locked, "%s\\locked.bin", probe);
    file = fopen(locked, "wb");
    BT_CHECK(file != NULL);
    fputs("payload", file);
    fclose(file);
    HANDLE hold =
        CreateFileA(locked, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    BT_CHECK(hold != INVALID_HANDLE_VALUE);
    char setting[BT_PATH_MAX];
    snprintf(setting, sizeof setting, "BT_REF_GROUND=%s\\ref_ground", base);
    _putenv(setting);
    fuzz_bounds bounds;
    bounds.active = true;
    bounds.runs = 10;
    bounds.done = 0;
    bounds.secs = 30;
    bounds.start = time(NULL);
    size_t ran = 0;
    BT_CHECK(fuzz_replay_dir("helper-probe", noop_body, &bounds, &ran));
    BT_CHECK_EQ((long long)ran, 0LL);
    bounds.runs = 0;
    bounds.done = 0;
    ran = 0;
    BT_CHECK(fuzz_replay_dir("helper-probe", noop_body, &bounds, &ran));
    BT_CHECK_EQ((long long)ran, 0LL);
    CloseHandle(hold);
    env_restore();
}

BT_TEST(fuzz_helpers, crash_and_pending_writes_survive_an_unwritable_artifacts_root) {
    env_snapshot();
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char blocker[BT_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s\\build\\test-tmp\\fuzz-blocker.txt", root);
    FILE *file = fopen(blocker, "wb");
    BT_CHECK(file != NULL);
    fclose(file);
    char setting[BT_PATH_MAX];
    snprintf(setting, sizeof setting, "BT_ARTIFACTS=%s", blocker);
    _putenv(setting);
    uint8_t data[4] = {9, 9, 9, 9};
    fuzz_save_crash("helper", data, sizeof data);
    fuzz_pending_write("helper", data, sizeof data);
    env_restore();
    fuzz_save_crash("helper", data, sizeof data);
    char artifacts[BT_PATH_MAX];
    fuzz_artifacts_dir(artifacts);
    char crash[BT_PATH_MAX];
    fuzz_crash_path("helper", data, sizeof data, crash, sizeof crash);
    FILE *saved = fopen(crash, "rb");
    BT_CHECK(saved != NULL);
    fclose(saved);
    remove(crash);
    fuzz_pending_remove("helper");
    remove(blocker);
}
