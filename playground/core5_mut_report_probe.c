#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "mutate/mutate.h"

#define FX_REL "tests/unit/tools/fixtures/fx_target.c"
#define PROBE_ROWS 24

static void write_ledger(const char *path) {
    char body[16384];
    size_t used = (size_t)snprintf(body, sizeof body,
                                   "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\t"
                                   "test_exe\n");
    for (size_t i = 0; i < PROBE_ROWS; i++) {
        used += (size_t)snprintf(body + used, sizeof body - used,
                                 "%016zu\t" FX_REL "\t%zu\t20\tNumericTweak\t0\t1\t%s\t1\t"
                                 "2026-10-29T10:00:00\tfx_test\n",
                                 7000 + i, 6 + i, i % 2 == 0 ? "killed" : "survived");
    }
    bt_mut_write_file(path, body, used);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: probe <repo_root> <scratch_dir>\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    bt_mut_mkdirs(argv[2]);
    char ledger[BT_MUT_PATH_MAX];
    char out_path[BT_MUT_PATH_MAX];
    snprintf(ledger, sizeof ledger, "%s/ledger.tsv", argv[2]);
    snprintf(out_path, sizeof out_path, "%s/outcomes.json", argv[2]);
    write_ledger(ledger);
    for (size_t k = 0; k <= 2500; k++) {
        bt_alloc_fail_arm(k);
        int rc = bt_mut_report(ledger, argv[1], out_path);
        size_t failures = bt_alloc_fail_count();
        bt_alloc_fail_disarm();
        remove(out_path);
        printf("arm %zu rc=%d failures=%zu\n", k, rc, failures);
        if (rc == 0) {
            break;
        }
    }
    int rc = bt_mut_report(ledger, argv[1], out_path);
    char *text = NULL;
    size_t len = 0;
    if (bt_mut_read_file(out_path, &text, &len) == 0) {
        printf("clean rc=%d serialized=%zu\n", rc, len);
        free(text);
    } else {
        printf("clean rc=%d serialized=unreadable\n", rc);
    }
    return 0;
}
