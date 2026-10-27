#include "common/bt_harness.h"
#include "data/corpus.h"
#include "data/paths.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void read_dir_names(const char *dir, char names[8][64], size_t *count) {
    *count = 0;
    char pattern[BT_TU_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        return;
    }
    do {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        if (*count < 8) {
            snprintf(names[*count], 64, "%s", entry.name);
            (*count)++;
        }
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
}

BT_TEST(paths_layer, user_root_prefers_appdata_then_xdg_then_home) {
    char out[BT_PATH_MAX];
    bt_data_user_root_from("C:/Users/u/AppData/Roaming", NULL, NULL, out);
    BT_CHECK(strcmp(out, "C:/Users/u/AppData/Roaming/blind-trainer") == 0);
    bt_data_user_root_from(NULL, "/home/u/.local/share", NULL, out);
    BT_CHECK(strcmp(out, "/home/u/.local/share/blind-trainer") == 0);
    bt_data_user_root_from(NULL, NULL, "/home/u", out);
    BT_CHECK(strcmp(out, "/home/u/.local/share/blind-trainer") == 0);
}

BT_TEST(paths_layer, user_dirs_are_named_subdirectories) {
    char out[BT_PATH_MAX];
    bt_data_user_dir_path_at("/base", "algs", out);
    BT_CHECK(strcmp(out, "/base/algs") == 0);
    bt_data_user_dir_path_at("/base", "srs", out);
    BT_CHECK(strcmp(out, "/base/srs") == 0);
    bt_data_user_dir_path_at("/base", "solves", out);
    BT_CHECK(strcmp(out, "/base/solves") == 0);
    bt_data_user_dir_path_at("/base", "settings", out);
    BT_CHECK(strcmp(out, "/base/settings") == 0);
    bt_data_user_algs_path_at("/base", out);
    BT_CHECK(strcmp(out, "/base/algs/user-algs.json") == 0);
    bt_data_user_dir_path_at("/base", "unknown", out);
    BT_CHECK(out[0] == '\0');
}

BT_TEST(paths_layer, config_path_resolves_env_then_workspace_default) {
    char out[BT_PATH_MAX];
    bt_data_config_path_from("C:/etc/app.json", out);
    BT_CHECK(strcmp(out, "C:/etc/app.json") == 0);
    bt_data_config_path_from("config/config.json", out);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/config/config.json", root);
    BT_CHECK(strcmp(out, want) == 0);
    bt_data_config_path_from(NULL, out);
    size_t len = strlen(out);
    BT_CHECK(len >= 18);
    BT_CHECK(strcmp(out + len - 18, "config/config.json") == 0);
}

BT_TEST(paths_layer, atomic_write_creates_reads_and_replaces) {
    const char *dir = tu_temp_dir("bt-data-paths", "atomic");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/settings/state.json", dir);
    bt_json_value *first = bt_json_new_object();
    bt_json_object_set(first, "mode", bt_json_new_string("a"));
    bt_paths_error err;
    BT_CHECK(bt_data_write_json_atomic(target, first, &err) == 0);
    bt_json_value *read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    bt_json_value *mode = bt_json_get(read, "mode");
    BT_CHECK(mode != NULL && strcmp(mode->as.string, "a") == 0);
    bt_json_free(read);
    bt_json_object_set(first, "mode", bt_json_new_string("b"));
    BT_CHECK(bt_data_write_json_atomic(target, first, &err) == 0);
    bt_json_free(first);
    read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    mode = bt_json_get(read, "mode");
    BT_CHECK(mode != NULL && strcmp(mode->as.string, "b") == 0);
    bt_json_free(read);
    char names[8][64];
    size_t count = 0;
    char parent[BT_TU_PATH_MAX];
    snprintf(parent, sizeof parent, "%s/settings", dir);
    read_dir_names(parent, names, &count);
    BT_CHECK(count == 1);
    BT_CHECK_EQ(names[0], "state.json");
    tu_remove_tree(dir);
}

BT_TEST(paths_layer, failed_write_leaves_target_and_directory_untouched) {
    const char *dir = tu_temp_dir("bt-data-paths", "badwrite");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/state.json", dir);
    bt_json_value *value = bt_json_new_object();
    bt_json_object_set(value, "ok", bt_json_new_number(1));
    bt_paths_error err;
    BT_CHECK(bt_data_write_json_atomic(target, value, &err) == 0);
    bt_json_free(value);
    bt_json_value *read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    bt_json_value *ok = bt_json_get(read, "ok");
    BT_CHECK(ok != NULL && ok->as.number == 1);
    bt_json_free(read);
    value = bt_json_new_object();
    bt_json_object_set(value, "next", bt_json_new_number(2));
    char held[BT_TU_PATH_MAX];
    snprintf(held, sizeof held, "%s/held.json", dir);
    tu_mkdir_p(held);
    BT_CHECK(bt_data_write_json_atomic(held, value, &err) != 0);
    bt_json_free(value);
    read = NULL;
    BT_CHECK(bt_data_read_json(target, &read) == BT_DATA_READ_OK);
    ok = bt_json_get(read, "ok");
    BT_CHECK(ok != NULL && ok->as.number == 1);
    bt_json_free(read);
    char names[8][64];
    size_t count = 0;
    read_dir_names(dir, names, &count);
    BT_CHECK(count == 2);
    BT_CHECK((strcmp(names[0], "state.json") == 0 && strcmp(names[1], "held.json") == 0) ||
             (strcmp(names[0], "held.json") == 0 && strcmp(names[1], "state.json") == 0));
    tu_remove_tree(dir);
}

