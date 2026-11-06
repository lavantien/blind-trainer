#include "mutate/mutate.h"
#include "unit/tools/spawn_util.h"

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FX_REL "tests/unit/tools/fixtures/fx_target.c"
#define OOM_ROWS 24
#define OOM_ARM_LIMIT 4096

static void oom_fixture(const char *slot, char ledger[BT_MUT_PATH_MAX],
                        char out_path[BT_MUT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/build/test-tmp/mutate-oom/%s", root, slot);
    bt_test_mkdirs(dir);
    snprintf(ledger, BT_MUT_PATH_MAX, "%s/ledger.tsv", dir);
    snprintf(out_path, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    char body[16384];
    size_t used = (size_t)snprintf(body, sizeof body,
                                   "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\t"
                                   "test_exe\n");
    for (size_t i = 0; i < OOM_ROWS; i++) {
        used += (size_t)snprintf(body + used, sizeof body - used,
                                 "%016zu\t" FX_REL "\t%zu\t20\tNumericTweak\t0\t1\t%s\t1\t"
                                 "2026-10-29T10:00:00\tfx_test\n",
                                 7000 + i, 6 + i, i % 2 == 0 ? "killed" : "survived");
    }
    BT_CHECK_EQ(bt_mut_write_file(ledger, body, used), 0);
}

static int armed_report(const char *ledger, const char *root, const char *out_path, size_t ok) {
    bt_alloc_fail_arm(ok);
    int rc = bt_mut_report(ledger, root, out_path);
    bt_alloc_fail_disarm();
    remove(out_path);
    return rc;
}

static unsigned long long oom_counter(const bt_json_value *root, const char *name) {
    const bt_json_value *value = bt_json_get(root, name);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return (unsigned long long)-1;
    }
    return (unsigned long long)value->as.number;
}

BT_TEST(mutate_oom, report_writes_the_full_ledger_when_unarmed) {
    char ledger[BT_MUT_PATH_MAX];
    char out_path[BT_MUT_PATH_MAX];
    oom_fixture("clean", ledger, out_path);
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    BT_CHECK_EQ(bt_mut_report(ledger, root, out_path), 0);
    char *text = NULL;
    size_t len = 0;
    BT_CHECK_EQ(bt_mut_read_file(out_path, &text, &len), 0);
    bt_json_value *report = NULL;
    bt_json_error error;
    int rc = bt_json_parse(text, len, &report, &error);
    free(text);
    BT_CHECK_EQ(rc, 0);
    if (report != NULL) {
        BT_CHECK_EQ(oom_counter(report, "total_mutants"), OOM_ROWS);
        BT_CHECK_EQ(oom_counter(report, "caught"), OOM_ROWS / 2);
        BT_CHECK_EQ(oom_counter(report, "missed"), OOM_ROWS / 2);
        bt_json_free(report);
    }
    remove(out_path);
}

BT_TEST(mutate_oom, every_allocation_failure_fails_the_report_closed) {
    char ledger[BT_MUT_PATH_MAX];
    char out_path[BT_MUT_PATH_MAX];
    oom_fixture("sweep", ledger, out_path);
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    size_t flip = OOM_ARM_LIMIT;
    for (size_t k = 0; k < OOM_ARM_LIMIT; k++) {
        if (armed_report(ledger, root, out_path, k) == 0) {
            flip = k;
            break;
        }
    }
    BT_CHECK(flip >= 3);
    BT_CHECK(flip < OOM_ARM_LIMIT);
    BT_CHECK_EQ(armed_report(ledger, root, out_path, flip - 1), 1);
    BT_CHECK_EQ(armed_report(ledger, root, out_path, flip - 2), 1);
    BT_CHECK_EQ(armed_report(ledger, root, out_path, flip), 0);
}
