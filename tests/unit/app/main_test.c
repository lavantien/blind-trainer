#include "app/main.h"

#include "common.h"

#include <string.h>

static char **args_pack(char (*slots)[64], char **argv, const char *const *items, int count) {
    for (int i = 0; i < count; i++) {
        snprintf(slots[i], 64, "%s", items[i]);
        argv[i] = slots[i];
    }
    return argv;
}

BT_TEST(main, parse_shot_mode_routes_each_flag) {
    bt_app_shot_mode mode;
    char dir[BT_PATH_MAX];
    char error[128];
    char slots[3][64];
    char *argv[3];
    const char *usage[3] = {"bt", "--usage", "docs/screenshots"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(3, args_pack(slots, argv, usage, 3), &mode, dir,
                                       sizeof dir, error, sizeof error),
                1);
    BT_CHECK_EQ(mode, BT_APP_SHOT_USAGE);
    BT_CHECK_EQ(strcmp(dir, "docs/screenshots"), 0);
    const char *fatal[3] = {"bt", "--fatal-shot", "docs/screenshots"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(3, args_pack(slots, argv, fatal, 3), &mode, dir,
                                       sizeof dir, error, sizeof error),
                1);
    BT_CHECK_EQ(mode, BT_APP_SHOT_FATAL_SHOT);
    const char *smoke[3] = {"bt", "--smoke", "artifacts/e2e"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(3, args_pack(slots, argv, smoke, 3), &mode, dir,
                                       sizeof dir, error, sizeof error),
                1);
    BT_CHECK_EQ(mode, BT_APP_SHOT_SMOKE);
    BT_CHECK_EQ(strcmp(dir, "artifacts/e2e"), 0);
    const char *plain[1] = {"bt"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(1, args_pack(slots, argv, plain, 1), &mode, dir,
                                       sizeof dir, error, sizeof error),
                0);
    BT_CHECK_EQ(mode, BT_APP_SHOT_NONE);
}

BT_TEST(main, parse_shot_mode_rejects_missing_values) {
    bt_app_shot_mode mode;
    char dir[BT_PATH_MAX];
    char error[128];
    char slots[2][64];
    char *argv[2];
    const char *usage[2] = {"bt", "--usage"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(2, args_pack(slots, argv, usage, 2), &mode, dir,
                                       sizeof dir, error, sizeof error),
                -1);
    BT_CHECK(strstr(error, "--usage") != NULL);
    const char *fatal[2] = {"bt", "--fatal-shot"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(2, args_pack(slots, argv, fatal, 2), &mode, dir,
                                       sizeof dir, error, sizeof error),
                -1);
    const char *smoke[2] = {"bt", "--smoke"};
    BT_CHECK_EQ(bt_app_parse_shot_mode(2, args_pack(slots, argv, smoke, 2), &mode, dir,
                                       sizeof dir, error, sizeof error),
                -1);
    BT_CHECK(strstr(error, "needs an output directory") != NULL);
}

BT_TEST(main, parse_shot_mode_rejects_flag_combinations_and_flag_like_values) {
    bt_app_shot_mode mode;
    char dir[BT_PATH_MAX];
    char error[128];
    char slots[5][64];
    char *argv[5];
    const char *const combos[4][5] = {
        {"bt", "--usage", "a", "--fatal-shot", "b"},
        {"bt", "--smoke", "a", "--usage", "b"},
        {"bt", "--smoke", "--usage", "docs/x", ""},
        {"bt", "--usage", "--fatal-shot", "", ""},
    };
    const int counts[4] = {5, 5, 4, 3};
    for (int combo = 0; combo < 4; combo++) {
        BT_CHECK_EQ(bt_app_parse_shot_mode(counts[combo],
                                           args_pack(slots, argv, combos[combo],
                                                     counts[combo]),
                                           &mode, dir, sizeof dir, error, sizeof error),
                    -1);
    }
    BT_CHECK(strstr(error, "only one of") != NULL || strstr(error, "got --") != NULL);
}

BT_TEST(main, state_table_has_a_row_per_state_and_boot_enters_the_flow) {
    for (int state = 0; state < BT_APP_STATE_COUNT; state++) {
        BT_CHECK(BT_APP_STATE_TABLE[state].enter != NULL ||
                 BT_APP_STATE_TABLE[state].update != NULL ||
                 BT_APP_STATE_TABLE[state].draw != NULL);
    }
    BT_CHECK(BT_APP_STATE_TABLE[BT_APP_BOOTSTRAP].enter != NULL);
    bt_app_ctx ctx;
    bt_app_ctx_init(&ctx);
    snprintf(ctx.paths.config_path, BT_PATH_MAX,
             "%s/bt-app-main-tests/missing-config/config.json", getenv("TEMP"));
    BT_APP_STATE_TABLE[BT_APP_BOOTSTRAP].enter(&ctx);
    BT_CHECK_EQ(bt_app_apply_pending(&ctx), 1);
    BT_CHECK_EQ(ctx.state, BT_APP_FATAL);
    BT_CHECK(strlen(ctx.fatal_reason) > 0);
    bt_app_ctx_free(&ctx);
}