BT_TEST(paths_layer, read_json_reports_missing_as_none_and_corrupt_as_error) {
    const char *dir = tu_temp_dir("bt-data-paths", "readjson");
    char target[BT_TU_PATH_MAX];
    snprintf(target, sizeof target, "%s/x.json", dir);
    bt_json_value *value = NULL;
    BT_CHECK(bt_data_read_json(target, &value) == BT_DATA_READ_MISSING);
    tu_write_file(target, "{ not json");
    BT_CHECK(bt_data_read_json(target, &value) == BT_DATA_READ_ERROR);
    tu_remove_tree(dir);
}

static void td_vendor_root(char out[BT_PATH_MAX]) {
    const char *raw = getenv("BT_REF_GROUND");
    if (raw == NULL || raw[0] == '\0') {
        BT_ABORT("BT_REF_GROUND is unset: run tests through bt.py");
    }
    if (raw[0] == '/' || (raw[1] == ':' && (raw[2] == '\\' || raw[2] == '/'))) {
        snprintf(out, BT_PATH_MAX, "%s", raw);
        return;
    }
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s\\%s", root, raw);
}

static unsigned long long vendor_tsv_digest(const char *vendor) {
    unsigned long long digest = 1469598103934665603ULL;
    const char *solvers[2] = {"cherry", "eggins"};
    for (int s = 0; s < 2; s++) {
        char pattern[BT_TU_PATH_MAX];
        snprintf(pattern, sizeof pattern, "%s/%s/*.tsv", vendor, solvers[s]);
        WIN32_FIND_DATAA entry;
        HANDLE find = FindFirstFileA(pattern, &entry);
        if (find == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            char path[BT_TU_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s/%s", vendor, solvers[s], entry.cFileName);
            for (const char *p = path; *p != '\0'; p++) {
                digest ^= (unsigned char)*p;
                digest *= 1099511628211ULL;
            }
            FILE *file = fopen(path, "rb");
            if (file == NULL) {
                continue;
            }
            int ch;
            while ((ch = fgetc(file)) != EOF) {
                digest ^= (unsigned char)ch;
                digest *= 1099511628211ULL;
            }
            fclose(file);
        } while (FindNextFileA(find, &entry));
        FindClose(find);
    }
    return digest;
}

BT_TEST(paths_layer, user_algs_round_trip_through_the_user_layer_only) {
    const char *user = tu_temp_dir("bt-data-paths", "overlay");
    char vendor[BT_PATH_MAX];
    td_vendor_root(vendor);
    unsigned long long before = vendor_tsv_digest(vendor);
    bt_paths_error err;
    bt_user_algs overlay;
    BT_CHECK(bt_user_algs_load_at(&overlay, user, &err) == BT_DATA_READ_MISSING);
    BT_CHECK(bt_user_algs_is_empty(&overlay) == 1);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UL", "M2");
    bt_user_algs_set(&overlay, "ltct", "UF-UR, UBR-LUB[FUR]", "U R U' R'");
    BT_CHECK(bt_user_algs_save_at(&overlay, user, &err) == 0);
    char path[BT_TU_PATH_MAX];
    bt_data_user_algs_path_at(user, path);
    BT_CHECK(fopen(path, "rb") != NULL);
    BT_CHECK(strncmp(path, user, strlen(user)) == 0);
    BT_CHECK(strncmp(path, vendor, strlen(vendor)) != 0);
    bt_user_algs reloaded;
    BT_CHECK(bt_user_algs_load_at(&reloaded, user, &err) == BT_DATA_READ_OK);
    BT_CHECK_EQ(bt_user_algs_alg_for(&reloaded, "edge", "UF-UB-UL"), "M2");
    BT_CHECK_EQ(bt_user_algs_alg_for(&reloaded, "ltct", "UF-UR, UBR-LUB[FUR]"), "U R U' R'");
    BT_CHECK(bt_user_algs_len(&reloaded) == 2);
    BT_CHECK(bt_user_algs_remove(&reloaded, "edge", "UF-UB-UL") == 1);
    BT_CHECK(bt_user_algs_remove(&reloaded, "edge", "UF-UB-UL") == 0);
    BT_CHECK(bt_user_algs_len(&reloaded) == 1);
    bt_corpus corpus;
    bt_corpus_error cerr;
    BT_CHECK(bt_corpus_load(vendor, &corpus, &cerr) == 0);
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &reloaded, &merged, &oerr) == 0);
    BT_CHECK(bt_corpus_total_rows(&merged) == 1077 + 1);
    size_t indices[BT_CORPUS_INDEX_MAX];
    size_t group = bt_corpus_rows_for_position(&merged, BT_SHEET_LTCT, "UF-UR, UBR-LUB[FUR]",
                                               indices, BT_CORPUS_INDEX_MAX);
    BT_CHECK(group == 2);
    if (group == 2) {
        BT_CHECK(merged.rows[indices[0]].solver == BT_SOLVER_CHERRY);
        BT_CHECK(merged.rows[indices[1]].solver == BT_SOLVER_USER);
        BT_CHECK_EQ(merged.rows[indices[1]].alg_text, "U R U' R'");
    }
    bt_corpus_free(&merged);
    bt_user_algs_free(&reloaded);
    bt_user_algs fresh;
    BT_CHECK(bt_user_algs_load_at(&fresh, user, &err) == BT_DATA_READ_OK);
    BT_CHECK(bt_user_algs_remove(&fresh, "ltct", "UF-UR, UBR-LUB[FUR]") == 1);
    bt_user_algs_set(&fresh, "edge", "UF-UB-ZZ", "R2");
    bt_corpus merged_new;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &fresh, &merged_new, &oerr) == 0);
    BT_CHECK(bt_corpus_total_rows(&merged_new) == 1077 + 2);
    BT_CHECK(bt_corpus_rows_for_position(&merged_new, BT_SHEET_EDGE, "UF-UB-UL", indices,
                                         BT_CORPUS_INDEX_MAX) == 4);
    BT_CHECK(bt_corpus_rows_for_position(&merged_new, BT_SHEET_EDGE, "UF-UB-ZZ", indices,
                                         BT_CORPUS_INDEX_MAX) == 1);
    if (bt_corpus_rows_for_position(&merged_new, BT_SHEET_EDGE, "UF-UB-ZZ", indices,
                                    BT_CORPUS_INDEX_MAX) == 1) {
        BT_CHECK(merged_new.rows[indices[0]].solver == BT_SOLVER_USER);
    }
    bt_corpus_free(&merged_new);
    bt_user_algs_free(&fresh);
    bt_corpus_free(&corpus);
    BT_CHECK(vendor_tsv_digest(vendor) == before);
    tu_remove_tree(user);
}

