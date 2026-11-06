#include "gendata/model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "gendata/lib.h"

void bt_gd_string_list_free(char **items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static void string_vec_free(char **items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static bool string_vec_eq(char *const *a, size_t a_count, char *const *b, size_t b_count) {
    if (a_count != b_count) {
        return false;
    }
    for (size_t i = 0; i < a_count; i++) {
        if (strcmp(a[i], b[i]) != 0) {
            return false;
        }
    }
    return true;
}

static int push_string(bt_json_value *array, const char *text) {
    return bt_json_array_push(array, bt_json_new_string(text));
}

static int set_string(bt_json_value *object, const char *key, const char *text) {
    return bt_json_object_set(object, key, bt_json_new_string(text));
}

static int set_integer(bt_json_value *object, const char *key, long long value) {
    return bt_json_object_set(object, key, bt_json_new_integer(value));
}

static int set_member(bt_json_value *object, const char *key, bt_json_value *value) {
    if (value == NULL || bt_json_object_set(object, key, value) != 0) {
        bt_json_free(value);
        return -1;
    }
    return 0;
}

static int push_member(bt_json_value *array, bt_json_value *item) {
    if (item == NULL || bt_json_array_push(array, item) != 0) {
        bt_json_free(item);
        return -1;
    }
    return 0;
}

static int get_string(const bt_json_value *object, const char *key, char **out, char *err,
                      size_t err_len) {
    const bt_json_value *node = bt_json_get(object, key);
    if (node == NULL || node->type != BT_JSON_STRING) {
        snprintf(err, err_len, "missing field `%s`", key);
        return -1;
    }
    *out = bt_gd_strdup(node->as.string);
    if (*out == NULL) {
        snprintf(err, err_len, "field `%s` duplication failed", key);
        return -1;
    }
    return 0;
}

static int get_integer(const bt_json_value *object, const char *key, size_t *out, char *err,
                       size_t err_len) {
    const bt_json_value *node = bt_json_get(object, key);
    if (node == NULL || node->type != BT_JSON_NUMBER || !bt_json_is_integer(node) ||
        node->integer < 0) {
        snprintf(err, err_len, "missing field `%s`", key);
        return -1;
    }
    *out = (size_t)node->integer;
    return 0;
}

static int get_u32(const bt_json_value *object, const char *key, uint32_t *out, char *err,
                   size_t err_len) {
    size_t wide = 0;
    if (get_integer(object, key, &wide, err, err_len) != 0) {
        return -1;
    }
    if (wide > UINT32_MAX) {
        snprintf(err, err_len, "field `%s` exceeds u32", key);
        return -1;
    }
    *out = (uint32_t)wide;
    return 0;
}

static int get_bool(const bt_json_value *object, const char *key, bool *out, char *err,
                    size_t err_len) {
    const bt_json_value *node = bt_json_get(object, key);
    if (node == NULL || node->type != BT_JSON_BOOL) {
        snprintf(err, err_len, "missing field `%s`", key);
        return -1;
    }
    *out = node->as.boolean != 0;
    return 0;
}

static bt_json_value *string_vec_to_json(char *const *items, size_t count) {
    bt_json_value *array = bt_json_new_array();
    if (array == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (push_string(array, items[i]) != 0) {
            bt_json_free(array);
            return NULL;
        }
    }
    return array;
}

static int string_vec_from_json(const bt_json_value *array, char ***out, size_t *count, char *err,
                                size_t err_len) {
    if (array == NULL || array->type != BT_JSON_ARRAY) {
        snprintf(err, err_len, "%s", "expected a string array");
        return -1;
    }
    *count = array->as.array.count;
    *out = bt_malloc(*count * sizeof(char *));
    if (*out == NULL && *count > 0) {
        *count = 0;
        snprintf(err, err_len, "%s", "allocation failed");
        return -1;
    }
    for (size_t i = 0; i < *count; i++) {
        const bt_json_value *node = bt_json_at(array, i);
        if (node == NULL || node->type != BT_JSON_STRING) {
            snprintf(err, err_len, "%s", "expected a string array");
            string_vec_free(*out, i);
            *out = NULL;
            *count = 0;
            return -1;
        }
        (*out)[i] = bt_gd_strdup(node->as.string);
        if ((*out)[i] == NULL) {
            snprintf(err, err_len, "%s", "allocation failed");
            string_vec_free(*out, i + 1);
            *out = NULL;
            *count = 0;
            return -1;
        }
    }
    return 0;
}

void bt_gd_vendor_alg_free(bt_gd_vendor_alg *alg) {
    free(alg->solver);
    free(alg->position);
    free(alg->alg);
    string_vec_free(alg->problems, alg->problem_count);
    string_vec_free(alg->notes, alg->note_count);
    free(alg->commutator);
    free(alg->recon_link);
    free(alg->youtube_link);
    memset(alg, 0, sizeof *alg);
}

bool bt_gd_vendor_alg_eq(const bt_gd_vendor_alg *a, const bt_gd_vendor_alg *b) {
    if (strcmp(a->solver, b->solver) != 0 || a->rowid != b->rowid ||
        strcmp(a->position, b->position) != 0 || strcmp(a->alg, b->alg) != 0 || a->qtm != b->qtm ||
        a->htm != b->htm || a->ok != b->ok ||
        !string_vec_eq(a->problems, a->problem_count, b->problems, b->problem_count) ||
        !string_vec_eq(a->notes, a->note_count, b->notes, b->note_count) ||
        strcmp(a->commutator, b->commutator) != 0 || strcmp(a->recon_link, b->recon_link) != 0) {
        return false;
    }
    return strcmp(a->youtube_link, b->youtube_link) == 0;
}

static bt_json_value *vendor_alg_core_to_json(const bt_gd_vendor_alg *alg) {
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        return NULL;
    }
    if (set_string(object, "solver", alg->solver) != 0 ||
        set_integer(object, "rowid", (long long)alg->rowid) != 0 ||
        set_string(object, "position", alg->position) != 0 ||
        set_string(object, "alg", alg->alg) != 0 || set_integer(object, "qtm", alg->qtm) != 0 ||
        set_integer(object, "htm", alg->htm) != 0 ||
        bt_json_object_set(object, "ok", bt_json_new_bool(alg->ok ? 1 : 0)) != 0) {
        bt_json_free(object);
        return NULL;
    }
    bt_json_value *problems = string_vec_to_json(alg->problems, alg->problem_count);
    bt_json_value *notes = string_vec_to_json(alg->notes, alg->note_count);
    if (problems == NULL || notes == NULL ||
        bt_json_object_set(object, "problems", problems) != 0) {
        bt_json_free(problems);
        bt_json_free(notes);
        bt_json_free(object);
        return NULL;
    }
    if (bt_json_object_set(object, "notes", notes) != 0) {
        bt_json_free(notes);
        bt_json_free(object);
        return NULL;
    }
    if (set_string(object, "commutator", alg->commutator) != 0 ||
        set_string(object, "recon_link", alg->recon_link) != 0 ||
        set_string(object, "youtube_link", alg->youtube_link) != 0) {
        bt_json_free(object);
        return NULL;
    }
    return object;
}

bt_json_value *bt_gd_vendor_alg_to_json(const bt_gd_vendor_alg *alg) {
    BT_TRACE_ENTER("gd.model.vendor_alg_to_json");
    bt_json_value *object = vendor_alg_core_to_json(alg);
    BT_TRACE_EXIT("gd.model.vendor_alg_to_json");
    return object;
}

static int vendor_alg_core_from_json(const bt_json_value *object, bt_gd_vendor_alg *out, char *err,
                                     size_t err_len) {
    memset(out, 0, sizeof *out);
    if (get_string(object, "solver", &out->solver, err, err_len) != 0 ||
        get_integer(object, "rowid", &out->rowid, err, err_len) != 0 ||
        get_string(object, "position", &out->position, err, err_len) != 0 ||
        get_string(object, "alg", &out->alg, err, err_len) != 0 ||
        get_u32(object, "qtm", &out->qtm, err, err_len) != 0 ||
        get_u32(object, "htm", &out->htm, err, err_len) != 0 ||
        get_bool(object, "ok", &out->ok, err, err_len) != 0 ||
        string_vec_from_json(bt_json_get(object, "problems"), &out->problems, &out->problem_count,
                             err, err_len) != 0 ||
        string_vec_from_json(bt_json_get(object, "notes"), &out->notes, &out->note_count, err,
                             err_len) != 0 ||
        get_string(object, "commutator", &out->commutator, err, err_len) != 0 ||
        get_string(object, "recon_link", &out->recon_link, err, err_len) != 0 ||
        get_string(object, "youtube_link", &out->youtube_link, err, err_len) != 0) {
        bt_gd_vendor_alg_free(out);
        return -1;
    }
    return 0;
}

int bt_gd_vendor_alg_from_json(const bt_json_value *value, bt_gd_vendor_alg *out, char *err,
                               size_t err_len) {
    BT_TRACE_ENTER("gd.model.vendor_alg_from_json");
    int rc = vendor_alg_core_from_json(value, out, err, err_len);
    if (rc != 0) {
        BT_TRACE_FAIL("gd.model.vendor_alg_from_json", err);
    } else {
        BT_TRACE_EXIT("gd.model.vendor_alg_from_json");
    }
    return rc;
}

void bt_gd_cases_file_free(bt_gd_cases_file *file) {
    for (size_t i = 0; i < file->count; i++) {
        bt_gd_case_entry *entry = &file->cases[i];
        free(entry->orbit);
        free(entry->t1_name);
        free(entry->t2_name);
        for (size_t a = 0; a < entry->vendor_count; a++) {
            bt_gd_vendor_alg_free(&entry->vendor_algs[a]);
        }
        free(entry->vendor_algs);
    }
    free(file->cases);
    file->cases = NULL;
    file->count = 0;
}

bool bt_gd_cases_file_eq(const bt_gd_cases_file *a, const bt_gd_cases_file *b) {
    if (a->count != b->count) {
        return false;
    }
    for (size_t i = 0; i < a->count; i++) {
        const bt_gd_case_entry *left = &a->cases[i];
        const bt_gd_case_entry *right = &b->cases[i];
        if (strcmp(left->orbit, right->orbit) != 0 || left->t1 != right->t1 ||
            left->t2 != right->t2 || left->t1_letter != right->t1_letter ||
            left->t2_letter != right->t2_letter || left->covered != right->covered ||
            left->vendor_count != right->vendor_count ||
            strcmp(left->t1_name, right->t1_name) != 0 ||
            strcmp(left->t2_name, right->t2_name) != 0) {
            return false;
        }
        for (size_t v = 0; v < left->vendor_count; v++) {
            if (!bt_gd_vendor_alg_eq(&left->vendor_algs[v], &right->vendor_algs[v])) {
                return false;
            }
        }
    }
    return true;
}

static bt_json_value *case_entry_to_json(const bt_gd_case_entry *entry) {
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        return NULL;
    }
    char letter[2] = {entry->t1_letter, '\0'};
    int ok = set_string(object, "orbit", entry->orbit) == 0 &&
             set_integer(object, "t1", entry->t1) == 0 &&
             set_integer(object, "t2", entry->t2) == 0 &&
             set_string(object, "t1_letter", letter) == 0;
    letter[0] = entry->t2_letter;
    if (ok) {
        ok = set_string(object, "t2_letter", letter) == 0 &&
             set_string(object, "t1_name", entry->t1_name) == 0 &&
             set_string(object, "t2_name", entry->t2_name) == 0 &&
             bt_json_object_set(object, "covered", bt_json_new_bool(entry->covered ? 1 : 0)) == 0;
    }
    if (!ok) {
        bt_json_free(object);
        return NULL;
    }
    bt_json_value *vendor = bt_json_new_array();
    if (vendor == NULL) {
        bt_json_free(object);
        return NULL;
    }
    for (size_t v = 0; v < entry->vendor_count; v++) {
        if (push_member(vendor, vendor_alg_core_to_json(&entry->vendor_algs[v])) != 0) {
            bt_json_free(vendor);
            bt_json_free(object);
            return NULL;
        }
    }
    if (bt_json_object_set(object, "vendor_algs", vendor) != 0) {
        bt_json_free(vendor);
        bt_json_free(object);
        return NULL;
    }
    return object;
}

