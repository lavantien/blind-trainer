#include "app/boot.h"

#include "app/views/drill.h"
#include "app/views/drill_view.h"
#include "app/views/memo.h"
#include "app/views/menu.h"
#include "app/views/replay.h"
#include "common/bt_trace.h"

#include <stdio.h>
#include <string.h>

static const char *const BT_APP_SHOT_FLAGS[3] = {"--smoke", "--usage", "--fatal-shot"};

static const char *flag_value(int argc, char **argv, const char *flag) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return i + 1 < argc ? argv[i + 1] : NULL;
        }
    }
    return NULL;
}

static int flag_present(int argc, char **argv, const char *flag) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return 1;
        }
    }
    return 0;
}

int bt_app_parse_shot_mode(int argc, char **argv, bt_app_shot_mode *mode, char *dir_out,
                           size_t dir_len, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.parse_shot_mode");
    *mode = BT_APP_SHOT_NONE;
    dir_out[0] = '\0';
    int present[3] = {0, 0, 0};
    int present_count = 0;
    for (int i = 0; i < 3; i++) {
        if (flag_present(argc, argv, BT_APP_SHOT_FLAGS[i])) {
            present[i] = 1;
            present_count++;
        }
    }
    if (present_count == 0) {
        BT_TRACE_EXIT("app.parse_shot_mode");
        return 0;
    }
    if (present_count > 1) {
        snprintf(error, error_len, "only one of --smoke, --usage, --fatal-shot may be set");
        BT_TRACE_FAIL("app.parse_shot_mode", "%s", error);
        BT_TRACE_EXIT("app.parse_shot_mode");
        return -1;
    }
    int at = 0;
    for (int i = 0; i < 3; i++) {
        if (present[i]) {
            at = i;
        }
    }
    const char *flag = BT_APP_SHOT_FLAGS[at];
    const char *value = flag_value(argc, argv, flag);
    if (value == NULL) {
        snprintf(error, error_len, "%s needs an output directory", flag);
        BT_TRACE_FAIL("app.parse_shot_mode", "%s", error);
        BT_TRACE_EXIT("app.parse_shot_mode");
        return -1;
    }
    if (strncmp(value, "--", 2) == 0) {
        snprintf(error, error_len, "%s needs an output directory, got %s", flag, value);
        BT_TRACE_FAIL("app.parse_shot_mode", "%s", error);
        BT_TRACE_EXIT("app.parse_shot_mode");
        return -1;
    }
    snprintf(dir_out, dir_len, "%s", value);
    *mode = (bt_app_shot_mode)(at + 1);
    BT_TRACE_EXIT("app.parse_shot_mode");
    return 1;
}

int bt_app_init(int argc, char **argv, bt_app_ctx *ctx, bt_app_shot_mode *mode, char *dir,
                size_t dir_len, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.init");
    if (bt_app_parse_shot_mode(argc, argv, mode, dir, dir_len, error, error_len) < 0) {
        BT_TRACE_FAIL("app.init", "%s", error);
        BT_TRACE_EXIT("app.init");
        return 2;
    }
    bt_app_ctx_init(ctx);
    bt_app_runtime_paths paths;
    bt_app_paths_from_env(&paths);
    ctx->paths = paths;
    bt_json_value *value = NULL;
    char load_error[BT_APP_ERROR_MAX];
    if (bt_app_load_config_value(paths.config_path, &value, load_error, sizeof load_error) == 0 &&
        bt_app_config_from_json(value, &ctx->config, load_error, sizeof load_error) == 0) {
        BT_TRACE_EXIT("app.init.config");
    } else {
        bt_app_config_defaults(&ctx->config);
    }
    bt_json_free(value);
    BT_TRACE_EXIT("app.init");
    return 0;
}

static void enter_bootstrap(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.enter_bootstrap");
    (void)bt_app_bootstrap(ctx);
    BT_TRACE_EXIT("app.enter_bootstrap");
}

static void noop_row(bt_app_ctx *ctx) {
    (void)ctx;
}

const bt_app_state_row BT_APP_STATE_TABLE[BT_APP_STATE_COUNT] = {
    {enter_bootstrap, NULL, NULL},
    {noop_row, NULL, NULL},
    {noop_row, NULL, bt_view_menu_row_draw},
    {bt_view_drill_enter, bt_view_drill_update, bt_view_drill_row_draw},
    {bt_view_memo_enter, bt_view_memo_update, bt_view_memo_row_draw},
    {noop_row, NULL, NULL},
    {bt_view_replay_enter, bt_view_replay_update, bt_view_replay_draw},
    {noop_row, NULL, NULL},
    {noop_row, NULL, NULL},
    {noop_row, NULL, NULL},
    {noop_row, NULL, NULL},
};
