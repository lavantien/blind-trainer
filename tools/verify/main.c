#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"
#include "common/when.h"
#include "data/digest.h"
#include "data/paths.h"
#include "verify/lib.h"
#include "verify/packs_report.h"
#include "verify/report.h"

enum { VERIFY_MAIN_ERR_MAX = 512 };

static int path_is_absolute(const char *path) {
    return path[0] == '/' || path[0] == '\\' || (path[0] != '\0' && path[1] == ':');
}

static int artifacts_verify_dir(char out[BT_PATH_MAX], char *err, size_t err_len) {
    BT_TRACE_ENTER("verify.main.artifacts_dir");
    const char *raw = getenv("BT_ARTIFACTS");
    if (raw != NULL && raw[0] != '\0') {
        if (path_is_absolute(raw)) {
            snprintf(out, BT_PATH_MAX, "%s/verify", raw);
        } else {
            char root[BT_PATH_MAX];
            bt_paths_workspace_root(root);
            snprintf(out, BT_PATH_MAX, "%s/%s/verify", root, raw);
        }
        BT_TRACE_EXIT("verify.main.artifacts_dir");
        return 0;
    }
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        char path[BT_PATH_MAX];
        bt_data_config_path(path);
        snprintf(err, err_len, "read %s: config unreadable", path);
        BT_TRACE_FAIL("verify.main.artifacts_dir", err);
        return -1;
    }
    const bt_json_value *node = bt_config_value(config, "paths.artifacts");
    if (node == NULL || node->type != BT_JSON_STRING) {
        bt_json_free(config);
        snprintf(err, err_len, "%s", "config section missing: paths.artifacts");
        BT_TRACE_FAIL("verify.main.artifacts_dir", err);
        return -1;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    if (path_is_absolute(node->as.string)) {
        snprintf(out, BT_PATH_MAX, "%s/verify", node->as.string);
    } else {
        snprintf(out, BT_PATH_MAX, "%s/%s/verify", root, node->as.string);
    }
    bt_json_free(config);
    BT_TRACE_EXIT("verify.main.artifacts_dir");
    return 0;
}

static int mkdirs_for(const char *dir, char *err, size_t err_len) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", dir);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            if (_mkdir(copy) != 0 && errno != EEXIST) {
                snprintf(err, err_len, "write %s: mkdir failed %d", copy, errno);
                return -1;
            }
            *cursor = '/';
        }
    }
    if (_mkdir(copy) != 0 && errno != EEXIST) {
        snprintf(err, err_len, "write %s: mkdir failed %d", copy, errno);
        return -1;
    }
    return 0;
}

static int write_report_file(const char *path, const char *data, size_t len, char *err,
                             size_t err_len) {
    BT_TRACE_ENTER("verify.main.write_report_file");
    char name[BT_PATH_MAX];
    snprintf(name, sizeof name, "%s", path);
    char *cut = strrchr(name, '/');
    char *backslash = strrchr(name, '\\');
    if (backslash > cut) {
        cut = backslash;
    }
    if (cut != NULL) {
        char dir[BT_PATH_MAX];
        snprintf(dir, sizeof dir, "%.*s", (int)(cut - name), name);
        if (mkdirs_for(dir, err, err_len) != 0) {
            BT_TRACE_FAIL("verify.main.write_report_file", err);
            BT_TRACE_EXIT("verify.main.write_report_file");
            return -1;
        }
    }
    if (bt_paths_write_atomic(path, data, len) != 0) {
        snprintf(err, err_len, "write %s: write failed", path);
        BT_TRACE_FAIL("verify.main.write_report_file", err);
        BT_TRACE_EXIT("verify.main.write_report_file");
        return -1;
    }
    BT_TRACE_EXIT("verify.main.write_report_file");
    return 0;
}

