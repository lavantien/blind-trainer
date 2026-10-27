#include "data/seedpack/mod.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_seed_error_string(const bt_seed_error *err, char *out, size_t out_len) {
    BT_TRACE_ENTER("seedpack.error_string");
    switch (err->kind) {
    case BT_SEED_ERR_REGISTRY_JSON:
        snprintf(out, out_len, "registry parse: %s", err->json_error);
        break;
    case BT_SEED_ERR_REGISTRY_RULE:
        snprintf(out, out_len, "registry rule: %s", err->json_error);
        break;
    case BT_SEED_ERR_IO:
        snprintf(out, out_len, "read %s: io failed", err->path);
        break;
    case BT_SEED_ERR_BAD_HEADER:
        snprintf(out, out_len,
                 "pack %s: header must be case_key/alg/front_json/source_url/source_at", err->file);
        break;
    case BT_SEED_ERR_RAGGED_ROW:
        snprintf(out, out_len, "pack %s line %zu: expected 5 columns, found %zu", err->file,
                 err->line, err->found);
        break;
    case BT_SEED_ERR_EMPTY_FIELD:
        snprintf(out, out_len, "pack %s line %zu: %s column is empty", err->file, err->line,
                 err->column);
        break;
    case BT_SEED_ERR_BAD_FIELD:
        snprintf(out, out_len, "pack %s row %s: bad %s", err->file, err->case_key, err->field);
        break;
    case BT_SEED_ERR_DUPLICATE_KEY:
        snprintf(out, out_len, "pack %s: duplicate case_key %s", err->file, err->case_key);
        break;
    case BT_SEED_ERR_BAD_FRONT_JSON:
        snprintf(out, out_len, "pack %s row %s: front_json is not json: %s", err->file,
                 err->case_key, err->json_error);
        break;
    case BT_SEED_ERR_FRONT_JSON_SHAPE:
        snprintf(out, out_len, "pack %s row %s: front_json is not an object", err->file,
                 err->case_key);
        break;
    case BT_SEED_ERR_NOTE_COLLISION:
        snprintf(out, out_len, "pack %s row %s: front_json already holds loader_notes", err->file,
                 err->case_key);
        break;
    case BT_SEED_ERR_COUNT_DRIFT:
        snprintf(out, out_len, "pack %s: row count drift, expected %zu, found %zu", err->file,
                 err->expected, err->found);
        break;
    case BT_SEED_ERR_NORMALIZE:
        snprintf(out, out_len, "pack %s row %s: %s", err->file, err->case_key, err->normalize);
        break;
    case BT_SEED_ERR_DRIFT:
        snprintf(out, out_len, "seed drift in pack %s row %s: stored hash %s, pack hash %s",
                 err->file, err->case_key, err->stored, err->packed);
        break;
    case BT_SEED_ERR_STALE:
        snprintf(out, out_len, "seed row %s no longer exists in pack %s", err->case_key, err->file);
        break;
    case BT_SEED_ERR_DB: {
        char db_text[512];
        bt_db_error_string(&err->db, db_text, sizeof db_text);
        snprintf(out, out_len, "%s", db_text);
        break;
    }
    }
    BT_TRACE_EXIT("seedpack.error_string");
}

bool bt_nspec_eq(const bt_nspec *a, const bt_nspec *b) {
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == BT_NSPEC_FIXED) {
        return a->fixed == b->fixed;
    }
    if (a->kind == BT_NSPEC_SIZES) {
        if (a->size_count != b->size_count) {
            return false;
        }
        for (size_t i = 0; i < a->size_count; i++) {
            if (a->sizes[i] != b->sizes[i]) {
                return false;
            }
        }
    }
    return true;
}

void bt_seed_rows_init(bt_seed_rows *rows) {
    rows->items = NULL;
    rows->count = 0;
}

void bt_seed_rows_free(bt_seed_rows *rows) {
    BT_TRACE_ENTER("seedpack.rows_free");
    for (size_t i = 0; i < rows->count; i++) {
        free(rows->items[i].alg);
        free(rows->items[i].front_json);
        free(rows->items[i].source_url);
        free(rows->items[i].source_at);
    }
    free(rows->items);
    rows->items = NULL;
    rows->count = 0;
    BT_TRACE_EXIT("seedpack.rows_free");
}
