#include "unit/tools/spawn_util.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"

static const char DRIVER_CAUGHT[] =
    "{\"total_mutants\":1,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":0,"
    "\"outcomes\":[{\"scenario\":\"Baseline\",\"summary\":\"Success\"},{\"scenario\":{"
    "\"Mutant\":{\"name\":\"src/z.c:3:5: NumericTweak 1 -> 2 in f\",\"package\":\"blind-trainer"
    "\",\"file\":\"src/z.c\",\"function\":{\"function_name\":\"f\"},\"span\":{\"start\":{"
    "\"line\":3,\"column\":5},\"end\":{\"line\":3,\"column\":6}},\"replacement\":\"2\","
    "\"genre\":\"NumericTweak\"}},\"summary\":\"CaughtMutant\"}]}";

static const char DRIVER_MISSED[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":1,\"timeout\":0,\"unviable\":0,\"success\":0,"
    "\"outcomes\":[{\"scenario\":\"Baseline\",\"summary\":\"Success\"},{\"scenario\":{"
    "\"Mutant\":{\"name\":\"src/z.c:3:5: NumericTweak 1 -> 2 in f\",\"package\":\"blind-trainer"
    "\",\"file\":\"src/z.c\",\"function\":{\"function_name\":\"f\"},\"span\":{\"start\":{"
    "\"line\":3,\"column\":5},\"end\":{\"line\":3,\"column\":6}},\"replacement\":\"2\","
    "\"genre\":\"NumericTweak\"}},\"summary\":\"MissedMutant\"}]}";

static const char DRIVER_MERGE_A[] =
    "{\"end_time\":\"2026-10-27T10:10:00Z\",\"total_mutants\":1,\"caught\":1,\"missed\":0,"
    "\"timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[{\"scenario\":\"Baseline\","
    "\"summary\":\"Success\"},{\"scenario\":{\"Mutant\":{\"name\":\"src/z.c:3:5: NumericTweak "
    "1 -> 2 in f\",\"package\":\"blind-trainer\",\"file\":\"src/z.c\",\"function\":{"
    "\"function_name\":\"f\"},\"span\":{\"start\":{\"line\":3,\"column\":5},\"end\":{"
    "\"line\":3,\"column\":6}},\"replacement\":\"2\",\"genre\":\"NumericTweak\"}},"
    "\"summary\":\"CaughtMutant\"}]}";

static const char DRIVER_MERGE_B[] =
    "{\"end_time\":\"2026-10-27T10:30:00Z\",\"total_mutants\":1,\"caught\":0,\"missed\":1,"
    "\"timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[{\"scenario\":\"Baseline\","
    "\"summary\":\"Success\"},{\"scenario\":{\"Mutant\":{\"name\":\"src/z.c:9:5: NumericTweak "
    "0 -> 1 in g\",\"package\":\"blind-trainer\",\"file\":\"src/z.c\",\"function\":{"
    "\"function_name\":\"g\"},\"span\":{\"start\":{\"line\":9,\"column\":5},\"end\":{"
    "\"line\":9,\"column\":6}},\"replacement\":\"1\",\"genre\":\"NumericTweak\"}},"
    "\"summary\":\"MissedMutant\"}]}";

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(text, 1, strlen(text), file);
    fclose(file);
}

static void scratch_dir(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/mutants-gate-driver", root);
    bt_test_mkdirs(out);
}

BT_TEST(gate_driver, selftest_passes) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {"selftest", NULL};
    char out[16384];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 120000, &code, out, sizeof out, err, sizeof err);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "spawn failed: %s", bt_spawn_diag());
    }
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "suite mutants-gate: 42 passed, 0 failed") != NULL);
}

BT_TEST(gate_driver, usage_without_args_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage") != NULL);
}

BT_TEST(gate_driver, missing_input_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    char missing[BT_PATH_MAX];
    snprintf(missing, BT_PATH_MAX, "%s/build/test-tmp/mutants-gate-driver/absent.json", root);
    const char *argv[] = {missing, missing, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "cannot read") != NULL);
}

BT_TEST(gate_driver, clean_run_prints_summary_and_exits_0) {
    char dir[BT_PATH_MAX];
    scratch_dir(dir);
    char outcomes[BT_PATH_MAX];
    char allowlist[BT_PATH_MAX];
    snprintf(outcomes, BT_PATH_MAX, "%s/caught.json", dir);
    snprintf(allowlist, BT_PATH_MAX, "%s/allowlist.tsv", dir);
    write_text(outcomes, DRIVER_CAUGHT);
    write_text(allowlist, "# columns: file\tfunction\tgenre\treplacement\tcount\tjustification\n");
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {outcomes, allowlist, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "survivors gated: 0, all covered") != NULL);
}

BT_TEST(gate_driver, unruled_survivor_exits_1) {
    char dir[BT_PATH_MAX];
    scratch_dir(dir);
    char outcomes[BT_PATH_MAX];
    char allowlist[BT_PATH_MAX];
    snprintf(outcomes, BT_PATH_MAX, "%s/missed.json", dir);
    snprintf(allowlist, BT_PATH_MAX, "%s/empty.tsv", dir);
    write_text(outcomes, DRIVER_MISSED);
    write_text(allowlist, "# columns: file\tfunction\tgenre\treplacement\tcount\tjustification\n");
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {outcomes, allowlist, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 1);
    BT_CHECK(strstr(err, "mutants gate:") != NULL);
    BT_CHECK(strstr(err, "not in allowlist") != NULL);
}

BT_TEST(gate_driver, merge_single_input_exits_2) {
    char dir[BT_PATH_MAX];
    scratch_dir(dir);
    char input_a[BT_PATH_MAX];
    char output[BT_PATH_MAX];
    snprintf(input_a, BT_PATH_MAX, "%s/merge-a.json", dir);
    snprintf(output, BT_PATH_MAX, "%s/merged.json", dir);
    write_text(input_a, DRIVER_MERGE_A);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {"merge", output, input_a, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "mutants merge: merge needs at least two input files") != NULL);
}

BT_TEST(gate_driver, merge_two_inputs_writes_output) {
    char dir[BT_PATH_MAX];
    scratch_dir(dir);
    char input_a[BT_PATH_MAX];
    char input_b[BT_PATH_MAX];
    char output[BT_PATH_MAX];
    snprintf(input_a, BT_PATH_MAX, "%s/merge-a.json", dir);
    snprintf(input_b, BT_PATH_MAX, "%s/merge-b.json", dir);
    snprintf(output, BT_PATH_MAX, "%s/merged.json", dir);
    write_text(input_a, DRIVER_MERGE_A);
    write_text(input_b, DRIVER_MERGE_B);
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutants-gate.exe");
    const char *argv[] = {"merge", output, input_a, input_b, NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "merged 2 inputs, 2 mutants") != NULL);
    FILE *file = fopen(output, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[8192];
        size_t got = fread(text, 1, sizeof text - 1, file);
        fclose(file);
        text[got] = '\0';
        BT_CHECK(strstr(text, "\"total_mutants\":2") != NULL);
        BT_CHECK(strstr(text, "\"missed\":1") != NULL);
        BT_CHECK(strstr(text, "\"end_time\":\"2026-10-27T10:30:00Z\"") != NULL);
    }
}
