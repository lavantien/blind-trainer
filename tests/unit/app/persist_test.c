#include "app/persist.h"

#include "common.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SENTINEL_CADENCE_MS = 640,
};

#define PACK_SOURCE_URL "https://example.test/pack"
#define PACK_SOURCE_AT "2026-10-16"
#define PACK_FRONT_JSON "{\"case\":\"x\"}"

static const bt_fsrs_config *fsrs_cfg(void) {
    static bt_fsrs_config cfg;
    static int ready = 0;
    if (!ready) {
        bt_fsrs_config_defaults(&cfg);
        ready = 1;
    }
    return &cfg;
}

static bt_settings sample_settings(void) {
    bt_settings settings;
    settings.memo_cadence_ms = 900;
    settings.memo_mode = BT_MEMO_IMAGES;
    settings.drill_reps = 7;
    return settings;
}

static bt_settings one_settings(void) {
    bt_settings settings;
    settings.memo_cadence_ms = 1;
    settings.memo_mode = BT_MEMO_SYLLABLES;
    settings.drill_reps = 1;
    return settings;
}

static int settings_eq(bt_settings a, bt_settings b) {
    return a.memo_cadence_ms == b.memo_cadence_ms && a.memo_mode == b.memo_mode &&
           a.drill_reps == b.drill_reps;
}

static int hidden_equals(const bt_seed_repo *repo, const char *const *wanted, size_t count) {
    if (repo->hidden.count != count) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(repo->hidden.items[i], wanted[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

static char *registry_text(const char *(*rows)[5], size_t count) {
    char *text = calloc(count * 256 + 3, 1);
    strcat(text, "[");
    for (size_t i = 0; i < count; i++) {
        char row[256];
        snprintf(row, sizeof row,
                 "{\"file\":\"%s\",\"event\":\"%s\",\"method\":\"%s\",\"stage\":\"%s\","
                 "\"n\":null,\"expected_rows\":%s}%s",
                 rows[i][0], rows[i][1], rows[i][2], rows[i][3], rows[i][4],
                 i + 1 == count ? "" : ",");
        strcat(text, row);
    }
    strcat(text, "]");
    return text;
}

static char *pack_text(const char *(*rows)[2], size_t count) {
    char *text = calloc(count * 256 + 64, 1);
    strcat(text, "case_key\talg\tfront_json\tsource_url\tsource_at\n");
    for (size_t i = 0; i < count; i++) {
        char row[256];
        snprintf(row, sizeof row, "%s\t%s\t%s\t%s\t%s\n", rows[i][0], rows[i][1], PACK_FRONT_JSON,
                 PACK_SOURCE_URL, PACK_SOURCE_AT);
        strcat(text, row);
    }
    return text;
}

static const char *write_packs(const char *root, const char *registry, const char *file,
                               const char *text) {
    static char packs[TA_PATH_MAX];
    snprintf(packs, sizeof packs, "%s/packs", root);
    ta_mkdir_p(packs);
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/registry.json", packs);
    ta_write_file(path, registry);
    snprintf(path, sizeof path, "%s/%s", packs, file);
    ta_write_file(path, text);
    return packs;
}

BT_TEST(persist, missing_files_fall_back_to_defaults) {
    const char *root = ta_temp_dir("persist", "defaults");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    int rc = bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(settings_eq(stores.settings, sample_settings()));
    BT_CHECK_EQ(bt_srs_store_len(&stores.srs), 0);
    BT_CHECK_EQ(stores.solves.count, 0);
    BT_CHECK(bt_user_algs_is_empty(&stores.user_algs));
    bt_stores_free(&stores);
}

BT_TEST(persist, settings_round_trip) {
    const char *root = ta_temp_dir("persist", "settings");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error),
                0);
    BT_CHECK_EQ(bt_app_save_settings(root, &stores.settings, error, sizeof error), 0);
    bt_stores_free(&stores);
    bt_stores reloaded;
    BT_CHECK_EQ(bt_stores_load(root, one_settings(), fsrs_cfg(), &reloaded, error, sizeof error),
                0);
    BT_CHECK(settings_eq(reloaded.settings, sample_settings()));
    bt_stores_free(&reloaded);
}

BT_TEST(persist, memo_mode_per_orbit_defaults_and_round_trips) {
    BT_CHECK_EQ(BT_MEMO_PER_ORBIT, BT_MEMO_PER_ORBIT);
    BT_CHECK_EQ(strcmp(bt_memo_mode_label(BT_MEMO_PER_ORBIT), "per-orbit"), 0);
    bt_settings settings = {SENTINEL_CADENCE_MS, BT_MEMO_PER_ORBIT, 6};
    bt_json_value *json = bt_settings_to_json(&settings);
    bt_strbuf text;
    bt_strbuf_init(&text, 64);
    BT_CHECK_EQ(bt_json_serialize(json, &text), 0);
    BT_CHECK(strstr(text.data, "\"per-orbit\"") != NULL);
    bt_settings back;
    char error[96];
    BT_CHECK_EQ(bt_settings_from_json(json, &back, error, sizeof error), 0);
    BT_CHECK(settings_eq(back, settings));
    BT_CHECK_EQ(back.drill_reps, 6);
    bt_strbuf_free(&text);
    bt_json_free(json);
}

BT_TEST(persist, memo_mode_legacy_values_still_parse) {
    const char *texts[3] = {"syllables", "images", "per-orbit"};
    const bt_memo_mode modes[3] = {BT_MEMO_SYLLABLES, BT_MEMO_IMAGES, BT_MEMO_PER_ORBIT};
    for (int i = 0; i < 3; i++) {
        bt_memo_mode mode;
        BT_CHECK_EQ(bt_memo_mode_parse(texts[i], &mode), 1);
        BT_CHECK_EQ(mode, modes[i]);
    }
    bt_memo_mode mode;
    BT_CHECK_EQ(bt_memo_mode_parse("perorbit", &mode), 0);
    BT_CHECK_EQ(bt_memo_mode_parse("", &mode), 0);
}

BT_TEST(persist, stored_per_orbit_settings_load_verbatim) {
    const char *root = ta_temp_dir("persist", "per-orbit-stored");
    bt_settings settings = {SENTINEL_CADENCE_MS, BT_MEMO_PER_ORBIT, 6};
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_save_settings(root, &settings, error, sizeof error), 0);
    bt_stores stores;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error),
                0);
    BT_CHECK_EQ(stores.settings.memo_mode, BT_MEMO_PER_ORBIT);
    BT_CHECK_EQ(stores.settings.memo_cadence_ms, SENTINEL_CADENCE_MS);
    BT_CHECK_EQ(stores.settings.drill_reps, 6);
    bt_stores_free(&stores);
}

