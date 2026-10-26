#include "data/paths.h"

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/strbuf.h"

#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const USER_DIR_KINDS[] = {"algs", "srs", "solves", "settings"};

static char *dup_str(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}

void bt_paths_error_string(const bt_paths_error *err, char *out, size_t out_len) {
    switch (err->kind) {
    case BT_PATHS_ERR_READ:
        snprintf(out, out_len, "read %s: %s", err->path, err->detail);
        break;
    case BT_PATHS_ERR_WRITE:
        snprintf(out, out_len, "write %s: %s", err->path, err->detail);
        break;
    case BT_PATHS_ERR_SERIALIZE:
        snprintf(out, out_len, "serialize %s: %s", err->path, err->detail);
        break;
    case BT_PATHS_ERR_DESERIALIZE:
        snprintf(out, out_len, "deserialize %s: %s", err->path, err->detail);
        break;
    case BT_PATHS_ERR_MISSING:
        snprintf(out, out_len, "config section missing: %s", err->section);
        break;
    }
}

void bt_data_workspace_root(char out[BT_PATH_MAX]) {
    bt_paths_workspace_root(out);
}

static int is_absolute_path(const char *path) {
    return path[0] == '/' || path[0] == '\\' || (path[0] != '\0' && path[1] == ':');
}

static int read_file_text(const char *path, char **out, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? 1 : -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        return -1;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';
    *out = text;
    *out_len = read;
    return 0;
}

void bt_data_config_path_from(const char *env_value, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("data.config_path_from");
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    if (env_value != NULL && env_value[0] != '\0') {
        if (is_absolute_path(env_value)) {
            snprintf(out, BT_PATH_MAX, "%s", env_value);
        } else {
            snprintf(out, BT_PATH_MAX, "%s/%s", root, env_value);
        }
    } else {
        snprintf(out, BT_PATH_MAX, "%s/config/config.json", root);
    }
    BT_TRACE_EXIT("data.config_path_from");
}

void bt_data_config_path(char out[BT_PATH_MAX]) {
    bt_data_config_path_from(getenv("BT_CONFIG"), out);
}

bt_json_value *bt_data_load_config(void) {
    BT_TRACE_ENTER("data.load_config");
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    char *text = NULL;
    size_t len = 0;
    if (read_file_text(path, &text, &len) != 0) {
        BT_TRACE_FAIL("data.load_config", "unreadable %s", path);
        return NULL;
    }
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, len, &value, &error) != 0) {
        free(text);
        BT_TRACE_FAIL("data.load_config", "malformed %s", path);
        return NULL;
    }
    free(text);
    BT_TRACE_EXIT("data.load_config");
    return value;
}

static const bt_json_value *config_pointer(const bt_json_value *config,
                                           const char *dotted) {
    return bt_config_value(config, dotted);
}

int bt_data_vendor_root_from(const char *env_value, const bt_json_value *config,
                             char out[BT_PATH_MAX], bt_paths_error *err) {
    BT_TRACE_ENTER("data.vendor_root_from");
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    if (env_value != NULL && env_value[0] != '\0') {
        if (is_absolute_path(env_value)) {
            snprintf(out, BT_PATH_MAX, "%s", env_value);
        } else {
            snprintf(out, BT_PATH_MAX, "%s/%s", root, env_value);
        }
        BT_TRACE_EXIT("data.vendor_root_from");
        return 0;
    }
    const bt_json_value *node = config_pointer(config, "paths.ref_ground");
    if (node == NULL || node->type != BT_JSON_STRING) {
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_MISSING;
        snprintf(err->section, sizeof err->section, "%s", "paths.ref_ground");
        BT_TRACE_FAIL("data.vendor_root_from", "missing paths.ref_ground");
        return -1;
    }
    snprintf(out, BT_PATH_MAX, "%s/%s", root, node->as.string);
    BT_TRACE_EXIT("data.vendor_root_from");
    return 0;
}

