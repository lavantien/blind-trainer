#include "data/seedpack/registry.h"

#include "common/bt_trace.h"
#include "common/json.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void registry_json_error(bt_seed_error *err, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->json_error, sizeof err->json_error, fmt, ap);
    va_end(ap);
    err->kind = BT_SEED_ERR_REGISTRY_JSON;
}

static void registry_rule(bt_seed_error *err, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->json_error, sizeof err->json_error, fmt, ap);
    va_end(ap);
    err->kind = BT_SEED_ERR_REGISTRY_RULE;
}

static bool valid_size(long long size) {
    return (size >= 2 && size <= 7) != 0;
}

static int read_n(const bt_json_value *node, const char *file, bt_nspec *n, bt_seed_error *err) {
    if (node->type == BT_JSON_NULL) {
        n->kind = BT_NSPEC_UNSET;
        return 0;
    }
    if (node->type == BT_JSON_NUMBER && bt_json_is_integer(node)) {
        long long size = bt_json_integer_or(node, 0);
        if (!valid_size(size)) {
            registry_rule(err, "entry for '%s' holds n %lld outside 2..=7", file, size);
            return -1;
        }
        n->kind = BT_NSPEC_FIXED;
        n->fixed = size;
        return 0;
    }
    if (node->type == BT_JSON_ARRAY) {
        if (node->as.array.count == 0 || node->as.array.count > BT_SEED_NSPEC_SIZES_MAX) {
            registry_rule(err, "entry for '%s' holds an empty or out-of-range n list", file);
            return -1;
        }
        for (size_t i = 0; i < node->as.array.count; i++) {
            const bt_json_value *item = bt_json_at(node, i);
            if (item == NULL || item->type != BT_JSON_NUMBER || !bt_json_is_integer(item) ||
                !valid_size(bt_json_integer_or(item, 0))) {
                registry_rule(err, "entry for '%s' holds an empty or out-of-range n list", file);
                return -1;
            }
        }
        n->kind = BT_NSPEC_SIZES;
        n->size_count = node->as.array.count;
        for (size_t i = 0; i < node->as.array.count; i++) {
            n->sizes[i] = bt_json_integer_or(bt_json_at(node, i), 0);
        }
        return 0;
    }
    registry_json_error(err, "n value is not an integer or a list of integers");
    return -1;
}