BT_TEST(persist, srs_and_solves_round_trip) {
    const char *root = ta_temp_dir("persist", "srs");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error),
                0);
    bt_card card;
    memset(&card, 0, sizeof card);
    card.interval_days = 6;
    card.due_day = 10;
    card.reps = 2;
    card.lapses = 0;
    BT_CHECK_EQ(bt_srs_store_set(&stores.srs, "edge/UF", &card), 0);
    bt_solve_record record = {5, 40000, 25000, 1};
    BT_CHECK_EQ(bt_solve_log_push(&stores.solves, record), 0);
    BT_CHECK_EQ(bt_app_save_srs(root, &stores.srs, error, sizeof error), 0);
    BT_CHECK_EQ(bt_app_save_solves(root, &stores.solves, error, sizeof error), 0);
    bt_stores_free(&stores);
    bt_stores reloaded;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &reloaded, error, sizeof error),
                0);
    BT_CHECK_EQ(bt_srs_store_len(&reloaded.srs), 1);
    const bt_card *stored = bt_srs_store_get(&reloaded.srs, "edge/UF");
    BT_CHECK(stored != NULL);
    BT_CHECK_EQ(stored->interval_days, 6);
    BT_CHECK_EQ(reloaded.solves.count, 1);
    BT_CHECK_EQ(reloaded.solves.items[0].memo_ms, 40000);
    bt_stores_free(&reloaded);
}

