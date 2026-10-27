#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "common/when.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "data/paths.h"
#include "gendata/audit.h"
#include "gendata/build.h"
#include "gendata/lib.h"
#include "gendata/model.h"
#include "gendata/out.h"
#include "gendata/scoring.h"
#include "gendata/synth.h"

typedef struct {
    char **items;
    size_t count;
} bt_gd_errors;

static int errors_push(bt_gd_errors *errors, const char *text) {
    char **grown = realloc(errors->items, (errors->count + 1) * sizeof(char *));
    if (grown == NULL) {
        return -1;
    }
    errors->items = grown;
    errors->items[errors->count++] = _strdup(text);
    return 0;
}

static void errors_free(bt_gd_errors *errors) {
    for (size_t i = 0; i < errors->count; i++) {
        free(errors->items[i]);
    }
    free(errors->items);
    errors->items = NULL;
    errors->count = 0;
}

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int run(bt_gd_errors *errors) {
    BT_TRACE_ENTER("gd.main.run");
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        errors_push(errors, "config load failed");
        BT_TRACE_FAIL("gd.main.run", "config load failed");
        return -1;
    }
    char root[BT_PATH_MAX];
    bt_paths_error perr;
    if (bt_data_vendor_root_from(getenv("BT_REF_GROUND"), config, root, &perr) != 0) {
        char text[256];
        bt_paths_error_string(&perr, text, sizeof text);
        errors_push(errors, text);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", text);
        return -1;
    }
    bt_corpus_digest digest;
    char err[BT_GD_ERR_MAX];
    if (bt_digest_corpus(root, &digest, err, sizeof err) != 0) {
        errors_push(errors, err);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", err);
        return -1;
    }
    bt_gd_synth_limits limits;
    if (bt_gd_limits_from_config(config, &limits, err, sizeof err) != 0) {
        errors_push(errors, err);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", err);
        return -1;
    }
    bt_gd_scoring_cfg scoring;
    if (bt_gd_scoring_from_config(config, &scoring, err, sizeof err) != 0) {
        errors_push(errors, err);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", err);
        return -1;
    }
    char params_sha256[65];
    if (bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, params_sha256, err, sizeof err) !=
        0) {
        errors_push(errors, err);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", err);
        return -1;
    }
    char out_dir[BT_PATH_MAX];
    if (bt_gd_data_vendor_path(config, out_dir, err, sizeof err) != 0) {
        errors_push(errors, err);
        bt_json_free(config);
        BT_TRACE_FAIL("gd.main.run", err);
        return -1;
    }
    bt_json_free(config);
    bool has_existing = false;
    bt_gd_manifest existing;
    char manifest_path[BT_PATH_MAX];
    char cases_path[BT_PATH_MAX];
    char specials_path[BT_PATH_MAX];
    snprintf(manifest_path, sizeof manifest_path, "%s/manifest.json", out_dir);
    snprintf(cases_path, sizeof cases_path, "%s/cases.json", out_dir);
    snprintf(specials_path, sizeof specials_path, "%s/specials.json", out_dir);
    bt_json_value *existing_value = NULL;
    if (bt_data_read_json(manifest_path, &existing_value) == BT_DATA_READ_OK) {
        char parse_err[96];
        if (bt_gd_manifest_from_json(existing_value, &existing, parse_err, sizeof parse_err) == 0) {
            has_existing = true;
        }
    }
    bool outputs_present = false;
    if (file_exists(cases_path) && file_exists(specials_path)) {
        outputs_present = true;
    }
    if (outputs_present && has_existing &&
        !bt_gd_regen_needed(&existing, has_existing, params_sha256, digest.sha256)) {
        puts("gendata: skip regeneration, synth params and corpus digest unchanged");
        if (has_existing) {
            bt_gd_manifest_free(&existing);
        }
        if (existing_value != NULL) {
            bt_json_free(existing_value);
        }
        BT_TRACE_EXIT("gd.main.run");
        return 0;
    }
    if (has_existing) {
        bt_gd_manifest_free(&existing);
    }
    if (existing_value != NULL) {
        bt_json_free(existing_value);
    }
    bt_corpus corpus;
    bt_corpus_error cerr;
    if (bt_corpus_load(root, &corpus, &cerr) != 0) {
        char text[BT_PATH_MAX + 96];
        bt_corpus_error_string(&cerr, text, sizeof text);
        errors_push(errors, text);
        BT_TRACE_FAIL("gd.main.run", text);
        return -1;
    }
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    char **audit_offenders = NULL;
    size_t audit_count = 0;
    bt_gd_audit(&built, &corpus, digest.rows, &audit_offenders, &audit_count);
    if (audit_count != 0) {
        for (size_t i = 0; i < audit_count; i++) {
            errors_push(errors, audit_offenders[i]);
        }
        bt_gd_string_list_free(audit_offenders, audit_count);
        bt_gd_built_free(&built);
        bt_corpus_free(&corpus);
        BT_TRACE_FAIL("gd.main.run", "audit offenders present");
        return -1;
    }
    bt_gd_string_list_free(audit_offenders, audit_count);
    size_t edge_added = 0;
    size_t corner_added = 0;
    bt_gd_synthesize(&built.cases, &limits, &scoring, &edge_added, &corner_added);
    bt_json_value *cases_value = bt_gd_cases_file_to_json(&built.cases);
    bt_json_value *specials_value = bt_gd_specials_file_to_json(&built.specials);
    bt_strbuf cases_text;
    bt_strbuf specials_text;
    bt_strbuf_init(&cases_text, 0);
    bt_strbuf_init(&specials_text, 0);
    bt_gd_json_pretty(cases_value, &cases_text);
    bt_gd_json_pretty(specials_value, &specials_text);
    char cases_sha[65];
    char specials_sha[65];
    bt_digest_sha256_hex(cases_text.data, cases_text.len, cases_sha);
    bt_digest_sha256_hex(specials_text.data, specials_text.len, specials_sha);
    char stamp[BT_WHEN_ISO8601_LEN];
    bt_when_now(stamp, sizeof stamp);
    bt_gd_manifest record;
    bt_gd_manifest_make(&built, &digest, cases_sha, specials_sha, params_sha256, stamp, &record);
    bt_json_value *manifest_value = bt_gd_manifest_to_json(&record);
    bt_strbuf manifest_text;
    bt_strbuf_init(&manifest_text, 0);
    bt_gd_json_pretty(manifest_value, &manifest_text);
    int failed = 0;
    if (bt_gd_write_atomic(cases_path, cases_text.data, cases_text.len, err, sizeof err) != 0) {
        failed = -1;
    }
    if (failed == 0 && bt_gd_write_atomic(specials_path, specials_text.data, specials_text.len, err,
                                          sizeof err) != 0) {
        failed = -1;
    }
    if (failed == 0 && bt_gd_write_atomic(manifest_path, manifest_text.data, manifest_text.len, err,
                                          sizeof err) != 0) {
        failed = -1;
    }
    if (failed != 0) {
        errors_push(errors, err);
    }
    if (failed == 0) {
        printf("gendata: %zu cases (%zu covered, %zu generated: %zu edge, %zu corner), "
               "%zu specials, manifest at %s\n",
               record.cases.entries, record.cases.edge.covered + record.cases.corner.covered,
               edge_added + corner_added, edge_added, corner_added, record.specials.entries,
               manifest_path);
    }
    bt_strbuf_free(&manifest_text);
    bt_json_free(manifest_value);
    bt_gd_manifest_free(&record);
    bt_strbuf_free(&specials_text);
    bt_strbuf_free(&cases_text);
    bt_json_free(specials_value);
    bt_json_free(cases_value);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    if (failed != 0) {
        BT_TRACE_FAIL("gd.main.run", "output write failed");
    } else {
        BT_TRACE_EXIT("gd.main.run");
    }
    return failed;
}

int main(void) {
    bt_gd_errors errors = {0};
    if (run(&errors) != 0) {
        for (size_t i = 0; i < errors.count; i++) {
            fprintf(stderr, "gendata: %s\n", errors.items[i]);
        }
        errors_free(&errors);
        return 1;
    }
    errors_free(&errors);
    return 0;
}
