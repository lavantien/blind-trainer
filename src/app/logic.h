#ifndef BT_APP_LOGIC_H
#define BT_APP_LOGIC_H

#include "app/config.h"
#include "app/persist.h"
#include "app/states.h"
#include "common/paths.h"
#include "core/cube.h"
#include "data/corpus.h"
#include "data/fsrs.h"

#include <stddef.h>

#define BT_APP_FATAL_MAX 256
#define BT_APP_WARNING_MAX 256
#define BT_APP_KEY_QUEUE_MAX 16

typedef struct {
    char config_path[BT_PATH_MAX];
    char vendor_root[BT_PATH_MAX];
    char packs_root[BT_PATH_MAX];
    char user_root[BT_PATH_MAX];
    char assets_root[BT_PATH_MAX];
} bt_app_runtime_paths;

typedef enum {
    BT_APP_HIGHLIGHT_BUFFER = 0,
    BT_APP_HIGHLIGHT_TARGET,
} bt_app_highlight_kind;

typedef struct {
    bt_cube cube;
    int has_buffer;
    unsigned char buffer_sticker;
    int has_target[2];
    unsigned char targets[2];
} bt_app_net_state;

typedef struct {
    int settings_ok;
    int srs_ok;
    int solves_ok;
    int user_algs_ok;
} bt_app_last_save;

typedef enum {
    BT_APP_KEY_OTHER = 0,
    BT_APP_KEY_ESCAPE,
    BT_APP_KEY_DIGIT,
} bt_app_key_kind;

typedef struct {
    bt_app_key_kind kind;
    char digit;
    int pressed;
} bt_app_key_event;

typedef struct {
    bt_app_runtime_paths paths;
    bt_app_config config;
    bt_fsrs_config fsrs;
    int has_corpus;
    bt_corpus vendor_corpus;
    int has_stores;
    bt_stores stores;
    bt_app_state state;
    int has_pending;
    bt_app_state pending;
    int transitioned;
    bt_drill_phase drill_phase;
    bt_case_drill_phase case_drill_phase;
    bt_solve_phase solve_phase;
    bt_memo_phase memo_phase;
    bt_app_net_state net;
    char fatal_reason[BT_APP_FATAL_MAX];
    bt_str_list boot_warnings;
    bt_app_last_save last_save;
    int input_focus;
    bt_app_key_event keys[BT_APP_KEY_QUEUE_MAX];
    size_t key_count;
} bt_app_ctx;

typedef struct {
    bt_app_config config;
    bt_fsrs_config fsrs;
    bt_corpus corpus;
    bt_stores stores;
    bt_str_list warnings;
} bt_app_booted;

void bt_app_paths_workspace_layout(bt_app_runtime_paths *paths);
void bt_app_paths_from_env(bt_app_runtime_paths *paths);
void bt_app_paths_resolve(const bt_app_runtime_paths *primary, const char *exe_dir,
                          bt_app_runtime_paths *out);
void bt_app_net_state_default(bt_app_net_state *net);
int bt_app_net_highlight_kind(const bt_app_net_state *net, unsigned char sticker,
                              bt_app_highlight_kind *out);
void bt_app_ctx_init(bt_app_ctx *ctx);
void bt_app_ctx_free(bt_app_ctx *ctx);
void bt_app_push_key(bt_app_ctx *ctx, bt_app_key_event event);
void bt_app_process_keys(bt_app_ctx *ctx);
void bt_app_request_state(bt_app_ctx *ctx, bt_app_state state);
void bt_app_nav_to_menu(bt_app_ctx *ctx);
int bt_app_apply_pending(bt_app_ctx *ctx);
int bt_app_default_settings(const bt_app_config *config, bt_settings *out, char *error,
                            size_t error_len);
int bt_app_boot(const bt_app_runtime_paths *paths, bt_app_booted *out, char *error,
                size_t error_len);
void bt_app_booted_free(bt_app_booted *booted);
int bt_app_bootstrap(bt_app_ctx *ctx);
int bt_app_apply_settings_change(bt_app_ctx *ctx, const bt_settings *settings);
int bt_app_save_stores(bt_app_ctx *ctx);
int bt_app_last_save_all_ok(const bt_app_last_save *last);

#endif