BT_TEST(persist, corrupt_db_fails_closed_naming_the_file) {
    const char *root = ta_temp_dir("persist", "corrupt-db");
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    ta_write_file(db_path, "definitely not a sqlite database");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    int rc = bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(error, "user_version") != NULL);
}

BT_TEST(persist, corrupt_settings_row_fails_closed_naming_the_db) {
    const char *root = ta_temp_dir("persist", "corrupt-row");
    bt_db db;
    bt_db_error db_err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    BT_CHECK_EQ(bt_db_open_at(db_path, &db, &db_err), 0);
    BT_CHECK_EQ(bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, "{not json", &db_err), 0);
    bt_db_close(&db);
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    int rc = bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(error, "settings") != NULL);
    BT_CHECK(strstr(error, "blind-trainer.db") != NULL);
}

BT_TEST(persist, corrupt_legacy_json_fails_closed_naming_the_file) {
    const char *root = ta_temp_dir("persist", "corrupt-legacy");
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/settings/settings.json", root);
    ta_write_file(path, "{not json");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    int rc = bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error);
    BT_CHECK_EQ(rc, -1);
    BT_CHECK(strstr(error, "settings.json") != NULL);
}

BT_TEST(persist, memo_mode_index_round_trips_through_the_label_order) {
    for (int index = 0; index < BT_MEMO_MODE_COUNT; index++) {
        bt_memo_mode mode;
        BT_CHECK_EQ(bt_memo_mode_from_index(index, &mode), 1);
        BT_CHECK_EQ(bt_memo_mode_index(mode), index);
    }
    bt_memo_mode mode;
    BT_CHECK_EQ(bt_memo_mode_from_index(BT_MEMO_MODE_COUNT, &mode), 0);
}

BT_TEST(persist, every_store_saves_into_the_db) {
    const char *root = ta_temp_dir("persist", "db-file");
    bt_settings settings = sample_settings();
    bt_srs_store srs;
    bt_srs_store_init(&srs);
    bt_solve_log solves;
    bt_solve_log_init(&solves);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_save_settings(root, &settings, error, sizeof error), 0);
    BT_CHECK_EQ(bt_app_save_srs(root, &srs, error, sizeof error), 0);
    BT_CHECK_EQ(bt_app_save_solves(root, &solves, error, sizeof error), 0);
    BT_CHECK_EQ(bt_app_save_user_algs(root, &algs, error, sizeof error), 0);
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(root, db_path);
    struct _stat st;
    BT_CHECK_EQ(_stat(db_path, &st), 0);
    char dir[TA_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/settings", root);
    BT_CHECK(_stat(dir, &st) != 0);
    snprintf(dir, sizeof dir, "%s/srs", root);
    BT_CHECK(_stat(dir, &st) != 0);
    snprintf(dir, sizeof dir, "%s/solves", root);
    BT_CHECK(_stat(dir, &st) != 0);
    snprintf(dir, sizeof dir, "%s/algs", root);
    BT_CHECK(_stat(dir, &st) != 0);
    bt_srs_store_free(&srs);
    bt_solve_log_free(&solves);
    bt_user_algs_free(&algs);
}

BT_TEST(persist, user_algs_round_trip_through_the_store) {
    const char *root = ta_temp_dir("persist", "algs-roundtrip");
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK_EQ(bt_user_algs_set_entry(&algs, "edge/UF-UB-UR", "R2 U' S R2 S' R2 U R2"), 0);
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_save_user_algs(root, &algs, error, sizeof error), 0);
    bt_user_algs_free(&algs);
    bt_stores stores;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error),
                0);
    BT_CHECK_EQ(bt_user_algs_len(&stores.user_algs), 1);
    BT_CHECK_EQ(strcmp(bt_user_algs_alg_for(&stores.user_algs, "edge", "UF-UB-UR"),
                       "R2 U' S R2 S' R2 U R2"),
                0);
    bt_stores_free(&stores);
}