bt_json_value *bt_gd_cases_file_to_json(const bt_gd_cases_file *file) {
    BT_TRACE_ENTER("gd.model.cases_file_to_json");
    bt_json_value *root = bt_json_new_object();
    bt_json_value *cases = bt_json_new_array();
    if (root == NULL || cases == NULL) {
        bt_json_free(cases);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.cases_file_to_json", "root allocation failed");
        return NULL;
    }
    for (size_t i = 0; i < file->count; i++) {
        if (push_member(cases, case_entry_to_json(&file->cases[i])) != 0) {
            bt_json_free(cases);
            bt_json_free(root);
            BT_TRACE_FAIL("gd.model.cases_file_to_json", "case entry dropped");
            return NULL;
        }
    }
    if (bt_json_object_set(root, "cases", cases) != 0) {
        bt_json_free(cases);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.cases_file_to_json", "cases member dropped");
        return NULL;
    }
    BT_TRACE_EXIT("gd.model.cases_file_to_json");
    return root;
}

static int cases_file_from_json(const bt_json_value *value, bt_gd_cases_file *out, bool full,
                                char *err, size_t err_len) {
    memset(out, 0, sizeof *out);
    const bt_json_value *cases = bt_json_get(value, "cases");
    if (cases == NULL || cases->type != BT_JSON_ARRAY) {
        snprintf(err, err_len, "%s", "missing field `cases`");
        return -1;
    }
    out->count = cases->as.array.count;
    out->cases = bt_malloc(out->count * sizeof(bt_gd_case_entry));
    if (out->cases == NULL && out->count > 0) {
        out->count = 0;
        snprintf(err, err_len, "%s", "allocation failed");
        return -1;
    }
    for (size_t i = 0; i < out->count; i++) {
        const bt_json_value *object = bt_json_at(cases, i);
        bt_gd_case_entry *entry = &out->cases[i];
        memset(entry, 0, sizeof *entry);
        size_t t1 = 0;
        size_t t2 = 0;
        const bt_json_value *t1_letter = bt_json_get(object, "t1_letter");
        const bt_json_value *t2_letter = bt_json_get(object, "t2_letter");
        if (object == NULL || get_string(object, "orbit", &entry->orbit, err, err_len) != 0 ||
            get_integer(object, "t1", &t1, err, err_len) != 0 ||
            get_integer(object, "t2", &t2, err, err_len) != 0 || t1_letter == NULL ||
            t1_letter->type != BT_JSON_STRING || strlen(t1_letter->as.string) != 1 ||
            t2_letter == NULL || t2_letter->type != BT_JSON_STRING ||
            strlen(t2_letter->as.string) != 1 ||
            get_string(object, "t1_name", &entry->t1_name, err, err_len) != 0 ||
            get_string(object, "t2_name", &entry->t2_name, err, err_len) != 0 ||
            get_bool(object, "covered", &entry->covered, err, err_len) != 0) {
            snprintf(err, err_len, "case entry %zu is malformed", i);
            out->count = i + 1;
            bt_gd_cases_file_free(out);
            return -1;
        }
        entry->t1 = (uint8_t)t1;
        entry->t2 = (uint8_t)t2;
        entry->t1_letter = t1_letter->as.string[0];
        entry->t2_letter = t2_letter->as.string[0];
        const bt_json_value *vendor = bt_json_get(object, "vendor_algs");
        if (vendor == NULL || vendor->type != BT_JSON_ARRAY) {
            snprintf(err, err_len, "case entry %zu is malformed", i);
            out->count = i + 1;
            bt_gd_cases_file_free(out);
            return -1;
        }
        if (full) {
            entry->vendor_count = vendor->as.array.count;
            entry->vendor_algs = entry->vendor_count > 0
                                     ? bt_malloc(entry->vendor_count * sizeof(bt_gd_vendor_alg))
                                     : NULL;
            if (entry->vendor_algs == NULL && entry->vendor_count > 0) {
                entry->vendor_count = 0;
                snprintf(err, err_len, "%s", "allocation failed");
                out->count = i + 1;
                bt_gd_cases_file_free(out);
                return -1;
            }
            for (size_t v = 0; v < entry->vendor_count; v++) {
                if (vendor_alg_core_from_json(bt_json_at(vendor, v), &entry->vendor_algs[v], err,
                                              err_len) != 0) {
                    entry->vendor_count = v + 1;
                    bt_gd_cases_file_free(out);
                    return -1;
                }
            }
        }
    }
    return 0;
}