BT_TEST(paths_layer, overlay_rejects_bad_algs_and_unknown_sheets) {
    const char *user = tu_temp_dir("bt-data-paths", "badalgs");
    bt_paths_error err;
    bt_user_algs overlay;
    BT_CHECK(bt_user_algs_load_at(&overlay, user, &err) == BT_DATA_READ_MISSING);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UL", "Q9");
    char vendor[BT_PATH_MAX];
    td_vendor_root(vendor);
    bt_corpus corpus;
    bt_corpus_error cerr;
    BT_CHECK(bt_corpus_load(vendor, &corpus, &cerr) == 0);
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) != 0);
    char text[512];
    bt_overlay_error_string(&oerr, text, sizeof text);
    BT_CHECK(strstr(text, "UF-UB-UL") != NULL);
    bt_user_algs_free(&overlay);
    bt_corpus_free(&corpus);
    tu_remove_tree(user);
}

BT_TEST(paths_layer, overlay_rows_get_unique_user_rowids_per_sheet) {
    const char *user = tu_temp_dir("bt-data-paths", "rowids");
    bt_paths_error err;
    bt_user_algs overlay;
    BT_CHECK(bt_user_algs_load_at(&overlay, user, &err) == BT_DATA_READ_MISSING);
    bt_user_algs_set(&overlay, "edge", "UF-UB-UL", "M2");
    bt_user_algs_set(&overlay, "edge", "UF-BL-BD", "E2");
    char vendor[BT_PATH_MAX];
    td_vendor_root(vendor);
    bt_corpus corpus;
    bt_corpus_error cerr;
    BT_CHECK(bt_corpus_load(vendor, &corpus, &cerr) == 0);
    bt_corpus merged;
    bt_overlay_error oerr;
    BT_CHECK(bt_corpus_with_overlay(&corpus, &overlay, &merged, &oerr) == 0);
    size_t indices[BT_CORPUS_INDEX_MAX];
    size_t user_edge =
        bt_corpus_sheet_rows(&merged, BT_SOLVER_USER, BT_SHEET_EDGE, indices, BT_CORPUS_INDEX_MAX);
    BT_CHECK(user_edge == 2);
    if (user_edge == 2) {
        BT_CHECK(merged.rows[indices[0]].rowid != merged.rows[indices[1]].rowid);
        for (size_t i = 0; i < 2; i++) {
            BT_CHECK(bt_corpus_row_at(&merged, BT_SOLVER_USER, BT_SHEET_EDGE,
                                      merged.rows[indices[i]].rowid) != NULL);
        }
    }
    bt_corpus_free(&merged);
    bt_user_algs_free(&overlay);
    bt_corpus_free(&corpus);
    tu_remove_tree(user);
}