BT_TEST(persist, legacy_stores_import_once_then_the_db_drives_loads) {
    const char *root = ta_temp_dir("persist", "legacy-import");
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/settings/settings.json", root);
    ta_write_file(path, "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\","
                        "\"drill_reps\":6}");
    snprintf(path, sizeof path, "%s/algs/user-algs.json", root);
    ta_write_file(path, "{\"entries\":{\"corner/URB\":{\"alg\":\"R U' R'\"}}}");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &stores, error, sizeof error),
                0);
    BT_CHECK_EQ(stores.settings.memo_cadence_ms, SENTINEL_CADENCE_MS);
    BT_CHECK_EQ(bt_user_algs_len(&stores.user_algs), 1);
    bt_stores_free(&stores);
    snprintf(path, sizeof path, "%s/settings/settings.json.migrated", root);
    struct _stat st;
    BT_CHECK_EQ(_stat(path, &st), 0);
    snprintf(path, sizeof path, "%s/algs/user-algs.json.migrated", root);
    BT_CHECK_EQ(_stat(path, &st), 0);
    snprintf(path, sizeof path, "%s/settings/settings.json", root);
    ta_write_file(path, "{\"memo_cadence_ms\":1,\"memo_mode\":\"images\",\"drill_reps\":1}");
    bt_stores again;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &again, error, sizeof error),
                0);
    BT_CHECK_EQ(again.settings.memo_cadence_ms, SENTINEL_CADENCE_MS);
    bt_stores_free(&again);
    bt_settings wanted = sample_settings();
    BT_CHECK_EQ(bt_app_save_settings(root, &wanted, error, sizeof error), 0);
    bt_stores third;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &third, error, sizeof error),
                0);
    BT_CHECK(settings_eq(third.settings, wanted));
    bt_stores_free(&third);
}

BT_TEST(persist, saving_into_a_file_root_fails_loudly) {
    char holder[TA_PATH_MAX];
    snprintf(holder, sizeof holder, "%s/blocker", ta_temp_dir("persist", "file-root"));
    ta_write_file(holder, "occupied");
    bt_settings settings = sample_settings();
    bt_srs_store srs;
    bt_srs_store_init(&srs);
    bt_solve_log solves;
    bt_solve_log_init(&solves);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_app_save_settings(holder, &settings, error, sizeof error), -1);
    BT_CHECK_EQ(bt_app_save_srs(holder, &srs, error, sizeof error), -1);
    BT_CHECK_EQ(bt_app_save_solves(holder, &solves, error, sizeof error), -1);
    BT_CHECK_EQ(bt_app_save_user_algs(holder, &algs, error, sizeof error), -1);
    bt_srs_store_free(&srs);
    bt_solve_log_free(&solves);
    bt_user_algs_free(&algs);
}

BT_TEST(persist, settings_json_uses_the_documented_field_names) {
    bt_json_value *json = bt_settings_to_json(&(bt_settings){900, BT_MEMO_IMAGES, 7});
    BT_CHECK_EQ(bt_json_integer_or(bt_json_get(json, "memo_cadence_ms"), -1), 900);
    const bt_json_value *mode = bt_json_get(json, "memo_mode");
    BT_CHECK(mode != NULL && mode->type == BT_JSON_STRING);
    BT_CHECK_EQ(strcmp(mode->as.string, "images"), 0);
    BT_CHECK_EQ(bt_json_integer_or(bt_json_get(json, "drill_reps"), -1), 7);
    bt_json_free(json);
}

