#include "app/persist.h"

#include "common/bt_trace.h"
#include "common/strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const bt_memo_label BT_MEMO_MODE_LABELS[BT_MEMO_MODE_COUNT] = {
    {"syllables", BT_MEMO_SYLLABLES},
    {"images", BT_MEMO_IMAGES},
    {"per-orbit", BT_MEMO_PER_ORBIT},
};

int bt_memo_mode_parse(const char *text, bt_memo_mode *out) {
    BT_TRACE_ENTER("app.memo_mode_parse");
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        if (strcmp(BT_MEMO_MODE_LABELS[i].label, text) == 0) {
            *out = BT_MEMO_MODE_LABELS[i].mode;
            BT_TRACE_EXIT("app.memo_mode_parse");
            return 1;
        }
    }
    BT_TRACE_EXIT("app.memo_mode_parse");
    return 0;
}

const char *bt_memo_mode_label(bt_memo_mode mode) {
    BT_TRACE_ENTER("app.memo_mode_label");
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        if (BT_MEMO_MODE_LABELS[i].mode == mode) {
            BT_TRACE_EXIT("app.memo_mode_label");
            return BT_MEMO_MODE_LABELS[i].label;
        }
    }
    BT_TRACE_FAIL("app.memo_mode_label", "mode %d has no label", (int)mode);
    BT_TRACE_EXIT("app.memo_mode_label");
    return NULL;
}

int bt_memo_mode_index(bt_memo_mode mode) {
    BT_TRACE_ENTER("app.memo_mode_index");
    for (int i = 0; i < BT_MEMO_MODE_COUNT; i++) {
        if (BT_MEMO_MODE_LABELS[i].mode == mode) {
            BT_TRACE_EXIT("app.memo_mode_index");
            return i;
        }
    }
    BT_TRACE_FAIL("app.memo_mode_index", "mode %d has no index", (int)mode);
    BT_TRACE_EXIT("app.memo_mode_index");
    return -1;
}

int bt_memo_mode_from_index(int index, bt_memo_mode *out) {
    BT_TRACE_ENTER("app.memo_mode_from_index");
    if (index < 0 || index >= BT_MEMO_MODE_COUNT) {
        BT_TRACE_EXIT("app.memo_mode_from_index");
        return 0;
    }
    *out = BT_MEMO_MODE_LABELS[index].mode;
    BT_TRACE_EXIT("app.memo_mode_from_index");
    return 1;
}

void bt_str_list_init(bt_str_list *list) {
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void bt_str_list_free(bt_str_list *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    bt_str_list_init(list);
}

static int str_list_grow(bt_str_list *list) {
    if (list->count < list->cap) {
        return 0;
    }
    size_t cap = list->cap == 0 ? 8 : list->cap * 2;
    char **grown = realloc(list->items, cap * sizeof *grown);
    if (grown == NULL) {
        BT_TRACE_FAIL("app.str_list", "growth to %zu failed", cap);
        return -1;
    }
    list->items = grown;
    list->cap = cap;
    return 0;
}

static char *str_copy(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, text, len + 1);
    }
    return copy;
}

int bt_str_list_push(bt_str_list *list, const char *text) {
    BT_TRACE_ENTER("app.str_list_push");
    if (str_list_grow(list) != 0) {
        BT_TRACE_EXIT("app.str_list_push");
        return -1;
    }
    char *copy = str_copy(text);
    if (copy == NULL) {
        BT_TRACE_FAIL("app.str_list_push", "copy of %s failed", text);
        BT_TRACE_EXIT("app.str_list_push");
        return -1;
    }
    list->items[list->count++] = copy;
    BT_TRACE_EXIT("app.str_list_push");
    return 0;
}

int bt_str_list_sorted_insert_unique(bt_str_list *list, const char *text) {
    BT_TRACE_ENTER("app.str_list_insert");
    size_t at = 0;
    while (at < list->count && strcmp(list->items[at], text) < 0) {
        at++;
    }
    if (at < list->count && strcmp(list->items[at], text) == 0) {
        BT_TRACE_EXIT("app.str_list_insert");
        return 0;
    }
    if (str_list_grow(list) != 0) {
        BT_TRACE_EXIT("app.str_list_insert");
        return -1;
    }
    char *copy = str_copy(text);
    if (copy == NULL) {
        BT_TRACE_FAIL("app.str_list_insert", "copy of %s failed", text);
        BT_TRACE_EXIT("app.str_list_insert");
        return -1;
    }
    memmove(list->items + at + 1, list->items + at, (list->count - at) * sizeof *list->items);
    list->items[at] = copy;
    list->count++;
    BT_TRACE_EXIT("app.str_list_insert");
    return 0;
}

