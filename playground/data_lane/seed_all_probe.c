#include "common/bt_harness.h"
#include "data/db.h"
#include "data/repo_seeds.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

static const char *HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at";

static const char *seed_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-seedall-probe/%s-%d", base ? base : ".", tag, _getpid());
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void write_pack(const char *root, const char *file, const char *key) {
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", root, file);
    char text[512];
    snprintf(text, sizeof text, "%s\n%s\tR U R'\t{}\thttps://x\t2026-01-01\n", HEADER, key);
    tu_write_file(path, text);
}

int main(void) {
    const char *root = seed_dir("two");
    write_pack(root, "a.tsv", "A");
    printf("pack a written\n");
    fflush(stdout);
    write_pack(root, "b.tsv", "B");
    printf("pack b written\n");
    fflush(stdout);
    char reg_path[BT_TU_PATH_MAX];
    snprintf(reg_path, sizeof reg_path, "%s/registry.json", root);
    tu_write_file(reg_path,
                  "[{\"file\":\"a.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                  "\"expected_rows\":1},"
                  "{\"file\":\"b.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                  "\"expected_rows\":1}]");
    printf("registry written\n");
    fflush(stdout);
    bt_db db;
    bt_db_error db_err;
    if (bt_db_open_memory(&db, &db_err) != 0) {
        printf("open failed\n");
        return 1;
    }
    printf("db open\n");
    fflush(stdout);
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    int rc = bt_seed_all(&db, root, &reports, &count, &err);
    printf("seed_all rc=%d count=%zu\n", rc, count);
    fflush(stdout);
    if (rc != 0) {
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        printf("err: %s\n", text);
        return 1;
    }
    printf("reports %zu %zu\n", reports[0].inserted, reports[1].inserted);
    bt_seed_reports_free(reports);
    bt_db_close(&db);
    tu_remove_tree(root);
    printf("done\n");
    return 0;
}
