#include "app/views/state.h"

#include "app/config.h"
#include "common/bt_trace.h"
#include "common/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void bt_syllable_book_init(bt_syllable_book *book) {
    BT_TRACE_ENTER("app.views.book_init");
    book->items = NULL;
    book->count = 0;
    book->loaded = 0;
    BT_TRACE_EXIT("app.views.book_init");
}

void bt_syllable_book_free(bt_syllable_book *book) {
    BT_TRACE_ENTER("app.views.book_free");
    for (size_t i = 0; i < book->count; i++) {
        free(book->items[i].syllable);
    }
    free(book->items);
    book->items = NULL;
    book->count = 0;
    book->loaded = 0;
    BT_TRACE_EXIT("app.views.book_free");
}

const char *bt_syllable_book_for(const bt_syllable_book *book, const char *orbit,
                                 const char letters[2]) {
    BT_TRACE_ENTER("app.views.book_for");
    for (size_t i = 0; i < book->count; i++) {
        if (strcmp(book->items[i].orbit, orbit) == 0 &&
            strncmp(book->items[i].letters, letters, 2) == 0) {
            BT_TRACE_EXIT("app.views.book_for");
            return book->items[i].syllable;
        }
    }
    BT_TRACE_EXIT("app.views.book_for");
    return NULL;
}

void bt_app_views_init(bt_app_views *views) {
    BT_TRACE_ENTER("app.views_init");
    bt_syllable_book_init(&views->syllables);
    memset(&views->net, 0, sizeof views->net);
    BT_TRACE_EXIT("app.views_init");
}

void bt_app_views_free(bt_app_views *views) {
    BT_TRACE_ENTER("app.views_free");
    bt_syllable_book_free(&views->syllables);
    BT_TRACE_EXIT("app.views_free");
}

static bt_app_views *shared_views;

bt_app_views *bt_app_views_shared(void) {
    BT_TRACE_ENTER("app.views_shared");
    if (shared_views == NULL) {
        shared_views = malloc(sizeof *shared_views);
        if (shared_views == NULL) {
            BT_TRACE_FAIL("app.views_shared", "out of memory");
            BT_TRACE_EXIT("app.views_shared");
            return NULL;
        }
        bt_app_views_init(shared_views);
    }
    BT_TRACE_EXIT("app.views_shared");
    return shared_views;
}

void bt_app_views_dispose_shared(void) {
    BT_TRACE_ENTER("app.views_dispose_shared");
    if (shared_views != NULL) {
        bt_app_views_free(shared_views);
        free(shared_views);
        shared_views = NULL;
    }
    BT_TRACE_EXIT("app.views_dispose_shared");
}

static int configured_is_absolute(const char *path) {
    if (path[0] != '\0' && path[1] == ':') {
        return 1;
    }
    return path[0] == '\\' && path[1] == '\\';
}

void bt_views_syllables_path(const bt_app_runtime_paths *paths, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("app.views_syllables_path");
    char configured[BT_PATH_MAX] = "data\\ga";
    bt_json_value *config = NULL;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_load_config_value(paths->config_path, &config, error, sizeof error) == 0) {
        const bt_json_value *paths_section = bt_json_get(config, "paths");
        const bt_json_value *data_ga =
            paths_section != NULL ? bt_json_get(paths_section, "data_ga") : NULL;
        if (data_ga != NULL && data_ga->type == BT_JSON_STRING) {
            snprintf(configured, sizeof configured, "%s", data_ga->as.string);
        }
    }
    bt_json_free(config);
    if (configured_is_absolute(configured)) {
        snprintf(out, BT_PATH_MAX, "%s\\syllables.json", configured);
    } else {
        char root[BT_PATH_MAX];
        bt_paths_workspace_root(root);
        snprintf(out, BT_PATH_MAX, "%s\\%s\\syllables.json", root, configured);
    }
    BT_TRACE_EXIT("app.views_syllables_path");
}

static int syllable_entry_matches(const bt_syllable_entry *entry, const char *orbit,
                                  const char letters[2]) {
    return strcmp(entry->orbit, orbit) == 0 && strncmp(entry->letters, letters, 2) == 0;
}

static int syllable_upsert(bt_syllable_book *book, const char *orbit, const char letters[2],
                           const char *syllable) {
    for (size_t i = 0; i < book->count; i++) {
        if (syllable_entry_matches(&book->items[i], orbit, letters)) {
            char *copy = _strdup(syllable);
            if (copy == NULL) {
                return -1;
            }
            free(book->items[i].syllable);
            book->items[i].syllable = copy;
            return 0;
        }
    }
    bt_syllable_entry *grown = realloc(book->items, (book->count + 1) * sizeof *book->items);
    if (grown == NULL) {
        return -1;
    }
    book->items = grown;
    bt_syllable_entry *entry = &book->items[book->count];
    snprintf(entry->orbit, sizeof entry->orbit, "%s", orbit);
    snprintf(entry->letters, sizeof entry->letters, "%.2s", letters);
    entry->syllable = _strdup(syllable);
    if (entry->syllable == NULL) {
        return -1;
    }
    book->count += 1;
    return 0;
}