static int uint_field(const bt_json_value *value) {
    return value != NULL && value->type == BT_JSON_NUMBER && bt_json_is_integer(value) &&
           value->integer >= 0 && value->integer <= 0xFFFFFFFFLL;
}

int bt_settings_from_json(const bt_json_value *value, bt_settings *out, char *error,
                          size_t error_len) {
    BT_TRACE_ENTER("app.settings_from_json");
    const bt_json_value *cadence = bt_json_get(value, "memo_cadence_ms");
    const bt_json_value *mode = bt_json_get(value, "memo_mode");
    const bt_json_value *reps = bt_json_get(value, "drill_reps");
    if (!uint_field(cadence) || !uint_field(reps) || mode == NULL || mode->type != BT_JSON_STRING) {
        snprintf(error, error_len, "settings fields malformed");
        BT_TRACE_FAIL("app.settings_from_json", "fields malformed");
        BT_TRACE_EXIT("app.settings_from_json");
        return -1;
    }
    bt_memo_mode parsed;
    if (!bt_memo_mode_parse(mode->as.string, &parsed)) {
        snprintf(error, error_len, "settings memo_mode unknown: %s", mode->as.string);
        BT_TRACE_FAIL("app.settings_from_json", "memo_mode %s unknown", mode->as.string);
        BT_TRACE_EXIT("app.settings_from_json");
        return -1;
    }
    out->memo_cadence_ms = (unsigned int)cadence->integer;
    out->memo_mode = parsed;
    out->drill_reps = (unsigned int)reps->integer;
    BT_TRACE_EXIT("app.settings_from_json");
    return 0;
}

bt_json_value *bt_settings_to_json(const bt_settings *settings) {
    BT_TRACE_ENTER("app.settings_to_json");
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, "memo_cadence_ms",
                       bt_json_new_integer((long long)settings->memo_cadence_ms));
    bt_json_object_set(root, "memo_mode",
                       bt_json_new_string(bt_memo_mode_label(settings->memo_mode)));
    bt_json_object_set(root, "drill_reps", bt_json_new_integer((long long)settings->drill_reps));
    BT_TRACE_EXIT("app.settings_to_json");
    return root;
}

static void db_shown_of(const char *root, char *out, size_t out_len) {
    char path[BT_PATH_MAX];
    bt_data_db_path_at(root, path);
    snprintf(out, out_len, "%s", path);
}

static void prefix_db_shown(const char *root, char *error, size_t error_len) {
    char shown[BT_PATH_MAX];
    db_shown_of(root, shown, sizeof shown);
    char held[BT_APP_ERROR_MAX];
    snprintf(held, sizeof held, "%s", error);
    snprintf(error, error_len, "%s: %s", shown, held);
}

static void prefix_db_shown_settings(const char *root, char *error, size_t error_len) {
    char shown[BT_PATH_MAX];
    db_shown_of(root, shown, sizeof shown);
    char held[BT_APP_ERROR_MAX];
    snprintf(held, sizeof held, "%s", error);
    snprintf(error, error_len, "%s settings: %s", shown, held);
}

static int open_root(const char *root, bt_db *db, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.open_root");
    char path[BT_PATH_MAX];
    bt_data_db_path_at(root, path);
    bt_db_error err;
    if (bt_db_open_at(path, db, &err) != 0) {
        bt_db_error_string(&err, error, error_len);
        BT_TRACE_FAIL("app.open_root", "db open failed: %s", error);
        BT_TRACE_EXIT("app.open_root");
        return -1;
    }
    BT_TRACE_EXIT("app.open_root");
    return 0;
}

static int settings_from_db(bt_db *db, const char *root, bt_settings defaults, bt_settings *out,
                            char *error, size_t error_len) {
    BT_TRACE_ENTER("app.settings_from_db");
    bt_db_error err;
    char *text = NULL;
    if (bt_repo_settings_load_text(db, BT_REPO_SETTINGS_APP_KEY, &text, &err) != 0) {
        bt_db_error_string(&err, error, error_len);
        prefix_db_shown(root, error, error_len);
        BT_TRACE_FAIL("app.settings_from_db", "load_text failed: %s", error);
        BT_TRACE_EXIT("app.settings_from_db");
        return -1;
    }
    if (text == NULL) {
        *out = defaults;
        BT_TRACE_EXIT("app.settings_from_db");
        return 0;
    }
    bt_json_value *value = NULL;
    bt_json_error json_error;
    int rc = bt_json_parse(text, strlen(text), &value, &json_error);
    free(text);
    int parsed = 0;
    if (rc == 0 && value != NULL && value->type == BT_JSON_OBJECT) {
        parsed = bt_settings_from_json(value, out, error, error_len) == 0;
        bt_json_free(value);
    } else {
        snprintf(error, error_len, "%s", json_error.message);
    }
    if (!parsed) {
        prefix_db_shown_settings(root, error, error_len);
        BT_TRACE_FAIL("app.settings_from_db", "settings row malformed: %s", error);
        BT_TRACE_EXIT("app.settings_from_db");
        return -1;
    }
    BT_TRACE_EXIT("app.settings_from_db");
    return 0;
}