int bt_gd_cases_file_from_json(const bt_json_value *value, bt_gd_cases_file *out, char *err,
                               size_t err_len) {
    BT_TRACE_ENTER("gd.model.cases_file_from_json");
    int rc = cases_file_from_json(value, out, true, err, err_len);
    if (rc != 0) {
        BT_TRACE_FAIL("gd.model.cases_file_from_json", err);
    } else {
        BT_TRACE_EXIT("gd.model.cases_file_from_json");
    }
    return rc;
}

void bt_gd_specials_file_free(bt_gd_specials_file *file) {
    for (size_t i = 0; i < file->count; i++) {
        bt_gd_special_entry *special = &file->specials[i];
        free(special->class_name);
        free(special->solver);
        free(special->position);
        free(special->alg);
        string_vec_free(special->problems, special->problem_count);
        string_vec_free(special->notes, special->note_count);
        free(special->commutator);
        free(special->recon_link);
        free(special->youtube_link);
    }
    free(file->specials);
    file->specials = NULL;
    file->count = 0;
}

bool bt_gd_specials_file_eq(const bt_gd_specials_file *a, const bt_gd_specials_file *b) {
    if (a->count != b->count) {
        return false;
    }
    for (size_t i = 0; i < a->count; i++) {
        const bt_gd_special_entry *left = &a->specials[i];
        const bt_gd_special_entry *right = &b->specials[i];
        if (strcmp(left->class_name, right->class_name) != 0 ||
            strcmp(left->solver, right->solver) != 0 || left->rowid != right->rowid ||
            strcmp(left->position, right->position) != 0 || strcmp(left->alg, right->alg) != 0 ||
            left->qtm != right->qtm || left->htm != right->htm || left->ok != right->ok ||
            !string_vec_eq(left->problems, left->problem_count, right->problems,
                           right->problem_count) ||
            !string_vec_eq(left->notes, left->note_count, right->notes, right->note_count) ||
            strcmp(left->commutator, right->commutator) != 0 ||
            strcmp(left->recon_link, right->recon_link) != 0 ||
            strcmp(left->youtube_link, right->youtube_link) != 0) {
            return false;
        }
    }
    return true;
}

