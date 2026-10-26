#include "data/import.h"

#include "common/bt_trace.h"
#include "data/paths.h"
#include "data/repo_algs.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_import_migrated_path(const char *path, char *out, size_t out_len) {
    snprintf(out, out_len, "%s.migrated", path);
}

static int file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int read_bytes(const char *path, char **out, size_t *out_len, int *missing, char *error,
                      size_t error_len) {
    *out = NULL;
    *out_len = 0;
    *missing = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            *missing = 1;
            return 0;
        }
        snprintf(error, error_len, "read %s: %s", path, strerror(errno));
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        snprintf(error, error_len, "read %s: out of memory", path);
        return -1;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = '\0';
    *out = text;
    *out_len = read;
    return 0;
}

static int finish_renames(char (*files)[BT_PATH_MAX], size_t count, char *error, size_t error_len) {
    for (size_t i = 0; i < count; i++) {
        if (!file_exists(files[i])) {
            continue;
        }
        char migrated[BT_PATH_MAX];
        bt_import_migrated_path(files[i], migrated, sizeof migrated);
        if (rename(files[i], migrated) != 0) {
            snprintf(error, error_len,
                     "rename %s: %s: the import already committed to the db, finish by "
                     "renaming it to %s or removing it",
                     files[i], strerror(errno), migrated);
            return -1;
        }
    }
    return 0;
}