static int packs_grow(bt_seed_repo *repo) {
    if (repo->pack_count < repo->pack_cap) {
        return 0;
    }
    size_t cap = repo->pack_cap == 0 ? 4 : repo->pack_cap * 2;
    bt_pack_entry *grown = realloc(repo->packs, cap * sizeof *grown);
    if (grown == NULL) {
        BT_TRACE_FAIL("app.load_seeds", "pack growth to %zu failed", cap);
        return -1;
    }
    repo->packs = grown;
    repo->pack_cap = cap;
    return 0;
}

static int load_seeds(bt_db *db, const char *packs_root, bt_seed_repo *repo, char *error,
                      size_t error_len) {
    BT_TRACE_ENTER("app.load_seeds");
    bt_seed_rows_init(&repo->rows);
    bt_str_list_init(&repo->hidden);
    bt_str_list_init(&repo->warnings);
    int registry_failed = 0;
    if (packs_root != NULL) {
        bt_pack_entry *entries = NULL;
        size_t entry_count = 0;
        bt_seed_error err;
        if (bt_seedpack_load_registry(packs_root, &entries, &entry_count, &err) == 0) {
            for (size_t i = 0; i < entry_count; i++) {
                bt_seed_report report;
                int seeded = bt_seed_pack(db, packs_root, &entries[i], &report, &err);
                if (seeded == 0 && packs_grow(repo) == 0) {
                    repo->packs[repo->pack_count++] = entries[i];
                } else if (seeded != 0) {
                    bt_str_list_sorted_insert_unique(&repo->hidden, entries[i].event);
                    char text[192];
                    bt_seed_error_string(&err, text, sizeof text);
                    char warning[BT_APP_ERROR_MAX];
                    snprintf(warning, sizeof warning, "pack %s: %s", entries[i].file, text);
                    bt_str_list_push(&repo->warnings, warning);
                }
            }
            bt_seedpack_free_entries(entries);
        } else {
            registry_failed = 1;
            char text[192];
            bt_seed_error_string(&err, text, sizeof text);
            char warning[BT_PATH_MAX + 64];
            snprintf(warning, sizeof warning, "seed registry at %s: %s", packs_root, text);
            bt_str_list_push(&repo->warnings, warning);
        }
    }
    bt_seed_error err;
    if (bt_load_seed_rows(db, &repo->rows, &err) != 0) {
        bt_seed_error_string(&err, error, error_len);
        BT_TRACE_FAIL("app.load_seeds", "load rows failed: %s", error);
        BT_TRACE_EXIT("app.load_seeds");
        return -1;
    }
    if (registry_failed) {
        for (size_t i = 0; i < repo->rows.count; i++) {
            bt_str_list_sorted_insert_unique(&repo->hidden, repo->rows.items[i].event);
        }
    }
    BT_TRACE_EXIT("app.load_seeds");
    return 0;
}

int bt_stores_load_with_packs(const char *root, bt_settings defaults, const bt_fsrs_config *fsrs,
                              const char *packs_root, bt_stores *out, char *error,
                              size_t error_len) {
    BT_TRACE_ENTER("app.stores_load");
    memset(out, 0, sizeof *out);
    bt_db db;
    if (open_root(root, &db, error, error_len) != 0) {
        BT_TRACE_EXIT("app.stores_load");
        return -1;
    }
    int failed = bt_import_run(&db, root, error, error_len) != 0;
    bt_settings settings = defaults;
    if (!failed && settings_from_db(&db, root, defaults, &settings, error, error_len) != 0) {
        failed = 1;
    }
    bt_srs_load_error srs_err;
    if (!failed && bt_repo_srs_load(&db, fsrs, &out->srs, &srs_err) != 0) {
        bt_srs_load_error_string(&srs_err, error, error_len);
        prefix_db_shown(root, error, error_len);
        failed = 1;
    }
    bt_db_error db_err;
    if (!failed && bt_solve_log_load(&db, &out->solves, &db_err) != 0) {
        bt_db_error_string(&db_err, error, error_len);
        prefix_db_shown(root, error, error_len);
        failed = 1;
    }
    if (!failed && bt_repo_algs_load(&db, &out->user_algs, &db_err) != 0) {
        bt_db_error_string(&db_err, error, error_len);
        prefix_db_shown(root, error, error_len);
        failed = 1;
    }
    if (!failed && load_seeds(&db, packs_root, &out->seeds, error, error_len) != 0) {
        prefix_db_shown(root, error, error_len);
        failed = 1;
    }
    bt_db_close(&db);
    if (failed) {
        bt_stores_free(out);
        BT_TRACE_FAIL("app.stores_load", "%s", error);
        BT_TRACE_EXIT("app.stores_load");
        return -1;
    }
    out->settings = settings;
    BT_TRACE_EXIT("app.stores_load");
    return 0;
}

