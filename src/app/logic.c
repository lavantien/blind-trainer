#include "app/logic.h"

#include "common/bt_trace.h"
#include "data/paths.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BT_APP_DEFAULT_REF_GROUND "ref_ground"
#define BT_APP_DEFAULT_ASSETS_ROOT "assets"

enum {
    BT_APP_MEMO_CADENCE_MIN = 1,
    BT_APP_MEMO_CADENCE_MAX = 10000,
    BT_APP_DRILL_REPS_MIN = 1,
    BT_APP_DRILL_REPS_MAX = 100,
    BT_APP_MESSAGE_PAD = 96,
};

static int path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static void resolve_root(const char *primary, const char *const *candidates, size_t count,
                         char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s", primary);
    if (path_exists(primary)) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (path_exists(candidates[i])) {
            snprintf(out, BT_PATH_MAX, "%s", candidates[i]);
            return;
        }
    }
}

void bt_app_paths_workspace_layout(bt_app_runtime_paths *paths) {
    BT_TRACE_ENTER("app.paths_workspace_layout");
    bt_data_config_path(paths->config_path);
    bt_json_value *config = NULL;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_load_config_value(paths->config_path, &config, error, sizeof error) != 0) {
        config = NULL;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    bt_paths_error err;
    const char *ref_ground_env = getenv("BT_REF_GROUND");
    if (bt_data_vendor_root_from(ref_ground_env, config, paths->vendor_root, &err) != 0) {
        snprintf(paths->vendor_root, BT_PATH_MAX, "%s\\%s", root, BT_APP_DEFAULT_REF_GROUND);
    }
    if (bt_data_packs_root_from(ref_ground_env, config, paths->packs_root, &err) != 0) {
        snprintf(paths->packs_root, BT_PATH_MAX, "%s\\%s", paths->vendor_root, "packs");
    }
    snprintf(paths->assets_root, BT_PATH_MAX, "%s\\%s", root, BT_APP_DEFAULT_ASSETS_ROOT);
    if (config != NULL && config->type == BT_JSON_OBJECT) {
        const bt_json_value *fonts = bt_json_get(config, "paths");
        if (fonts != NULL) {
            fonts = bt_json_get(fonts, "fonts");
            if (fonts != NULL && fonts->type == BT_JSON_STRING) {
                const char *slash = strrchr(fonts->as.string, '/');
                const char *back = strrchr(fonts->as.string, '\\');
                const char *cut = back > slash ? back : slash;
                if (cut != NULL) {
                    char assets[BT_PATH_MAX];
                    snprintf(assets, BT_PATH_MAX, "%.*s", (int)(cut - fonts->as.string),
                             fonts->as.string);
                    snprintf(paths->assets_root, BT_PATH_MAX, "%s\\%s", root, assets);
                }
            }
        }
    }
    bt_json_free(config);
    bt_data_user_root(paths->user_root);
    BT_TRACE_EXIT("app.paths_workspace_layout");
}

void bt_app_paths_from_env(bt_app_runtime_paths *paths) {
    BT_TRACE_ENTER("app.paths_from_env");
    bt_app_paths_workspace_layout(paths);
    char exe_dir[BT_PATH_MAX];
    bt_paths_exe_dir(exe_dir);
    bt_app_runtime_paths resolved;
    bt_app_paths_resolve(paths, exe_dir, &resolved);
    *paths = resolved;
    BT_TRACE_EXIT("app.paths_from_env");
}

void bt_app_paths_resolve(const bt_app_runtime_paths *primary, const char *exe_dir,
                          bt_app_runtime_paths *out) {
    BT_TRACE_ENTER("app.paths_resolve");
    *out = *primary;
    if (exe_dir == NULL) {
        BT_TRACE_EXIT("app.paths_resolve");
        return;
    }
    char candidate[BT_PATH_MAX];
    const char *config_candidates[1];
    snprintf(candidate, BT_PATH_MAX, "%s\\config\\config.json", exe_dir);
    config_candidates[0] = candidate;
    resolve_root(primary->config_path, config_candidates, 1, out->config_path);
    const char *vendor_candidates[1];
    snprintf(candidate, BT_PATH_MAX, "%s\\%s", exe_dir, BT_APP_DEFAULT_REF_GROUND);
    vendor_candidates[0] = candidate;
    resolve_root(primary->vendor_root, vendor_candidates, 1, out->vendor_root);
    const char *packs_candidates[1];
    snprintf(candidate, BT_PATH_MAX, "%s\\%s\\packs", exe_dir, BT_APP_DEFAULT_REF_GROUND);
    packs_candidates[0] = candidate;
    resolve_root(primary->packs_root, packs_candidates, 1, out->packs_root);
    const char *assets_candidates[1];
    snprintf(candidate, BT_PATH_MAX, "%s\\%s", exe_dir, BT_APP_DEFAULT_ASSETS_ROOT);
    assets_candidates[0] = candidate;
    resolve_root(primary->assets_root, assets_candidates, 1, out->assets_root);
    BT_TRACE_EXIT("app.paths_resolve");
}

