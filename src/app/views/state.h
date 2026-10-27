#ifndef BT_APP_VIEWS_STATE_H
#define BT_APP_VIEWS_STATE_H

#include "app/logic.h"
#include "app/net.h"
#include "common/paths.h"

#include <stdint.h>

#define BT_VIEWS_SYLLABLE_ORBIT_MAX 16
#define BT_VIEWS_SYLLABLE_LETTERS_MAX 8

typedef struct {
    char orbit[BT_VIEWS_SYLLABLE_ORBIT_MAX];
    char letters[BT_VIEWS_SYLLABLE_LETTERS_MAX];
    char *syllable;
} bt_syllable_entry;

typedef struct {
    bt_syllable_entry *items;
    size_t count;
    int loaded;
} bt_syllable_book;

typedef struct {
    float pulse[BT_NET_GRID_CELLS];
    double elapsed;
    int frozen;
    bt_net_cell_ink ink[BT_NET_GRID_CELLS];
} bt_views_net;

typedef struct {
    bt_syllable_book syllables;
    bt_views_net net;
} bt_app_views;

void bt_syllable_book_init(bt_syllable_book *book);
void bt_syllable_book_free(bt_syllable_book *book);
int bt_syllable_book_insert(bt_syllable_book *book, const char *orbit, const char letters[2],
                            const char *syllable);
const char *bt_syllable_book_for(const bt_syllable_book *book, const char *orbit,
                                 const char letters[2]);

void bt_app_views_init(bt_app_views *views);
void bt_app_views_free(bt_app_views *views);
bt_app_views *bt_app_views_shared(void);
void bt_app_views_dispose_shared(void);
void bt_views_syllables_path(const bt_app_runtime_paths *paths, char out[BT_PATH_MAX]);
void bt_app_views_load_syllables(bt_app_views *views, const bt_app_runtime_paths *paths);
long long bt_views_now_unix_secs(void);
long long bt_views_now_day(void);
uint64_t bt_views_time_seed(void);

typedef const char *(*bt_views_image_lookup)(const char letters[2]);

void bt_views_set_image_lookup(bt_views_image_lookup lookup);
const char *bt_views_image_for(const char letters[2]);
const bt_syllable_book *bt_views_book(const bt_app_ctx *ctx);

#endif