int bt_import_run(bt_db *db, const char *root, char *error, size_t error_len) {
    BT_TRACE_ENTER("import.run");
    char files[4][BT_PATH_MAX];
    bt_data_settings_path_at(root, files[0]);
    bt_data_srs_path_at(root, files[1]);
    bt_data_solves_path_at(root, files[2]);
    bt_data_user_algs_path_at(root, files[3]);
    int any = 0;
    for (int i = 0; i < 4; i++) {
        if (file_exists(files[i])) {
            any = 1;
        }
    }
    if (!any) {
        BT_TRACE_EXIT("import.run");
        return 0;
    }
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    bt_db_error db_err;
    int fresh = 0;
    if (bt_db_is_fresh(db, &fresh, &db_err) != 0) {
        char text[512];
        bt_db_error_string(&db_err, text, sizeof text);
        snprintf(error, error_len, "%s: %s", db_path, text);
        BT_TRACE_FAIL("import.run", "is_fresh failed");
        return -1;
    }
    if (!fresh) {
        if (finish_renames(files, 4, error, error_len) != 0) {
            BT_TRACE_FAIL("import.run", "rename residue failed");
            return -1;
        }
        BT_TRACE_EXIT("import.run");
        return 0;
    }
    char *settings_text = NULL;
    size_t settings_len = 0;
    int settings_missing = 0;
    if (read_bytes(files[0], &settings_text, &settings_len, &settings_missing, error, error_len) !=
        0) {
        BT_TRACE_FAIL("import.run", "settings read failed");
        return -1;
    }
    int has_settings = !settings_missing;
    if (has_settings) {
        bt_json_value *probe = NULL;
        bt_json_error jerr;
        if (bt_json_parse(settings_text, settings_len, &probe, &jerr) != 0) {
            free(settings_text);
            snprintf(error, error_len, "%s: malformed json at line %zu column %zu", files[0],
                     jerr.line, jerr.column);
            BT_TRACE_FAIL("import.run", "settings json invalid");
            return -1;
        }
        bt_json_free(probe);
    }
    bt_json_value *srs_root = NULL;
    bt_json_value *solves_root = NULL;
    bt_json_value *algs_root = NULL;
    bt_json_error jerr;
    bt_srs_store srs_store;
    bt_solve_log solve_log;
    bt_user_algs user_algs;
    int has_srs = 0;
    int has_solves = 0;
    int has_algs = 0;
    int failed = 0;
    char *text = NULL;
    size_t text_len = 0;
    int missing = 0;
    if (!failed && read_bytes(files[1], &text, &text_len, &missing, error, error_len) != 0) {
        failed = 1;
    }
    if (!failed && !missing) {
        if (bt_json_parse(text, text_len, &srs_root, &jerr) != 0 ||
            bt_srs_store_from_json(srs_root, &srs_store) != 0) {
            snprintf(error, error_len, "%s: srs store json invalid", files[1]);
            failed = 1;
        } else {
            has_srs = 1;
        }
        free(text);
        text = NULL;
    }
    if (!failed && read_bytes(files[2], &text, &text_len, &missing, error, error_len) != 0) {
        failed = 1;
    }
    if (!failed && !missing) {
        if (bt_json_parse(text, text_len, &solves_root, &jerr) != 0 ||
            bt_solve_log_from_json(solves_root, &solve_log) != 0) {
            snprintf(error, error_len, "%s: solve log json invalid", files[2]);
            failed = 1;
        } else {
            has_solves = 1;
        }
        free(text);
        text = NULL;
    }
    if (!failed && read_bytes(files[3], &text, &text_len, &missing, error, error_len) != 0) {
        failed = 1;
    }
    if (!failed && !missing) {
        bt_paths_error perr;
        if (bt_json_parse(text, text_len, &algs_root, &jerr) != 0 ||
            bt_user_algs_from_json(algs_root, &user_algs, &perr) != 0) {
            snprintf(error, error_len, "%s: user algs json invalid", files[3]);
            failed = 1;
        } else {
            has_algs = 1;
        }
        free(text);
        text = NULL;
    }
    if (failed) {
        free(settings_text);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        if (has_srs) {
            bt_srs_store_free(&srs_store);
        }
        if (has_solves) {
            bt_solve_log_free(&solve_log);
        }
        if (has_algs) {
            bt_user_algs_free(&user_algs);
        }
        BT_TRACE_FAIL("import.run", "legacy parse failed");
        return -1;
    }
    if (bt_db_begin(db, &db_err) != 0) {
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        free(settings_text);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        BT_TRACE_FAIL("import.run", "begin failed");
        return -1;
    }
    if (has_settings &&
        bt_repo_settings_save_tx(db, BT_REPO_SETTINGS_APP_KEY, settings_text, &db_err) != 0) {
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        bt_db_rollback(db);
        free(settings_text);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        BT_TRACE_FAIL("import.run", "settings save failed");
        return -1;
    }
    free(settings_text);
    if (has_srs && bt_repo_srs_save_tx(db, &srs_store, &db_err) != 0) {
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        bt_db_rollback(db);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        bt_srs_store_free(&srs_store);
        BT_TRACE_FAIL("import.run", "srs save failed");
        return -1;
    }
    if (has_srs) {
        bt_srs_store_free(&srs_store);
    }
    if (has_solves && bt_solve_log_save_tx(db, &solve_log, &db_err) != 0) {
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        bt_db_rollback(db);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        bt_solve_log_free(&solve_log);
        BT_TRACE_FAIL("import.run", "solves save failed");
        return -1;
    }
    if (has_solves) {
        bt_solve_log_free(&solve_log);
    }
    if (has_algs && bt_repo_algs_save_tx(db, &user_algs, &db_err) != 0) {
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        bt_db_rollback(db);
        bt_json_free(srs_root);
        bt_json_free(solves_root);
        bt_json_free(algs_root);
        bt_user_algs_free(&user_algs);
        BT_TRACE_FAIL("import.run", "algs save failed");
        return -1;
    }
    if (has_algs) {
        bt_user_algs_free(&user_algs);
    }
    bt_json_free(srs_root);
    bt_json_free(solves_root);
    bt_json_free(algs_root);
    if (bt_db_commit(db, &db_err) != 0) {
        db_err.kind = BT_DB_ERR_EXEC;
        snprintf(db_err.step, sizeof db_err.step, "%s", "commit import");
        char text2[512];
        bt_db_error_string(&db_err, text2, sizeof text2);
        snprintf(error, error_len, "%s: %s", db_path, text2);
        bt_db_rollback(db);
        BT_TRACE_FAIL("import.run", "commit failed");
        return -1;
    }
    if (finish_renames(files, 4, error, error_len) != 0) {
        BT_TRACE_FAIL("import.run", "rename failed");
        return -1;
    }
    BT_TRACE_EXIT("import.run");
    return 0;
}