static int run(verify_strlist *offenders) {
    BT_TRACE_ENTER("verify.main.run");
    char err[VERIFY_MAIN_ERR_MAX];
    bt_json_value *config = bt_data_load_config();
    bt_corpus corpus = {0};
    verify_report report;
    memset(&report, 0, sizeof report);
    verify_pack_audit pack_audit;
    memset(&pack_audit, 0, sizeof pack_audit);
    verify_sim_map sim;
    verify_sim_map_init(&sim);
    verify_strlist_init(offenders);
    if (config == NULL) {
        char path[BT_PATH_MAX];
        bt_data_config_path(path);
        verify_strlist_pushf(offenders, "read %s: config unreadable", path);
        goto fail;
    }
    char root[BT_PATH_MAX];
    bt_paths_error paths_err;
    if (bt_data_vendor_root_from(getenv("BT_REF_GROUND"), config, root, &paths_err) != 0) {
        bt_paths_error_string(&paths_err, err, sizeof err);
        verify_strlist_push(offenders, err);
        goto fail;
    }
    bt_corpus_error corpus_err;
    if (bt_corpus_load(root, &corpus, &corpus_err) != 0) {
        bt_corpus_error_string(&corpus_err, err, sizeof err);
        verify_strlist_push(offenders, err);
        goto fail;
    }
    verify_sim_map_init(&sim);
    if (verify_load_simcheck(root, &sim, err, sizeof err) != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    bt_corpus_digest digest;
    if (bt_digest_corpus(root, &digest, err, sizeof err) != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    char stamp[BT_WHEN_ISO8601_LEN];
    bt_when_now(stamp, sizeof stamp);
    verify_build_report(&corpus, &sim, stamp, digest.sha256, &report);
    verify_strlist report_offenders;
    verify_report_audit(&report, sim.count, &report_offenders);
    for (size_t at = 0; at < report_offenders.count; at++) {
        verify_strlist_push(offenders, report_offenders.items[at]);
    }
    verify_strlist_free(&report_offenders);
    char dir[BT_PATH_MAX];
    if (artifacts_verify_dir(dir, err, sizeof err) != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    char packs_root[BT_PATH_MAX];
    if (bt_data_packs_root_from(getenv("BT_REF_GROUND"), config, packs_root, &paths_err) != 0) {
        bt_paths_error_string(&paths_err, err, sizeof err);
        verify_strlist_push(offenders, err);
        goto fail;
    }
    if (verify_audit_packs(packs_root, &pack_audit) != 0) {
        verify_strlist_push(offenders, "pack registry load failed");
        goto fail;
    }
    char packs_md_path[BT_PATH_MAX];
    snprintf(packs_md_path, sizeof packs_md_path, "%s/packs.md", dir);
    bt_strbuf packs_md;
    bt_strbuf_init(&packs_md, 4096);
    verify_render_packs_markdown(&pack_audit, &packs_md);
    int write_failed =
        write_report_file(packs_md_path, packs_md.data, packs_md.len, err, sizeof err);
    bt_strbuf_free(&packs_md);
    if (write_failed != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    size_t pack_rows = 0;
    size_t pack_clean = 0;
    for (size_t at = 0; at < pack_audit.section_count; at++) {
        pack_rows += pack_audit.sections[at].rows;
        pack_clean += pack_audit.sections[at].passed + pack_audit.sections[at].ruled;
    }
    for (size_t at = 0; at < pack_audit.offenders.count; at++) {
        verify_strlist_push(offenders, pack_audit.offenders.items[at]);
    }
    if (offenders->count > 0) {
        goto fail;
    }
    bt_json_value *json = NULL;
    if (verify_report_to_json(&report, &json) != 0) {
        verify_strlist_push(offenders, "serialize report.json failed");
        goto fail;
    }
    bt_strbuf json_text;
    bt_strbuf_init(&json_text, 4096);
    int serialize_failed = bt_json_serialize(json, &json_text);
    bt_json_free(json);
    if (serialize_failed != 0) {
        bt_strbuf_free(&json_text);
        verify_strlist_push(offenders, "serialize report.json failed");
        goto fail;
    }
    char report_json_path[BT_PATH_MAX];
    snprintf(report_json_path, sizeof report_json_path, "%s/report.json", dir);
    int json_write_failed =
        write_report_file(report_json_path, json_text.data, json_text.len, err, sizeof err);
    bt_strbuf_free(&json_text);
    if (json_write_failed != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    char report_md_path[BT_PATH_MAX];
    snprintf(report_md_path, sizeof report_md_path, "%s/report.md", dir);
    bt_strbuf md;
    bt_strbuf_init(&md, 4096);
    verify_render_markdown(&report, &md);
    int md_write_failed = write_report_file(report_md_path, md.data, md.len, err, sizeof err);
    bt_strbuf_free(&md);
    if (md_write_failed != 0) {
        verify_strlist_push(offenders, err);
        goto fail;
    }
    printf("verify: %zu/%zu rows pass, report at %s\n", report.row_count - report.failure_count,
           report.row_count, report_md_path);
    printf("packs: %zu/%zu rows pass or ruled, report at %s\n", pack_clean, pack_rows,
           packs_md_path);
    verify_pack_audit_free(&pack_audit);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
    bt_json_free(config);
    BT_TRACE_EXIT("verify.main.run");
    return 0;
fail:
    verify_pack_audit_free(&pack_audit);
    verify_report_free(&report);
    verify_sim_map_free(&sim);
    bt_corpus_free(&corpus);
    bt_json_free(config);
    BT_TRACE_FAIL("verify.main.run", "%zu offenders", offenders->count);
    return 1;
}

int main(void) {
    verify_strlist offenders;
    if (run(&offenders) != 0) {
        for (size_t at = 0; at < offenders.count; at++) {
            fprintf(stderr, "verify: %s\n", offenders.items[at]);
        }
        verify_strlist_free(&offenders);
        return 1;
    }
    verify_strlist_free(&offenders);
    return 0;
}