void bt_app_net_state_default(bt_app_net_state *net) {
    BT_TRACE_ENTER("app.net_state_default");
    net->cube = bt_cube_solved();
    net->has_buffer = 1;
    net->buffer_sticker = BT_SCHEME_EDGE_BUFFER_STICKER;
    net->has_target[0] = 0;
    net->has_target[1] = 0;
    net->targets[0] = 0;
    net->targets[1] = 0;
    BT_TRACE_EXIT("app.net_state_default");
}

int bt_app_net_highlight_kind(const bt_app_net_state *net, unsigned char sticker,
                              bt_app_highlight_kind *out) {
    BT_TRACE_ENTER("app.net_highlight_kind");
    if (net->has_buffer && net->buffer_sticker == sticker) {
        *out = BT_APP_HIGHLIGHT_BUFFER;
        BT_TRACE_EXIT("app.net_highlight_kind");
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        if (net->has_target[i] && net->targets[i] == sticker) {
            *out = BT_APP_HIGHLIGHT_TARGET;
            BT_TRACE_EXIT("app.net_highlight_kind");
            return 1;
        }
    }
    BT_TRACE_EXIT("app.net_highlight_kind");
    return 0;
}

void bt_app_ctx_init(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.ctx_init");
    memset(ctx, 0, sizeof *ctx);
    ctx->state = BT_APP_BOOTSTRAP;
    ctx->drill_phase = BT_DRILL_CARD;
    ctx->case_drill_phase = BT_CASE_DRILL_SHOW;
    ctx->solve_phase = BT_SOLVE_READY;
    ctx->memo_phase = BT_MEMO_INTRO;
    bt_app_net_state_default(&ctx->net);
    bt_str_list_init(&ctx->boot_warnings);
    BT_TRACE_EXIT("app.ctx_init");
}

void bt_app_ctx_free(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.ctx_free");
    if (ctx->has_corpus) {
        bt_corpus_free(&ctx->vendor_corpus);
        ctx->has_corpus = 0;
    }
    if (ctx->has_stores) {
        bt_stores_free(&ctx->stores);
        ctx->has_stores = 0;
    }
    bt_str_list_free(&ctx->boot_warnings);
    BT_TRACE_EXIT("app.ctx_free");
}

void bt_app_push_key(bt_app_ctx *ctx, bt_app_key_event event) {
    BT_TRACE_ENTER("app.push_key");
    if (ctx->key_count < BT_APP_KEY_QUEUE_MAX) {
        ctx->keys[ctx->key_count++] = event;
    }
    BT_TRACE_EXIT("app.push_key");
}

void bt_app_nav_to_menu(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.nav_to_menu");
    if (ctx->state != BT_APP_MENU) {
        bt_app_request_state(ctx, BT_APP_MENU);
    }
    BT_TRACE_EXIT("app.nav_to_menu");
}

void bt_app_process_keys(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.process_keys");
    if (ctx->state == BT_APP_BOOTSTRAP || ctx->state == BT_APP_FATAL) {
        ctx->key_count = 0;
        BT_TRACE_EXIT("app.process_keys");
        return;
    }
    for (size_t i = 0; i < ctx->key_count; i++) {
        bt_app_key_event event = ctx->keys[i];
        if (!event.pressed) {
            continue;
        }
        if (event.kind == BT_APP_KEY_ESCAPE) {
            if (ctx->input_focus) {
                ctx->input_focus = 0;
                continue;
            }
            bt_app_nav_to_menu(ctx);
            continue;
        }
        if (ctx->state == BT_APP_MENU && event.kind == BT_APP_KEY_DIGIT) {
            for (int k = 0; k < BT_APP_MENU_KEY_COUNT; k++) {
                if (BT_APP_MENU_KEYS[k].digit == event.digit) {
                    bt_app_request_state(ctx, BT_APP_MENU_KEYS[k].target);
                    break;
                }
            }
        }
    }
    ctx->key_count = 0;
    BT_TRACE_EXIT("app.process_keys");
}