BT_TEST(persist, seed_repository_seeds_rows_and_verifies_on_reload) {
    const char *root = ta_temp_dir("persist", "seed-ok");
    const char *registry_one[1][5] = {{"good.tsv", "2x2", "cll", "cll", "2"}};
    char *registry = registry_text(registry_one, 1);
    const char *pack_two[2][2] = {{"cll.1", "R U R'"}, {"cll.2", "F R F'"}};
    char *pack = pack_text(pack_two, 2);
    const char *packs = write_packs(root, registry, "good.tsv", pack);
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &stores,
                                          error, sizeof error),
                0);
    BT_CHECK_EQ(stores.seeds.rows.count, 2);
    BT_CHECK_EQ(strcmp(stores.seeds.rows.items[0].case_key, "cll.1"), 0);
    BT_CHECK_EQ(stores.seeds.pack_count, 1);
    BT_CHECK_EQ(stores.seeds.hidden.count, 0);
    BT_CHECK_EQ(stores.seeds.warnings.count, 0);
    bt_stores_free(&stores);
    bt_stores reload;
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &reload,
                                          error, sizeof error),
                0);
    BT_CHECK_EQ(reload.seeds.rows.count, 2);
    BT_CHECK_EQ(reload.seeds.pack_count, 1);
    BT_CHECK_EQ(reload.seeds.warnings.count, 0);
    bt_stores_free(&reload);
    free(registry);
    free(pack);
}

BT_TEST(persist, bad_pack_hides_its_event_and_warns) {
    const char *root = ta_temp_dir("persist", "seed-bad");
    const char *registry_two[2][5] = {
        {"good.tsv", "2x2", "cll", "cll", "1"},
        {"bad.tsv", "skewb", "sarah", "sarah", "1"},
    };
    char *registry = registry_text(registry_two, 2);
    const char *pack_one[1][2] = {{"cll.1", "R U R'"}};
    char *good = pack_text(pack_one, 1);
    const char *packs = write_packs(root, registry, "good.tsv", good);
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/bad.tsv", packs);
    ta_write_file(path, "wrong\theader\n");
    bt_stores stores;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &stores,
                                          error, sizeof error),
                0);
    BT_CHECK_EQ(stores.seeds.rows.count, 1);
    BT_CHECK_EQ(strcmp(stores.seeds.rows.items[0].event, "2x2"), 0);
    BT_CHECK_EQ(stores.seeds.pack_count, 1);
    const char *hidden[1] = {"skewb"};
    BT_CHECK(hidden_equals(&stores.seeds, hidden, 1));
    BT_CHECK_EQ(stores.seeds.warnings.count, 1);
    BT_CHECK(strstr(stores.seeds.warnings.items[0], "bad.tsv") != NULL);
    BT_CHECK(strstr(stores.seeds.warnings.items[0], "header") != NULL);
    bt_stores_free(&stores);
    free(registry);
    free(good);
}

BT_TEST(persist, seed_drift_hides_the_event_and_warns) {
    const char *root = ta_temp_dir("persist", "seed-drift");
    const char *registry_one[1][5] = {{"good.tsv", "fto", "l4t", "l4t", "1"}};
    char *registry = registry_text(registry_one, 1);
    const char *pack_one[1][2] = {{"l4t.1", "R U R'"}};
    char *pack = pack_text(pack_one, 1);
    const char *packs = write_packs(root, registry, "good.tsv", pack);
    bt_stores seeded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &seeded,
                                          error, sizeof error),
                0);
    bt_stores_free(&seeded);
    const char *pack_drift[1][2] = {{"l4t.1", "F R F'"}};
    char *drifted_pack = pack_text(pack_drift, 1);
    write_packs(root, registry, "good.tsv", drifted_pack);
    bt_stores drifted;
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &drifted,
                                          error, sizeof error),
                0);
    const char *hidden[1] = {"fto"};
    BT_CHECK(hidden_equals(&drifted.seeds, hidden, 1));
    BT_CHECK_EQ(drifted.seeds.warnings.count, 1);
    BT_CHECK(strstr(drifted.seeds.warnings.items[0], "drift") != NULL);
    BT_CHECK_EQ(strcmp(drifted.seeds.rows.items[0].alg, "R U R'"), 0);
    bt_stores_free(&drifted);
    free(registry);
    free(pack);
    free(drifted_pack);
}