bt_json_value *bt_data_vendor_root_or_null(char out[BT_PATH_MAX]) {
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        return NULL;
    }
    bt_paths_error err;
    if (bt_data_vendor_root_from(getenv("BT_REF_GROUND"), config, out, &err) != 0) {
        bt_json_free(config);
        return NULL;
    }
    return config;
}

int bt_data_packs_root_from(const char *env_value, const bt_json_value *config,
                            char out[BT_PATH_MAX], bt_paths_error *err) {
    BT_TRACE_ENTER("data.packs_root_from");
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    if (env_value != NULL && env_value[0] != '\0') {
        char base[BT_PATH_MAX];
        if (is_absolute_path(env_value)) {
            snprintf(base, sizeof base, "%s", env_value);
        } else {
            snprintf(base, sizeof base, "%s/%s", root, env_value);
        }
        snprintf(out, BT_PATH_MAX, "%s/packs", base);
        BT_TRACE_EXIT("data.packs_root_from");
        return 0;
    }
    const bt_json_value *node = config_pointer(config, "paths.packs");
    if (node == NULL || node->type != BT_JSON_STRING) {
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_MISSING;
        snprintf(err->section, sizeof err->section, "%s", "paths.packs");
        BT_TRACE_FAIL("data.packs_root_from", "missing paths.packs");
        return -1;
    }
    snprintf(out, BT_PATH_MAX, "%s/%s", root, node->as.string);
    BT_TRACE_EXIT("data.packs_root_from");
    return 0;
}

bt_json_value *bt_data_packs_root_or_null(char out[BT_PATH_MAX]) {
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        return NULL;
    }
    bt_paths_error err;
    if (bt_data_packs_root_from(getenv("BT_REF_GROUND"), config, out, &err) != 0) {
        bt_json_free(config);
        return NULL;
    }
    return config;
}

void bt_data_user_root_from(const char *appdata, const char *xdg, const char *home,
                            char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("data.user_root_from");
    const char *base = NULL;
    char joined[BT_PATH_MAX];
    if (appdata != NULL && appdata[0] != '\0') {
        base = appdata;
    } else if (xdg != NULL && xdg[0] != '\0') {
        base = xdg;
    } else if (home != NULL && home[0] != '\0') {
        snprintf(joined, sizeof joined, "%s/.local/share", home);
        base = joined;
    } else {
        const char *temp = getenv("TEMP");
        base = temp && temp[0] != '\0' ? temp : ".";
    }
    snprintf(out, BT_PATH_MAX, "%s/%s", base, BT_APP_DIR_NAME);
    BT_TRACE_EXIT("data.user_root_from");
}

void bt_data_user_root(char out[BT_PATH_MAX]) {
    bt_data_user_root_from(getenv("APPDATA"), getenv("XDG_DATA_HOME"), getenv("HOME"),
                           out);
}

static int kind_index(const char *kind) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(kind, USER_DIR_KINDS[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void bt_data_user_dir_path_at(const char *root, const char *kind, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/%s", root, USER_DIR_KINDS[kind_index(kind)]);
}

void bt_data_user_algs_path_at(const char *root, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/algs/%s", root, BT_DATA_USER_ALGS_FILE);
}

void bt_data_settings_path_at(const char *root, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/settings/%s", root, BT_DATA_SETTINGS_FILE);
}

void bt_data_srs_path_at(const char *root, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/srs/%s", root, BT_DATA_SRS_FILE);
}

void bt_data_solves_path_at(const char *root, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/solves/%s", root, BT_DATA_SOLVES_FILE);
}

void bt_data_db_file_name(char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("data.db_file_name");
    snprintf(out, BT_PATH_MAX, "%s", BT_DATA_DB_FILE);
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        BT_TRACE_EXIT("data.db_file_name");
        return;
    }
    const bt_json_value *node = config_pointer(config, "paths.db_file");
    if (node != NULL && node->type == BT_JSON_STRING && node->as.string[0] != '\0') {
        snprintf(out, BT_PATH_MAX, "%s", node->as.string);
    }
    bt_json_free(config);
    BT_TRACE_EXIT("data.db_file_name");
}

void bt_data_db_path_at(const char *root, char out[BT_PATH_MAX]) {
    char name[BT_PATH_MAX];
    bt_data_db_file_name(name);
    snprintf(out, BT_PATH_MAX, "%s/%s", root, name);
}

bt_data_read_status bt_data_read_json(const char *path, bt_json_value **out) {
    BT_TRACE_ENTER("data.read_json");
    *out = NULL;
    char *text = NULL;
    size_t len = 0;
    int read = read_file_text(path, &text, &len);
    if (read == 1) {
        BT_TRACE_EXIT("data.read_json");
        return BT_DATA_READ_MISSING;
    }
    if (read != 0) {
        BT_TRACE_FAIL("data.read_json", "unreadable %s", path);
        return BT_DATA_READ_ERROR;
    }
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, len, &value, &error) != 0) {
        free(text);
        BT_TRACE_FAIL("data.read_json", "malformed %s at %zu:%zu", path, error.line,
                      error.column);
        return BT_DATA_READ_ERROR;
    }
    free(text);
    *out = value;
    BT_TRACE_EXIT("data.read_json");
    return BT_DATA_READ_OK;
}