static bt_json_value *special_core_to_json(const bt_gd_special_entry *special, bool with_class) {
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        return NULL;
    }
    int ok = 1;
    if (with_class) {
        ok = set_string(object, "class", special->class_name) == 0;
    }
    if (ok) {
        ok = set_string(object, "solver", special->solver) == 0 &&
             set_integer(object, "rowid", (long long)special->rowid) == 0 &&
             set_string(object, "position", special->position) == 0 &&
             set_string(object, "alg", special->alg) == 0 &&
             set_integer(object, "qtm", special->qtm) == 0 &&
             set_integer(object, "htm", special->htm) == 0 &&
             bt_json_object_set(object, "ok", bt_json_new_bool(special->ok ? 1 : 0)) == 0;
    }
    if (!ok) {
        bt_json_free(object);
        return NULL;
    }
    bt_json_value *problems = string_vec_to_json(special->problems, special->problem_count);
    bt_json_value *notes = string_vec_to_json(special->notes, special->note_count);
    if (problems == NULL || notes == NULL ||
        bt_json_object_set(object, "problems", problems) != 0) {
        bt_json_free(problems);
        bt_json_free(notes);
        bt_json_free(object);
        return NULL;
    }
    if (bt_json_object_set(object, "notes", notes) != 0) {
        bt_json_free(notes);
        bt_json_free(object);
        return NULL;
    }
    if (set_string(object, "commutator", special->commutator) != 0 ||
        set_string(object, "recon_link", special->recon_link) != 0 ||
        set_string(object, "youtube_link", special->youtube_link) != 0) {
        bt_json_free(object);
        return NULL;
    }
    return object;
}