BT_TEST(persist, stale_pack_row_hides_the_event_and_warns) {
    const char *root = ta_temp_dir("persist", "seed-stale");
    const char *registry_two[1][5] = {{"good.tsv", "pyraminx", "l4e", "l4e", "2"}};
    char *registry = registry_text(registry_two, 1);
    const char *pack_two[2][2] = {{"l4e.1", "R U R'"}, {"l4e.2", "F R F'"}};
    char *pack = pack_text(pack_two, 2);
    const char *packs = write_packs(root, registry, "good.tsv", pack);
    bt_stores seeded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &seeded,
                                          error, sizeof error),
                0);
    bt_stores_free(&seeded);
    const char *registry_one[1][5] = {{"good.tsv", "pyraminx", "l4e", "l4e", "1"}};
    char *registry_fewer = registry_text(registry_one, 1);
    const char *pack_one[1][2] = {{"l4e.1", "R U R'"}};
    char *pack_fewer = pack_text(pack_one, 1);
    write_packs(root, registry_fewer, "good.tsv", pack_fewer);
    bt_stores stale;
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &stale, error,
                                          sizeof error),
                0);
    const char *hidden[1] = {"pyraminx"};
    BT_CHECK(hidden_equals(&stale.seeds, hidden, 1));
    BT_CHECK_EQ(stale.seeds.warnings.count, 1);
    BT_CHECK(strstr(stale.seeds.warnings.items[0], "l4e.2") != NULL);
    bt_stores_free(&stale);
    free(registry);
    free(pack);
    free(registry_fewer);
    free(pack_fewer);
}

BT_TEST(persist, broken_registry_hides_every_seed_event) {
    const char *root = ta_temp_dir("persist", "seed-registry");
    const char *registry_one[1][5] = {{"good.tsv", "2x2", "cll", "cll", "1"}};
    char *registry = registry_text(registry_one, 1);
    const char *pack_one[1][2] = {{"cll.1", "R U R'"}};
    char *pack = pack_text(pack_one, 1);
    const char *packs = write_packs(root, registry, "good.tsv", pack);
    bt_stores seeded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &seeded,
                                          error, sizeof error),
                0);
    bt_stores_free(&seeded);
    char path[TA_PATH_MAX];
    snprintf(path, sizeof path, "%s/registry.json", packs);
    ta_write_file(path, "{not json");
    bt_stores broken;
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &broken,
                                          error, sizeof error),
                0);
    BT_CHECK_EQ(broken.seeds.rows.count, 1);
    BT_CHECK_EQ(broken.seeds.pack_count, 0);
    const char *hidden[1] = {"2x2"};
    BT_CHECK(hidden_equals(&broken.seeds, hidden, 1));
    BT_CHECK_EQ(broken.seeds.warnings.count, 1);
    BT_CHECK(strstr(broken.seeds.warnings.items[0], "seed registry") != NULL);
    bt_stores_free(&broken);
    free(registry);
    free(pack);
}

BT_TEST(persist, missing_packs_root_keeps_stored_rows_quiet) {
    const char *root = ta_temp_dir("persist", "seed-none");
    const char *registry_one[1][5] = {{"good.tsv", "2x2", "cll", "cll", "1"}};
    char *registry = registry_text(registry_one, 1);
    const char *pack_one[1][2] = {{"cll.1", "R U R'"}};
    char *pack = pack_text(pack_one, 1);
    const char *packs = write_packs(root, registry, "good.tsv", pack);
    bt_stores seeded;
    char error[BT_APP_ERROR_MAX];
    BT_CHECK_EQ(bt_stores_load_with_packs(root, sample_settings(), fsrs_cfg(), packs, &seeded,
                                          error, sizeof error),
                0);
    bt_stores_free(&seeded);
    bt_stores quiet;
    BT_CHECK_EQ(bt_stores_load(root, sample_settings(), fsrs_cfg(), &quiet, error, sizeof error),
                0);
    BT_CHECK_EQ(quiet.seeds.rows.count, 1);
    BT_CHECK_EQ(quiet.seeds.pack_count, 0);
    BT_CHECK_EQ(quiet.seeds.hidden.count, 0);
    BT_CHECK_EQ(quiet.seeds.warnings.count, 0);
    bt_stores_free(&quiet);
    free(registry);
    free(pack);
}