static void mkdir_parents_of(const char *path) {
    char buf[BT_PATH_MAX];
    snprintf(buf, sizeof buf, "%s", path);
    char *slash = strrchr(buf, '/');
    char *back = strrchr(buf, '\\');
    char *sep = slash > back ? slash : back;
    if (sep == NULL) {
        return;
    }
    *sep = '\0';
    if (buf[0] == '\0') {
        return;
    }
    for (char *p = buf + (buf[1] == ':' ? 3 : 1); *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            _mkdir(buf);
            *p = save;
        }
    }
    _mkdir(buf);
}

int bt_data_write_json_atomic(const char *path, const bt_json_value *value,
                              bt_paths_error *err) {
    BT_TRACE_ENTER("data.write_json_atomic");
    mkdir_parents_of(path);
    bt_strbuf sb;
    if (bt_strbuf_init(&sb, 256) != 0) {
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_SERIALIZE;
        snprintf(err->path, sizeof err->path, "%s", path);
        snprintf(err->detail, sizeof err->detail, "out of memory");
        BT_TRACE_FAIL("data.write_json_atomic", "strbuf init failed");
        return -1;
    }
    if (bt_json_serialize(value, &sb) != 0) {
        bt_strbuf_free(&sb);
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_SERIALIZE;
        snprintf(err->path, sizeof err->path, "%s", path);
        snprintf(err->detail, sizeof err->detail, "serialization failed");
        BT_TRACE_FAIL("data.write_json_atomic", "serialize failed");
        return -1;
    }
    if (bt_paths_write_atomic(path, sb.data, sb.len) != 0) {
        bt_strbuf_free(&sb);
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_WRITE;
        snprintf(err->path, sizeof err->path, "%s", path);
        snprintf(err->detail, sizeof err->detail, "%s", strerror(errno));
        BT_TRACE_FAIL("data.write_json_atomic", "write failed on %s", path);
        return -1;
    }
    bt_strbuf_free(&sb);
    BT_TRACE_EXIT("data.write_json_atomic");
    return 0;
}

static void entry_key(char *out, size_t out_len, const char *sheet_file,
                      const char *position) {
    snprintf(out, out_len, "%s/%s", sheet_file, position);
}