bt_json_value *bt_gd_specials_file_to_json(const bt_gd_specials_file *file) {
    BT_TRACE_ENTER("gd.model.specials_file_to_json");
    bt_json_value *root = bt_json_new_object();
    bt_json_value *specials = bt_json_new_array();
    if (root == NULL || specials == NULL) {
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.specials_file_to_json", "root allocation failed");
        return NULL;
    }
    for (size_t i = 0; i < file->count; i++) {
        if (push_member(specials, special_core_to_json(&file->specials[i], true)) != 0) {
            bt_json_free(specials);
            bt_json_free(root);
            BT_TRACE_FAIL("gd.model.specials_file_to_json", "special entry dropped");
            return NULL;
        }
    }
    if (bt_json_object_set(root, "specials", specials) != 0) {
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.specials_file_to_json", "specials member dropped");
        return NULL;
    }
    BT_TRACE_EXIT("gd.model.specials_file_to_json");
    return root;
}

static int special_core_from_json(const bt_json_value *object, bt_gd_special_entry *out,
                                  bool with_class, char *err, size_t err_len) {
    memset(out, 0, sizeof *out);
    if ((with_class && get_string(object, "class", &out->class_name, err, err_len) != 0) ||
        get_string(object, "solver", &out->solver, err, err_len) != 0 ||
        get_integer(object, "rowid", &out->rowid, err, err_len) != 0 ||
        get_string(object, "position", &out->position, err, err_len) != 0 ||
        get_string(object, "alg", &out->alg, err, err_len) != 0 ||
        get_u32(object, "qtm", &out->qtm, err, err_len) != 0 ||
        get_u32(object, "htm", &out->htm, err, err_len) != 0 ||
        get_bool(object, "ok", &out->ok, err, err_len) != 0 ||
        string_vec_from_json(bt_json_get(object, "problems"), &out->problems, &out->problem_count,
                             err, err_len) != 0 ||
        string_vec_from_json(bt_json_get(object, "notes"), &out->notes, &out->note_count, err,
                             err_len) != 0 ||
        get_string(object, "commutator", &out->commutator, err, err_len) != 0 ||
        get_string(object, "recon_link", &out->recon_link, err, err_len) != 0 ||
        get_string(object, "youtube_link", &out->youtube_link, err, err_len) != 0) {
        free(out->class_name);
        free(out->solver);
        free(out->position);
        free(out->alg);
        string_vec_free(out->problems, out->problem_count);
        string_vec_free(out->notes, out->note_count);
        free(out->commutator);
        free(out->recon_link);
        free(out->youtube_link);
        memset(out, 0, sizeof *out);
        return -1;
    }
    return 0;
}

int bt_gd_specials_file_from_json(const bt_json_value *value, bt_gd_specials_file *out, char *err,
                                  size_t err_len) {
    BT_TRACE_ENTER("gd.model.specials_file_from_json");
    memset(out, 0, sizeof *out);
    const bt_json_value *specials = bt_json_get(value, "specials");
    if (specials == NULL || specials->type != BT_JSON_ARRAY) {
        snprintf(err, err_len, "%s", "missing field `specials`");
        BT_TRACE_FAIL("gd.model.specials_file_from_json", err);
        return -1;
    }
    out->count = specials->as.array.count;
    out->specials = bt_malloc(out->count * sizeof(bt_gd_special_entry));
    if (out->specials == NULL && out->count > 0) {
        out->count = 0;
        snprintf(err, err_len, "%s", "allocation failed");
        BT_TRACE_FAIL("gd.model.specials_file_from_json", err);
        return -1;
    }
    for (size_t i = 0; i < out->count; i++) {
        if (special_core_from_json(bt_json_at(specials, i), &out->specials[i], true, err,
                                   err_len) != 0) {
            out->count = i;
            bt_gd_specials_file_free(out);
            BT_TRACE_FAIL("gd.model.specials_file_from_json", err);
            return -1;
        }
    }
    BT_TRACE_EXIT("gd.model.specials_file_from_json");
    return 0;
}