int bt_seedpack_parse_registry(const char *text, bt_pack_entry **out_entries, size_t *out_count,
                               bt_seed_error *err) {
    BT_TRACE_ENTER("seedpack.parse_registry");
    *out_entries = NULL;
    *out_count = 0;
    bt_json_error jerr;
    bt_json_value *root = NULL;
    if (bt_json_parse(text, strlen(text), &root, &jerr) != 0) {
        registry_json_error(err, "line %zu column %zu: %s", jerr.line, jerr.column, jerr.message);
        BT_TRACE_FAIL("seedpack.parse_registry", "json parse failed");
        return -1;
    }
    if (root->type != BT_JSON_ARRAY) {
        bt_json_free(root);
        registry_json_error(err, "registry must be a json array");
        BT_TRACE_FAIL("seedpack.parse_registry", "registry is not an array");
        return -1;
    }
    bt_pack_entry *entries = calloc(root->as.array.count, sizeof *entries);
    if (entries == NULL) {
        bt_json_free(root);
        registry_json_error(err, "out of memory");
        BT_TRACE_FAIL("seedpack.parse_registry", "alloc failed");
        return -1;
    }
    for (size_t i = 0; i < root->as.array.count; i++) {
        const bt_json_value *item = bt_json_at(root, i);
        if (item == NULL || item->type != BT_JSON_OBJECT) {
            free(entries);
            bt_json_free(root);
            registry_json_error(err, "entry %zu is not an object", i);
            BT_TRACE_FAIL("seedpack.parse_registry", "entry %zu is not an object", i);
            return -1;
        }
        bool have_file = false;
        bool have_event = false;
        bool have_method = false;
        bool have_stage = false;
        bool have_rows = false;
        bool have_n = false;
        bt_nspec n = {BT_NSPEC_UNSET, 0, 0, {0}};
        const char *file = NULL;
        const char *event = NULL;
        const char *method = NULL;
        const char *stage = NULL;
        long long expected_rows = 0;
        for (size_t k = 0; k < item->as.object.count; k++) {
            const char *key = item->as.object.keys[k];
            const bt_json_value *node = item->as.object.values[k];
            if (strcmp(key, "file") == 0) {
                if (node->type != BT_JSON_STRING) {
                    free(entries);
                    bt_json_free(root);
                    registry_json_error(err, "entry %zu: file is not a string", i);
                    BT_TRACE_FAIL("seedpack.parse_registry", "file is not a string at %zu", i);
                    return -1;
                }
                file = node->as.string;
                have_file = true;
            } else if (strcmp(key, "event") == 0) {
                if (node->type != BT_JSON_STRING) {
                    free(entries);
                    bt_json_free(root);
                    registry_json_error(err, "entry %zu: event is not a string", i);
                    BT_TRACE_FAIL("seedpack.parse_registry", "event is not a string at %zu", i);
                    return -1;
                }
                event = node->as.string;
                have_event = true;
            } else if (strcmp(key, "method") == 0) {
                if (node->type != BT_JSON_STRING) {
                    free(entries);
                    bt_json_free(root);
                    registry_json_error(err, "entry %zu: method is not a string", i);
                    BT_TRACE_FAIL("seedpack.parse_registry", "method is not a string at %zu", i);
                    return -1;
                }
                method = node->as.string;
                have_method = true;
            } else if (strcmp(key, "stage") == 0) {
                if (node->type != BT_JSON_STRING) {
                    free(entries);
                    bt_json_free(root);
                    registry_json_error(err, "entry %zu: stage is not a string", i);
                    BT_TRACE_FAIL("seedpack.parse_registry", "stage is not a string at %zu", i);
                    return -1;
                }
                stage = node->as.string;
                have_stage = true;
            } else if (strcmp(key, "n") == 0) {
                if (read_n(node, file == NULL ? "" : file, &n, err) != 0) {
                    free(entries);
                    bt_json_free(root);
                    BT_TRACE_FAIL("seedpack.parse_registry", "bad n at entry %zu", i);
                    return -1;
                }
                have_n = true;
            } else if (strcmp(key, "expected_rows") == 0) {
                if (node->type != BT_JSON_NUMBER || !bt_json_is_integer(node)) {
                    free(entries);
                    bt_json_free(root);
                    registry_json_error(err, "entry %zu: expected_rows is not an integer", i);
                    BT_TRACE_FAIL("seedpack.parse_registry", "expected_rows bad at %zu", i);
                    return -1;
                }
                expected_rows = bt_json_integer_or(node, 0);
                have_rows = true;
            } else {
                free(entries);
                bt_json_free(root);
                registry_json_error(err, "unknown field '%s'", key);
                BT_TRACE_FAIL("seedpack.parse_registry", "unknown field %s", key);
                return -1;
            }
        }
        (void)have_n;
        if (!have_file || !have_event || !have_method || !have_stage || !have_rows) {
            free(entries);
            bt_json_free(root);
            registry_json_error(err, "entry %zu misses a required field", i);
            BT_TRACE_FAIL("seedpack.parse_registry", "entry %zu misses a field", i);
            return -1;
        }
        if (file[0] == '\0' || strchr(file, '/') != NULL || strchr(file, '\\') != NULL ||
            strcmp(file, "..") == 0) {
            free(entries);
            bt_json_free(root);
            registry_rule(err, "file name '%s' is not a bare name", file);
            BT_TRACE_FAIL("seedpack.parse_registry", "file %s is not bare", file);
            return -1;
        }
        if (event[0] == '\0' || method[0] == '\0' || stage[0] == '\0') {
            free(entries);
            bt_json_free(root);
            registry_rule(err, "entry for '%s' holds an empty event, method, or stage", file);
            BT_TRACE_FAIL("seedpack.parse_registry", "empty event, method, or stage for %s", file);
            return -1;
        }
        if (expected_rows < 0) {
            free(entries);
            bt_json_free(root);
            registry_json_error(err, "entry %zu: expected_rows is negative", i);
            BT_TRACE_FAIL("seedpack.parse_registry", "negative expected rows at %zu", i);
            return -1;
        }
        if (expected_rows == 0) {
            free(entries);
            bt_json_free(root);
            registry_rule(err, "entry for '%s' declares zero expected rows", file);
            BT_TRACE_FAIL("seedpack.parse_registry", "zero expected rows for %s", file);
            return -1;
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(entries[j].event, event) == 0 && strcmp(entries[j].method, method) == 0 &&
                strcmp(entries[j].stage, stage) == 0) {
                free(entries);
                bt_json_free(root);
                registry_rule(err, "duplicate registry key %s / %s / %s", event, method, stage);
                BT_TRACE_FAIL("seedpack.parse_registry", "duplicate key %s/%s/%s", event, method,
                              stage);
                return -1;
            }
        }
        bt_pack_entry *entry = &entries[i];
        snprintf(entry->file, sizeof entry->file, "%s", file);
        snprintf(entry->event, sizeof entry->event, "%s", event);
        snprintf(entry->method, sizeof entry->method, "%s", method);
        snprintf(entry->stage, sizeof entry->stage, "%s", stage);
        entry->n = n;
        entry->expected_rows = (size_t)expected_rows;
    }
    size_t count = root->as.array.count;
    bt_json_free(root);
    *out_entries = entries;
    *out_count = count;
    BT_TRACE_EXIT("seedpack.parse_registry");
    return 0;
}

void bt_seedpack_free_entries(bt_pack_entry *entries) {
    free(entries);
}

int bt_seedpack_load_registry(const char *packs_root, bt_pack_entry **out_entries,
                              size_t *out_count, bt_seed_error *err) {
    BT_TRACE_ENTER("seedpack.load_registry");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/registry.json", packs_root);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        err->kind = BT_SEED_ERR_IO;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("seedpack.load_registry", "read %s failed", path);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        err->kind = BT_SEED_ERR_IO;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("seedpack.load_registry", "alloc failed for %s", path);
        return -1;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    int rc = bt_seedpack_parse_registry(text, out_entries, out_count, err);
    free(text);
    if (rc != 0) {
        BT_TRACE_FAIL("seedpack.load_registry", "parse failed for %s", path);
        return -1;
    }
    BT_TRACE_EXIT("seedpack.load_registry");
    return 0;
}
