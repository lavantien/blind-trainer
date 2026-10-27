#ifndef BT_APP_VIEWS_ALGS_BROWSE_H
#define BT_APP_VIEWS_ALGS_BROWSE_H

#include "app/persist.h"
#include "app/views/algs/mod.h"
#include "data/seedpack/mod.h"

#include <stddef.h>

#define BT_REPO_LABEL_MAX BT_SEED_CASE_KEY_MAX
#define BT_REPO_LETTERS_LABEL "letters"
#define BT_REPO_PATH_MAX 8
#define BT_REPO_LIST_MAX 64

typedef enum {
    BT_REPO_LETTERS = 0,
    BT_REPO_EVENT,
    BT_REPO_SIZE,
    BT_REPO_METHOD,
    BT_REPO_STAGE,
    BT_REPO_CASE,
} bt_repo_kind;

typedef struct bt_repo_node {
    char label[BT_REPO_LABEL_MAX];
    bt_repo_kind kind;
    struct bt_repo_node *children;
    size_t children_count;
    int has_case;
    char *case_alg;
    char *case_source_url;
    char *case_source_at;
} bt_repo_node;

typedef struct {
    bt_repo_node *arms;
    size_t arm_count;
    size_t arm;
    size_t depth;
    size_t path[BT_REPO_PATH_MAX];
    int repo_focused;
} bt_repo_browser;

typedef struct {
    char label[BT_REPO_LABEL_MAX + 2];
    int selected;
} bt_repo_list_row;

void bt_repo_node_free(bt_repo_node *node);
void bt_repo_browser_default(bt_repo_browser *browser);
void bt_repo_browser_free(bt_repo_browser *browser);
bt_repo_node *bt_repo_arms(const bt_pack_entry *packs, size_t pack_count, const bt_str_list *hidden,
                           const bt_seed_row *rows, size_t row_count, size_t *out_count);
const bt_repo_node *bt_repo_node_at(const bt_repo_node *arms, size_t arm_count, size_t arm,
                                    const size_t *path, size_t depth);
const bt_repo_node *bt_repo_browser_node(const bt_repo_browser *browser);
int bt_repo_browser_letters_focused(const bt_repo_browser *browser);
int bt_repo_browser_has_seed_arms(const bt_repo_browser *browser);
void bt_repo_enter(const bt_stores *stores, bt_repo_browser *browser);
void bt_repo_move(bt_repo_browser *browser, int step);
void bt_repo_deeper(bt_repo_browser *browser);
void bt_repo_shallower(bt_repo_browser *browser);
size_t bt_repo_list_at(const bt_repo_browser *browser, const bt_repo_node **out, size_t cap);
size_t bt_repo_selection(const bt_repo_browser *browser);
size_t bt_repo_list_rows(const bt_repo_browser *browser, size_t cap, bt_repo_list_row *out);
void bt_repo_breadcrumb(const bt_repo_browser *browser, char *out, size_t out_len);
size_t bt_repo_count_kind(const bt_repo_node *node, bt_repo_kind kind);

#endif