static bt_json_value *coverage_to_json(const bt_gd_orbit_coverage *coverage) {
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        return NULL;
    }
    if (set_integer(object, "total", (long long)coverage->total) != 0 ||
        set_integer(object, "covered", (long long)coverage->covered) != 0 ||
        set_integer(object, "uncovered", (long long)coverage->uncovered) != 0) {
        bt_json_free(object);
        return NULL;
    }
    return object;
}

static int coverage_from_json(const bt_json_value *object, bt_gd_orbit_coverage *out, char *err,
                              size_t err_len) {
    return get_integer(object, "total", &out->total, err, err_len) != 0 ||
                   get_integer(object, "covered", &out->covered, err, err_len) != 0 ||
                   get_integer(object, "uncovered", &out->uncovered, err, err_len) != 0
               ? -1
               : 0;
}

static bt_json_value *corpus_file_to_json(const bt_gd_corpus_file_entry *entry) {
    bt_json_value *object = bt_json_new_object();
    if (object == NULL) {
        return NULL;
    }
    if (set_string(object, "solver", entry->solver) != 0 ||
        set_string(object, "sheet", entry->sheet) != 0 ||
        set_integer(object, "rows", (long long)entry->rows) != 0 ||
        set_string(object, "sha256", entry->sha256) != 0) {
        bt_json_free(object);
        return NULL;
    }
    return object;
}

static int corpus_file_from_json(const bt_json_value *object, bt_gd_corpus_file_entry *out,
                                 char *err, size_t err_len) {
    memset(out, 0, sizeof *out);
    if (get_string(object, "solver", &out->solver, err, err_len) != 0 ||
        get_string(object, "sheet", &out->sheet, err, err_len) != 0 ||
        get_integer(object, "rows", &out->rows, err, err_len) != 0 ||
        get_string(object, "sha256", &out->sha256, err, err_len) != 0) {
        free(out->solver);
        free(out->sheet);
        free(out->sha256);
        memset(out, 0, sizeof *out);
        return -1;
    }
    return 0;
}

void bt_gd_manifest_free(bt_gd_manifest *manifest) {
    free(manifest->generated_at);
    free(manifest->generator);
    free(manifest->synth_params_sha256);
    free(manifest->corpus_sha256);
    for (size_t i = 0; i < manifest->corpus_file_count; i++) {
        free(manifest->corpus_files[i].solver);
        free(manifest->corpus_files[i].sheet);
        free(manifest->corpus_files[i].sha256);
    }
    free(manifest->corpus_files);
    free(manifest->cases.path);
    free(manifest->cases.sha256);
    free(manifest->specials.path);
    free(manifest->specials.sha256);
    for (size_t i = 0; i < manifest->specials.count_count; i++) {
        free(manifest->specials.counts[i].solver);
        free(manifest->specials.counts[i].class_name);
    }
    free(manifest->specials.counts);
    memset(manifest, 0, sizeof *manifest);
}

bool bt_gd_manifest_eq(const bt_gd_manifest *a, const bt_gd_manifest *b) {
    if (strcmp(a->generated_at, b->generated_at) != 0 || strcmp(a->generator, b->generator) != 0 ||
        strcmp(a->synth_params_sha256, b->synth_params_sha256) != 0 ||
        strcmp(a->corpus_sha256, b->corpus_sha256) != 0 || a->corpus_rows != b->corpus_rows ||
        a->corpus_file_count != b->corpus_file_count ||
        a->rows_in_pair_space != b->rows_in_pair_space ||
        a->rows_other_buffer != b->rows_other_buffer || strcmp(a->cases.path, b->cases.path) != 0 ||
        strcmp(a->cases.sha256, b->cases.sha256) != 0 || a->cases.entries != b->cases.entries ||
        a->cases.edge.total != b->cases.edge.total ||
        a->cases.edge.covered != b->cases.edge.covered ||
        a->cases.edge.uncovered != b->cases.edge.uncovered ||
        a->cases.corner.total != b->cases.corner.total ||
        a->cases.corner.covered != b->cases.corner.covered ||
        a->cases.corner.uncovered != b->cases.corner.uncovered ||
        strcmp(a->specials.path, b->specials.path) != 0 ||
        strcmp(a->specials.sha256, b->specials.sha256) != 0 ||
        a->specials.entries != b->specials.entries ||
        a->specials.count_count != b->specials.count_count) {
        return false;
    }
    for (size_t i = 0; i < a->corpus_file_count; i++) {
        if (strcmp(a->corpus_files[i].solver, b->corpus_files[i].solver) != 0 ||
            strcmp(a->corpus_files[i].sheet, b->corpus_files[i].sheet) != 0 ||
            a->corpus_files[i].rows != b->corpus_files[i].rows ||
            strcmp(a->corpus_files[i].sha256, b->corpus_files[i].sha256) != 0) {
            return false;
        }
    }
    for (size_t i = 0; i < a->specials.count_count; i++) {
        if (strcmp(a->specials.counts[i].solver, b->specials.counts[i].solver) != 0 ||
            strcmp(a->specials.counts[i].class_name, b->specials.counts[i].class_name) != 0 ||
            a->specials.counts[i].rows != b->specials.counts[i].rows) {
            return false;
        }
    }
    return true;
}