int bt_syllable_book_insert(bt_syllable_book *book, const char *orbit, const char letters[2],
                            const char *syllable) {
    BT_TRACE_ENTER("app.views.book_insert");
    int written = syllable_upsert(book, orbit, letters, syllable);
    if (written != 0) {
        BT_TRACE_FAIL("app.views.book_insert", "out of memory");
    }
    BT_TRACE_EXIT("app.views.book_insert");
    return written;
}

static bt_views_image_lookup image_lookup;

void bt_app_views_load_syllables(bt_app_views *views, const bt_app_runtime_paths *paths) {
    BT_TRACE_ENTER("app.views_load_syllables");
    char path[BT_PATH_MAX];
    bt_views_syllables_path(paths, path);
    bt_syllable_book_free(&views->syllables);
    bt_json_value *root = NULL;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_load_config_value(path, &root, error, sizeof error) != 0 || root == NULL ||
        root->type != BT_JSON_OBJECT) {
        bt_json_free(root);
        BT_TRACE_FAIL("app.views_load_syllables", "syllables file unreadable at %s", path);
        BT_TRACE_EXIT("app.views_load_syllables");
        return;
    }
    const bt_json_value *cases = bt_json_get(root, "cases");
    if (cases == NULL || cases->type != BT_JSON_ARRAY) {
        bt_json_free(root);
        BT_TRACE_FAIL("app.views_load_syllables", "syllables cases missing at %s", path);
        BT_TRACE_EXIT("app.views_load_syllables");
        return;
    }
    for (size_t i = 0; i < cases->as.array.count; i++) {
        const bt_json_value *entry = bt_json_at(cases, i);
        const bt_json_value *orbit = entry != NULL ? bt_json_get(entry, "orbit") : NULL;
        const bt_json_value *letters = entry != NULL ? bt_json_get(entry, "letters") : NULL;
        const bt_json_value *syllable = entry != NULL ? bt_json_get(entry, "syllable") : NULL;
        if (entry == NULL || entry->type != BT_JSON_OBJECT || orbit == NULL ||
            orbit->type != BT_JSON_STRING || letters == NULL || letters->type != BT_JSON_STRING ||
            syllable == NULL || syllable->type != BT_JSON_STRING) {
            bt_syllable_book_free(&views->syllables);
            bt_json_free(root);
            BT_TRACE_FAIL("app.views_load_syllables", "syllables case %zu malformed at %s", i,
                          path);
            BT_TRACE_EXIT("app.views_load_syllables");
            return;
        }
        if (syllable->as.string[0] == '\0') {
            continue;
        }
        if (syllable_upsert(&views->syllables, orbit->as.string, letters->as.string,
                            syllable->as.string) != 0) {
            bt_syllable_book_free(&views->syllables);
            bt_json_free(root);
            BT_TRACE_FAIL("app.views_load_syllables", "out of memory at %s", path);
            BT_TRACE_EXIT("app.views_load_syllables");
            return;
        }
    }
    bt_json_free(root);
    views->syllables.loaded = 1;
    BT_TRACE_EXIT("app.views_load_syllables");
}

long long bt_views_now_unix_secs(void) {
    BT_TRACE_ENTER("app.views_now_unix_secs");
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    BT_TRACE_EXIT("app.views_now_unix_secs");
    return (long long)now.tv_sec;
}

long long bt_views_now_day(void) {
    BT_TRACE_ENTER("app.views_now_day");
    long long day = bt_views_now_unix_secs() / 86400;
    BT_TRACE_EXIT("app.views_now_day");
    return day;
}

uint64_t bt_views_time_seed(void) {
    BT_TRACE_ENTER("app.views_time_seed");
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    uint64_t seed = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
    BT_TRACE_EXIT("app.views_time_seed");
    return seed;
}

void bt_views_set_image_lookup(bt_views_image_lookup lookup) {
    BT_TRACE_ENTER("app.views_set_image_lookup");
    image_lookup = lookup;
    BT_TRACE_EXIT("app.views_set_image_lookup");
}

const char *bt_views_image_for(const char letters[2]) {
    BT_TRACE_ENTER("app.views_image_for");
    const char *word = image_lookup != NULL ? image_lookup(letters) : NULL;
    BT_TRACE_EXIT("app.views_image_for");
    return word;
}