int bt_stores_load(const char *root, bt_settings defaults, const bt_fsrs_config *fsrs,
                   bt_stores *out, char *error, size_t error_len) {
    return bt_stores_load_with_packs(root, defaults, fsrs, NULL, out, error, error_len);
}

void bt_stores_free(bt_stores *stores) {
    BT_TRACE_ENTER("app.stores_free");
    bt_srs_store_free(&stores->srs);
    bt_solve_log_free(&stores->solves);
    bt_user_algs_free(&stores->user_algs);
    bt_seed_rows_free(&stores->seeds.rows);
    free(stores->seeds.packs);
    stores->seeds.packs = NULL;
    stores->seeds.pack_count = 0;
    stores->seeds.pack_cap = 0;
    bt_str_list_free(&stores->seeds.hidden);
    bt_str_list_free(&stores->seeds.warnings);
    BT_TRACE_EXIT("app.stores_free");
}

int bt_app_save_settings(const char *root, const bt_settings *settings, char *error,
                         size_t error_len) {
    BT_TRACE_ENTER("app.save_settings");
    bt_json_value *json = bt_settings_to_json(settings);
    bt_strbuf text;
    bt_strbuf_init(&text, 64);
    int rc = bt_json_serialize(json, &text);
    bt_json_free(json);
    if (rc != 0) {
        bt_strbuf_free(&text);
        snprintf(error, error_len, "settings serialize failed");
        BT_TRACE_FAIL("app.save_settings", "serialize failed");
        BT_TRACE_EXIT("app.save_settings");
        return -1;
    }
    bt_db db;
    if (open_root(root, &db, error, error_len) != 0) {
        bt_strbuf_free(&text);
        BT_TRACE_EXIT("app.save_settings");
        return -1;
    }
    bt_db_error err;
    int failed = bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, text.data, &err) != 0;
    bt_db_close(&db);
    bt_strbuf_free(&text);
    if (failed) {
        bt_db_error_string(&err, error, error_len);
        prefix_db_shown(root, error, error_len);
        BT_TRACE_FAIL("app.save_settings", "%s", error);
    }
    BT_TRACE_EXIT("app.save_settings");
    return failed ? -1 : 0;
}

int bt_app_save_srs(const char *root, const bt_srs_store *srs, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.save_srs");
    bt_db db;
    if (open_root(root, &db, error, error_len) != 0) {
        BT_TRACE_EXIT("app.save_srs");
        return -1;
    }
    bt_db_error err;
    int rc = bt_repo_srs_save(&db, srs, &err);
    bt_db_close(&db);
    if (rc != 0) {
        bt_db_error_string(&err, error, error_len);
        prefix_db_shown(root, error, error_len);
        BT_TRACE_FAIL("app.save_srs", "%s", error);
    }
    BT_TRACE_EXIT("app.save_srs");
    return rc != 0 ? -1 : 0;
}

int bt_app_save_solves(const char *root, const bt_solve_log *solves, char *error,
                       size_t error_len) {
    BT_TRACE_ENTER("app.save_solves");
    bt_db db;
    if (open_root(root, &db, error, error_len) != 0) {
        BT_TRACE_EXIT("app.save_solves");
        return -1;
    }
    bt_db_error err;
    int rc = bt_solve_log_save(&db, solves, &err);
    bt_db_close(&db);
    if (rc != 0) {
        bt_db_error_string(&err, error, error_len);
        prefix_db_shown(root, error, error_len);
        BT_TRACE_FAIL("app.save_solves", "%s", error);
    }
    BT_TRACE_EXIT("app.save_solves");
    return rc != 0 ? -1 : 0;
}

int bt_app_save_user_algs(const char *root, const bt_user_algs *algs, char *error,
                          size_t error_len) {
    BT_TRACE_ENTER("app.save_user_algs");
    bt_db db;
    if (open_root(root, &db, error, error_len) != 0) {
        BT_TRACE_EXIT("app.save_user_algs");
        return -1;
    }
    bt_db_error err;
    int rc = bt_repo_algs_save(&db, algs, &err);
    bt_db_close(&db);
    if (rc != 0) {
        bt_db_error_string(&err, error, error_len);
        prefix_db_shown(root, error, error_len);
        BT_TRACE_FAIL("app.save_user_algs", "%s", error);
    }
    BT_TRACE_EXIT("app.save_user_algs");
    return rc != 0 ? -1 : 0;
}