bt_json_value *bt_gd_manifest_to_json(const bt_gd_manifest *manifest) {
    BT_TRACE_ENTER("gd.model.manifest_to_json");
    bt_json_value *root = bt_json_new_object();
    if (root == NULL) {
        BT_TRACE_FAIL("gd.model.manifest_to_json", "root allocation failed");
        return NULL;
    }
    if (set_string(root, "generated_at", manifest->generated_at) != 0 ||
        set_string(root, "generator", manifest->generator) != 0 ||
        set_string(root, "synth_params_sha256", manifest->synth_params_sha256) != 0 ||
        set_string(root, "corpus_sha256", manifest->corpus_sha256) != 0 ||
        set_integer(root, "corpus_rows", (long long)manifest->corpus_rows) != 0) {
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "header member dropped");
        return NULL;
    }
    bt_json_value *files = bt_json_new_array();
    if (files == NULL) {
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "files array allocation failed");
        return NULL;
    }
    for (size_t i = 0; i < manifest->corpus_file_count; i++) {
        if (push_member(files, corpus_file_to_json(&manifest->corpus_files[i])) != 0) {
            bt_json_free(files);
            bt_json_free(root);
            BT_TRACE_FAIL("gd.model.manifest_to_json", "corpus file entry dropped");
            return NULL;
        }
    }
    if (bt_json_object_set(root, "corpus_files", files) != 0) {
        bt_json_free(files);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "corpus_files member dropped");
        return NULL;
    }
    if (set_integer(root, "rows_in_pair_space", (long long)manifest->rows_in_pair_space) != 0 ||
        set_integer(root, "rows_other_buffer", (long long)manifest->rows_other_buffer) != 0) {
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "row count member dropped");
        return NULL;
    }
    bt_json_value *cases = bt_json_new_object();
    if (cases == NULL) {
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "cases object allocation failed");
        return NULL;
    }
    if (set_string(cases, "path", manifest->cases.path) != 0 ||
        set_string(cases, "sha256", manifest->cases.sha256) != 0 ||
        set_integer(cases, "entries", (long long)manifest->cases.entries) != 0 ||
        set_member(cases, "edge", coverage_to_json(&manifest->cases.edge)) != 0 ||
        set_member(cases, "corner", coverage_to_json(&manifest->cases.corner)) != 0) {
        bt_json_free(cases);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "cases member dropped");
        return NULL;
    }
    if (bt_json_object_set(root, "cases", cases) != 0) {
        bt_json_free(cases);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "cases section dropped");
        return NULL;
    }
    bt_json_value *specials = bt_json_new_object();
    if (specials == NULL) {
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "specials object allocation failed");
        return NULL;
    }
    if (set_string(specials, "path", manifest->specials.path) != 0 ||
        set_string(specials, "sha256", manifest->specials.sha256) != 0 ||
        set_integer(specials, "entries", (long long)manifest->specials.entries) != 0) {
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "specials member dropped");
        return NULL;
    }
    bt_json_value *counts = bt_json_new_array();
    if (counts == NULL) {
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "counts array allocation failed");
        return NULL;
    }
    for (size_t i = 0; i < manifest->specials.count_count; i++) {
        bt_json_value *count = bt_json_new_object();
        if (count == NULL ||
            set_string(count, "solver", manifest->specials.counts[i].solver) != 0 ||
            set_string(count, "class", manifest->specials.counts[i].class_name) != 0 ||
            set_integer(count, "rows", (long long)manifest->specials.counts[i].rows) != 0 ||
            bt_json_array_push(counts, count) != 0) {
            bt_json_free(count);
            bt_json_free(counts);
            bt_json_free(specials);
            bt_json_free(root);
            BT_TRACE_FAIL("gd.model.manifest_to_json", "class count entry dropped");
            return NULL;
        }
    }
    if (bt_json_object_set(specials, "counts", counts) != 0) {
        bt_json_free(counts);
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "counts member dropped");
        return NULL;
    }
    if (bt_json_object_set(root, "specials", specials) != 0) {
        bt_json_free(specials);
        bt_json_free(root);
        BT_TRACE_FAIL("gd.model.manifest_to_json", "specials section dropped");
        return NULL;
    }
    BT_TRACE_EXIT("gd.model.manifest_to_json");
    return root;
}