void bt_app_request_state(bt_app_ctx *ctx, bt_app_state state) {
    BT_TRACE_ENTER("app.request_state");
    ctx->has_pending = 1;
    ctx->pending = state;
    BT_TRACE_EXIT("app.request_state");
}

int bt_app_apply_pending(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.apply_pending");
    ctx->transitioned = 0;
    if (!ctx->has_pending) {
        BT_TRACE_EXIT("app.apply_pending");
        return 0;
    }
    if (ctx->pending != ctx->state) {
        ctx->state = ctx->pending;
        ctx->transitioned = 1;
        ctx->input_focus = 0;
    }
    ctx->has_pending = 0;
    BT_TRACE_EXIT("app.apply_pending");
    return ctx->transitioned;
}

static unsigned clamp_uint(unsigned value, unsigned low, unsigned high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

int bt_app_default_settings(const bt_app_config *config, bt_settings *out, char *error,
                            size_t error_len) {
    BT_TRACE_ENTER("app.default_settings");
    bt_memo_mode mode;
    if (!bt_memo_mode_parse(config->memo.mode, &mode)) {
        snprintf(error, error_len, "config app.memo.mode invalid: %s", config->memo.mode);
        BT_TRACE_FAIL("app.default_settings", "%s", error);
        BT_TRACE_EXIT("app.default_settings");
        return -1;
    }
    out->memo_mode = mode;
    out->memo_cadence_ms =
        clamp_uint(config->memo.cadence_ms, BT_APP_MEMO_CADENCE_MIN, BT_APP_MEMO_CADENCE_MAX);
    out->drill_reps = clamp_uint(config->drill.reps, BT_APP_DRILL_REPS_MIN, BT_APP_DRILL_REPS_MAX);
    BT_TRACE_EXIT("app.default_settings");
    return 0;
}

int bt_app_boot(const bt_app_runtime_paths *paths, bt_app_booted *out, char *error,
                size_t error_len) {
    BT_TRACE_ENTER("app.boot");
    memset(out, 0, sizeof *out);
    bt_str_list_init(&out->warnings);
    bt_json_value *value = NULL;
    if (bt_app_load_config_value(paths->config_path, &value, error, error_len) != 0) {
        BT_TRACE_FAIL("app.boot", "%s", error);
        BT_TRACE_EXIT("app.boot");
        return -1;
    }
    if (bt_app_config_from_json(value, &out->config, error, error_len) != 0) {
        bt_json_free(value);
        BT_TRACE_FAIL("app.boot", "%s", error);
        BT_TRACE_EXIT("app.boot");
        return -1;
    }
    if (bt_fsrs_config_from_json(value, &out->fsrs, error, error_len) != 0) {
        bt_json_free(value);
        char held[BT_APP_ERROR_MAX];
        snprintf(held, sizeof held, "%s", error);
        snprintf(error, error_len, "config srs section: %s", held);
        BT_TRACE_FAIL("app.boot", "%s", error);
        BT_TRACE_EXIT("app.boot");
        return -1;
    }
    bt_json_free(value);
    bt_settings defaults;
    if (bt_app_default_settings(&out->config, &defaults, error, error_len) != 0) {
        BT_TRACE_FAIL("app.boot", "%s", error);
        BT_TRACE_EXIT("app.boot");
        return -1;
    }
    const char *packs = path_exists(paths->packs_root) ? paths->packs_root : NULL;
    if (bt_stores_load_with_packs(paths->user_root, defaults, &out->fsrs, packs, &out->stores,
                                  error, error_len) != 0) {
        bt_app_booted_free(out);
        BT_TRACE_FAIL("app.boot", "%s", error);
        BT_TRACE_EXIT("app.boot");
        return -1;
    }
    if (out->stores.seeds.warnings.count > 0) {
        for (size_t i = 0; i < out->stores.seeds.warnings.count; i++) {
            bt_str_list_push(&out->warnings, out->stores.seeds.warnings.items[i]);
        }
    }
    if (path_exists(paths->vendor_root)) {
        bt_corpus_error err;
        if (bt_corpus_load(paths->vendor_root, &out->corpus, &err) != 0) {
            char text[BT_PATH_MAX + BT_APP_MESSAGE_PAD];
            bt_corpus_error_string(&err, text, sizeof text);
            snprintf(error, error_len, "vendor corpus at %s: %s", paths->vendor_root, text);
            bt_app_booted_free(out);
            BT_TRACE_FAIL("app.boot", "%s", error);
            BT_TRACE_EXIT("app.boot");
            return -1;
        }
        bt_overlay_error overlay_err;
        bt_corpus overlaid;
        if (bt_corpus_with_overlay(&out->corpus, &out->stores.user_algs, &overlaid, &overlay_err) !=
            0) {
            char text[BT_PATH_MAX + BT_APP_MESSAGE_PAD];
            bt_overlay_error_string(&overlay_err, text, sizeof text);
            snprintf(error, error_len, "user algs overlay: %s", text);
            bt_app_booted_free(out);
            BT_TRACE_FAIL("app.boot", "%s", error);
            BT_TRACE_EXIT("app.boot");
            return -1;
        }
        bt_corpus_free(&out->corpus);
        out->corpus = overlaid;
        if (packs == NULL) {
            char warning[BT_PATH_MAX + BT_APP_MESSAGE_PAD];
            snprintf(warning, sizeof warning,
                     "seed packs missing at %s, repository browser limited to 3bld",
                     paths->packs_root);
            bt_str_list_push(&out->warnings, warning);
        }
    } else {
        char warning[BT_PATH_MAX + BT_APP_MESSAGE_PAD];
        snprintf(warning, sizeof warning, "vendor data missing at %s, running on user data only",
                 paths->vendor_root);
        bt_str_list_push(&out->warnings, warning);
        bt_corpus_from_rows(NULL, 0, &out->corpus);
    }
    BT_TRACE_EXIT("app.boot");
    return 0;
}

void bt_app_booted_free(bt_app_booted *booted) {
    BT_TRACE_ENTER("app.booted_free");
    bt_corpus_free(&booted->corpus);
    bt_stores_free(&booted->stores);
    bt_str_list_free(&booted->warnings);
    memset(booted, 0, sizeof *booted);
    BT_TRACE_EXIT("app.booted_free");
}

int bt_app_bootstrap(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.bootstrap");
    bt_app_booted booted;
    char error[BT_APP_ERROR_MAX];
    if (bt_app_boot(&ctx->paths, &booted, error, sizeof error) != 0) {
        snprintf(ctx->fatal_reason, BT_APP_FATAL_MAX, "%s", error);
        bt_app_request_state(ctx, BT_APP_FATAL);
        BT_TRACE_FAIL("app.bootstrap", "%s", error);
        BT_TRACE_EXIT("app.bootstrap");
        return -1;
    }
    ctx->config = booted.config;
    ctx->fsrs = booted.fsrs;
    ctx->vendor_corpus = booted.corpus;
    ctx->has_corpus = 1;
    ctx->stores = booted.stores;
    ctx->has_stores = 1;
    ctx->boot_warnings = booted.warnings;
    bt_app_request_state(ctx, BT_APP_MENU);
    BT_TRACE_EXIT("app.bootstrap");
    return 0;
}

int bt_app_apply_settings_change(bt_app_ctx *ctx, const bt_settings *settings) {
    BT_TRACE_ENTER("app.apply_settings_change");
    if (!ctx->has_stores) {
        BT_TRACE_FAIL("app.apply_settings_change", "no stores");
        BT_TRACE_EXIT("app.apply_settings_change");
        return -1;
    }
    ctx->stores.settings = *settings;
    char error[BT_APP_ERROR_MAX];
    ctx->last_save.settings_ok =
        bt_app_save_settings(ctx->paths.user_root, settings, error, sizeof error) == 0;
    BT_TRACE_EXIT("app.apply_settings_change");
    return 0;
}

int bt_app_save_stores(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.save_stores");
    if (!ctx->has_stores) {
        BT_TRACE_FAIL("app.save_stores", "no stores");
        BT_TRACE_EXIT("app.save_stores");
        return -1;
    }
    char error[BT_APP_ERROR_MAX];
    bt_app_last_save last;
    last.settings_ok =
        bt_app_save_settings(ctx->paths.user_root, &ctx->stores.settings, error, sizeof error) == 0;
    last.srs_ok = bt_app_save_srs(ctx->paths.user_root, &ctx->stores.srs, error, sizeof error) == 0;
    last.solves_ok =
        bt_app_save_solves(ctx->paths.user_root, &ctx->stores.solves, error, sizeof error) == 0;
    last.user_algs_ok = bt_app_save_user_algs(ctx->paths.user_root, &ctx->stores.user_algs, error,
                                              sizeof error) == 0;
    ctx->last_save = last;
    BT_TRACE_EXIT("app.save_stores");
    return 0;
}

int bt_app_last_save_all_ok(const bt_app_last_save *last) {
    return last->settings_ok && last->srs_ok && last->solves_ok && last->user_algs_ok;
}