static int algs_insert_sorted(bt_user_algs *algs, const char *key, const char *alg) {
    if (algs->count == algs->cap) {
        size_t cap = algs->cap == 0 ? 8 : algs->cap * 2;
        bt_user_alg_entry *grown = realloc(algs->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        algs->items = grown;
        algs->cap = cap;
    }
    size_t at = algs->count;
    for (size_t i = 0; i < algs->count; i++) {
        int cmp = strcmp(algs->items[i].key, key);
        if (cmp == 0) {
            char *copy = dup_str(alg);
            if (copy == NULL) {
                return -1;
            }
            free(algs->items[i].alg);
            algs->items[i].alg = copy;
            return 0;
        }
        if (cmp > 0) {
            at = i;
            break;
        }
    }
    char *key_copy = dup_str(key);
    char *alg_copy = dup_str(alg);
    if (key_copy == NULL || alg_copy == NULL) {
        free(key_copy);
        free(alg_copy);
        return -1;
    }
    memmove(&algs->items[at + 1], &algs->items[at],
            (algs->count - at) * sizeof *algs->items);
    algs->items[at].key = key_copy;
    algs->items[at].alg = alg_copy;
    algs->count++;
    return 0;
}

void bt_user_algs_init(bt_user_algs *algs) {
    BT_TRACE_ENTER("data.user_algs_init");
    memset(algs, 0, sizeof *algs);
    BT_TRACE_EXIT("data.user_algs_init");
}

void bt_user_algs_free(bt_user_algs *algs) {
    BT_TRACE_ENTER("data.user_algs_free");
    for (size_t i = 0; i < algs->count; i++) {
        free(algs->items[i].key);
        free(algs->items[i].alg);
    }
    free(algs->items);
    memset(algs, 0, sizeof *algs);
    BT_TRACE_EXIT("data.user_algs_free");
}

int bt_user_algs_clone(const bt_user_algs *src, bt_user_algs *dst) {
    bt_user_algs_init(dst);
    for (size_t i = 0; i < src->count; i++) {
        if (algs_insert_sorted(dst, src->items[i].key, src->items[i].alg) != 0) {
            bt_user_algs_free(dst);
            return -1;
        }
    }
    return 0;
}

int bt_user_algs_eq(const bt_user_algs *a, const bt_user_algs *b) {
    if (a->count != b->count) {
        return 0;
    }
    for (size_t i = 0; i < a->count; i++) {
        if (strcmp(a->items[i].key, b->items[i].key) != 0 ||
            strcmp(a->items[i].alg, b->items[i].alg) != 0) {
            return 0;
        }
    }
    return 1;
}

void bt_user_algs_set(bt_user_algs *algs, const char *sheet_file, const char *position,
                      const char *alg) {
    BT_TRACE_ENTER("data.user_algs_set");
    char key[BT_PATH_MAX];
    entry_key(key, sizeof key, sheet_file, position);
    algs_insert_sorted(algs, key, alg);
    BT_TRACE_EXIT("data.user_algs_set");
}

int bt_user_algs_set_entry(bt_user_algs *algs, const char *key, const char *alg) {
    BT_TRACE_ENTER("data.user_algs_set_entry");
    int rc = algs_insert_sorted(algs, key, alg);
    if (rc != 0) {
        BT_TRACE_FAIL("data.user_algs_set_entry", "insert failed for %s", key);
        return -1;
    }
    BT_TRACE_EXIT("data.user_algs_set_entry");
    return 0;
}

const char *bt_user_algs_alg_for(const bt_user_algs *algs, const char *sheet_file,
                                 const char *position) {
    char key[BT_PATH_MAX];
    entry_key(key, sizeof key, sheet_file, position);
    for (size_t i = 0; i < algs->count; i++) {
        if (strcmp(algs->items[i].key, key) == 0) {
            return algs->items[i].alg;
        }
    }
    return NULL;
}

int bt_user_algs_remove(bt_user_algs *algs, const char *sheet_file,
                        const char *position) {
    char key[BT_PATH_MAX];
    entry_key(key, sizeof key, sheet_file, position);
    for (size_t i = 0; i < algs->count; i++) {
        if (strcmp(algs->items[i].key, key) == 0) {
            free(algs->items[i].key);
            free(algs->items[i].alg);
            memmove(&algs->items[i], &algs->items[i + 1],
                    (algs->count - i - 1) * sizeof *algs->items);
            algs->count--;
            return 1;
        }
    }
    return 0;
}

size_t bt_user_algs_len(const bt_user_algs *algs) {
    return algs->count;
}

int bt_user_algs_is_empty(const bt_user_algs *algs) {
    return algs->count == 0;
}

int bt_user_algs_save_at(const bt_user_algs *algs, const char *user_root,
                         bt_paths_error *err) {
    BT_TRACE_ENTER("data.user_algs_save_at");
    char path[BT_PATH_MAX];
    bt_data_user_algs_path_at(user_root, path);
    bt_json_value *root = bt_json_new_object();
    bt_json_value *entries = bt_json_new_object();
    int failed = 0;
    for (size_t i = 0; i < algs->count && failed == 0; i++) {
        bt_json_value *entry = bt_json_new_object();
        bt_json_value *alg = bt_json_new_string(algs->items[i].alg);
        if (bt_json_object_set(entry, "alg", alg) != 0) {
            failed = 1;
        } else if (bt_json_object_set(entries, algs->items[i].key, entry) != 0) {
            failed = 1;
        }
    }
    if (failed == 0 && bt_json_object_set(root, "entries", entries) != 0) {
        failed = 1;
    }
    if (failed != 0) {
        bt_json_free(root);
        bt_json_free(entries);
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_SERIALIZE;
        snprintf(err->path, sizeof err->path, "%s", path);
        snprintf(err->detail, sizeof err->detail, "tree build failed");
        BT_TRACE_FAIL("data.user_algs_save_at", "tree build failed");
        return -1;
    }
    int rc = bt_data_write_json_atomic(path, root, err);
    bt_json_free(root);
    if (rc != 0) {
        BT_TRACE_FAIL("data.user_algs_save_at", "write failed on %s", path);
        return -1;
    }
    BT_TRACE_EXIT("data.user_algs_save_at");
    return 0;
}

bt_data_read_status bt_user_algs_load_at(bt_user_algs *algs, const char *user_root,
                                         bt_paths_error *err) {
    BT_TRACE_ENTER("data.user_algs_load_at");
    bt_user_algs_init(algs);
    char path[BT_PATH_MAX];
    bt_data_user_algs_path_at(user_root, path);
    bt_json_value *root = NULL;
    bt_data_read_status read = bt_data_read_json(path, &root);
    if (read == BT_DATA_READ_MISSING) {
        BT_TRACE_EXIT("data.user_algs_load_at");
        return BT_DATA_READ_MISSING;
    }
    if (read != BT_DATA_READ_OK) {
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_DESERIALIZE;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("data.user_algs_load_at", "unreadable %s", path);
        return BT_DATA_READ_ERROR;
    }
    bt_json_value *entries = bt_json_get(root, "entries");
    if (entries == NULL || entries->type != BT_JSON_OBJECT) {
        bt_json_free(root);
        bt_user_algs_free(algs);
        memset(err, 0, sizeof *err);
        err->kind = BT_PATHS_ERR_DESERIALIZE;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("data.user_algs_load_at", "missing entries on %s", path);
        return BT_DATA_READ_ERROR;
    }
    for (size_t i = 0; i < entries->as.object.count; i++) {
        const char *key = entries->as.object.keys[i];
        bt_json_value *entry = entries->as.object.values[i];
        bt_json_value *alg = bt_json_get(entry, "alg");
        if (entry->type != BT_JSON_OBJECT || alg == NULL ||
            alg->type != BT_JSON_STRING) {
            bt_json_free(root);
            bt_user_algs_free(algs);
            memset(err, 0, sizeof *err);
            err->kind = BT_PATHS_ERR_DESERIALIZE;
            snprintf(err->path, sizeof err->path, "%s", path);
            BT_TRACE_FAIL("data.user_algs_load_at", "bad entry %s", key);
            return BT_DATA_READ_ERROR;
        }
        const char *slash = strchr(key, '/');
        (void)slash;
        if (algs_insert_sorted(algs, key, alg->as.string) != 0) {
            bt_json_free(root);
            bt_user_algs_free(algs);
            memset(err, 0, sizeof *err);
            err->kind = BT_PATHS_ERR_DESERIALIZE;
            snprintf(err->path, sizeof err->path, "%s", path);
            return BT_DATA_READ_ERROR;
        }
    }
    bt_json_free(root);
    BT_TRACE_EXIT("data.user_algs_load_at");
    return BT_DATA_READ_OK;
}