int bt_gd_manifest_from_json(const bt_json_value *value, bt_gd_manifest *out, char *err,
                             size_t err_len) {
    BT_TRACE_ENTER("gd.model.manifest_from_json");
    memset(out, 0, sizeof *out);
    const bt_json_value *files = bt_json_get(value, "corpus_files");
    const bt_json_value *counts = NULL;
    const bt_json_value *cases = bt_json_get(value, "cases");
    const bt_json_value *specials = bt_json_get(value, "specials");
    if (get_string(value, "generated_at", &out->generated_at, err, err_len) != 0 ||
        get_string(value, "generator", &out->generator, err, err_len) != 0 ||
        get_string(value, "synth_params_sha256", &out->synth_params_sha256, err, err_len) != 0 ||
        get_string(value, "corpus_sha256", &out->corpus_sha256, err, err_len) != 0 ||
        get_integer(value, "corpus_rows", &out->corpus_rows, err, err_len) != 0 || files == NULL ||
        files->type != BT_JSON_ARRAY ||
        get_integer(value, "rows_in_pair_space", &out->rows_in_pair_space, err, err_len) != 0 ||
        get_integer(value, "rows_other_buffer", &out->rows_other_buffer, err, err_len) != 0 ||
        cases == NULL || specials == NULL) {
        if (err[0] == '\0') {
            snprintf(err, err_len, "%s", "missing manifest section");
        }
        BT_TRACE_FAIL("gd.model.manifest_from_json", err);
        bt_gd_manifest_free(out);
        return -1;
    }
    out->corpus_files = bt_malloc(files->as.array.count * sizeof(bt_gd_corpus_file_entry));
    if (out->corpus_files == NULL && files->as.array.count > 0) {
        snprintf(err, err_len, "%s", "allocation failed");
        BT_TRACE_FAIL("gd.model.manifest_from_json", err);
        bt_gd_manifest_free(out);
        return -1;
    }
    out->corpus_file_count = files->as.array.count;
    for (size_t i = 0; i < out->corpus_file_count; i++) {
        if (corpus_file_from_json(bt_json_at(files, i), &out->corpus_files[i], err, err_len) != 0) {
            out->corpus_file_count = i + 1;
            bt_gd_manifest_free(out);
            BT_TRACE_FAIL("gd.model.manifest_from_json", err);
            return -1;
        }
    }
    counts = bt_json_get(specials, "counts");
    if (get_string(cases, "path", &out->cases.path, err, err_len) != 0 ||
        get_string(cases, "sha256", &out->cases.sha256, err, err_len) != 0 ||
        get_integer(cases, "entries", &out->cases.entries, err, err_len) != 0 ||
        coverage_from_json(bt_json_get(cases, "edge"), &out->cases.edge, err, err_len) != 0 ||
        coverage_from_json(bt_json_get(cases, "corner"), &out->cases.corner, err, err_len) != 0 ||
        get_string(specials, "path", &out->specials.path, err, err_len) != 0 ||
        get_string(specials, "sha256", &out->specials.sha256, err, err_len) != 0 ||
        get_integer(specials, "entries", &out->specials.entries, err, err_len) != 0 ||
        counts == NULL || counts->type != BT_JSON_ARRAY) {
        if (err[0] == '\0') {
            snprintf(err, err_len, "%s", "missing manifest section");
        }
        BT_TRACE_FAIL("gd.model.manifest_from_json", err);
        bt_gd_manifest_free(out);
        return -1;
    }
    out->specials.counts = bt_malloc(counts->as.array.count * sizeof(bt_gd_class_count));
    if (out->specials.counts == NULL && counts->as.array.count > 0) {
        snprintf(err, err_len, "%s", "allocation failed");
        BT_TRACE_FAIL("gd.model.manifest_from_json", err);
        bt_gd_manifest_free(out);
        return -1;
    }
    out->specials.count_count = counts->as.array.count;
    for (size_t i = 0; i < out->specials.count_count; i++) {
        const bt_json_value *count = bt_json_at(counts, i);
        out->specials.counts[i] = (bt_gd_class_count){0};
        if (count == NULL ||
            get_string(count, "solver", &out->specials.counts[i].solver, err, err_len) != 0 ||
            get_string(count, "class", &out->specials.counts[i].class_name, err, err_len) != 0 ||
            get_integer(count, "rows", &out->specials.counts[i].rows, err, err_len) != 0) {
            out->specials.count_count = i + 1;
            bt_gd_manifest_free(out);
            BT_TRACE_FAIL("gd.model.manifest_from_json", err);
            return -1;
        }
    }
    BT_TRACE_EXIT("gd.model.manifest_from_json");
    return 0;
}
