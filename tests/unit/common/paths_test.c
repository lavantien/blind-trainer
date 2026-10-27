#include "common/bt_harness.h"
#include "common/paths.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void make_dir(const char *path) {
    _mkdir(path);
}

BT_TEST(paths, workspace_root_walks_up_to_config_marker) {
    make_dir("build/test-tmp/ws");
    make_dir("build/test-tmp/ws/config");
    FILE *file = fopen("build/test-tmp/ws/config/config.json", "w");
    BT_CHECK(file != NULL);
    fputs("{}", file);
    fclose(file);
    make_dir("build/test-tmp/ws/a");
    make_dir("build/test-tmp/ws/a/b");
    char root[BT_PATH_MAX];
    bt_paths_workspace_root_from("build/test-tmp/ws/a/b", root);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s", "build/test-tmp/ws");
    BT_CHECK(strcmp(root, want) == 0);
}

BT_TEST(paths, workspace_root_falls_back_to_start_without_marker) {
    const char *temp = getenv("TEMP");
    BT_CHECK(temp != NULL);
    char nested[BT_PATH_MAX];
    snprintf(nested, sizeof nested, "%s/bt_no_marker", temp);
    _mkdir(nested);
    char root[BT_PATH_MAX];
    bt_paths_workspace_root_from(nested, root);
    BT_CHECK(strcmp(root, nested) == 0);
    _rmdir(nested);
}

BT_TEST(paths, user_root_prefers_appdata_then_xdg_then_home) {
    char out[BT_PATH_MAX];
    bt_paths_user_root_from("C:/ad", "C:/xdg", "C:/home", out);
    BT_CHECK(strcmp(out, "C:/ad/blind-trainer") == 0);
    bt_paths_user_root_from("", "C:/xdg", "C:/home", out);
    BT_CHECK(strcmp(out, "C:/xdg/blind-trainer") == 0);
    bt_paths_user_root_from(NULL, NULL, "C:/home", out);
    BT_CHECK(strcmp(out, "C:/home/.local/share/blind-trainer") == 0);
    const char *temp = getenv("TEMP");
    bt_paths_user_root_from(NULL, "", "", out);
    char want[BT_PATH_MAX];
    snprintf(want, sizeof want, "%s/blind-trainer", temp);
    BT_CHECK(strcmp(out, want) == 0);
}

BT_TEST(paths, resolve_root_keeps_primary_when_it_exists) {
    make_dir("build/test-tmp/primary_dir");
    make_dir("build/test-tmp/candidate_dir");
    char out[BT_PATH_MAX];
    const char *candidates[1] = {"build/test-tmp/candidate_dir"};
    bt_paths_resolve_root(out, "build/test-tmp/primary_dir", candidates, 1);
    BT_CHECK(strcmp(out, "build/test-tmp/primary_dir") == 0);
}

BT_TEST(paths, resolve_root_falls_back_to_first_existing_candidate) {
    make_dir("build/test-tmp/candidate_dir");
    char out[BT_PATH_MAX];
    const char *candidates[2] = {"build/test-tmp/absent_dir", "build/test-tmp/candidate_dir"};
    bt_paths_resolve_root(out, "build/test-tmp/absent_primary", candidates, 2);
    BT_CHECK(strcmp(out, "build/test-tmp/candidate_dir") == 0);
}

BT_TEST(paths, resolve_root_returns_primary_when_nothing_exists) {
    char out[BT_PATH_MAX];
    const char *candidates[1] = {"build/test-tmp/absent_dir"};
    bt_paths_resolve_root(out, "build/test-tmp/absent_primary", candidates, 1);
    BT_CHECK(strcmp(out, "build/test-tmp/absent_primary") == 0);
}

BT_TEST(paths, config_path_from_env_and_default) {
    char out[BT_PATH_MAX];
    bt_paths_config_path_from(NULL, "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/ws/config/config.json") == 0);
    bt_paths_config_path_from("", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/ws/config/config.json") == 0);
    bt_paths_config_path_from("alt/hub.json", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/ws/alt/hub.json") == 0);
    bt_paths_config_path_from("C:/abs/hub.json", "R:/ws", out);
    BT_CHECK(strcmp(out, "C:/abs/hub.json") == 0);
}

BT_TEST(paths, config_path_from_root_without_drive_keeps_the_base_drive) {
    char out[BT_PATH_MAX];
    bt_paths_config_path_from("/out/hub.json", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:/out/hub.json") == 0);
    bt_paths_config_path_from("\\out\\hub.json", "R:/ws", out);
    BT_CHECK(strcmp(out, "R:\\out\\hub.json") == 0);
    bt_paths_config_path_from("C:ga/hub.json", "R:/ws", out);
    BT_CHECK(strcmp(out, "C:ga/hub.json") == 0);
}

BT_TEST(paths, write_atomic_writes_exact_bytes_and_cleans_tmp) {
    const char *path = "build/test-tmp/atomic.json";
    const char *payload = "{\"a\":1}";
    BT_CHECK_EQ(bt_paths_write_atomic(path, payload, strlen(payload)), 0);
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    char buffer[64] = {0};
    size_t got = fread(buffer, 1, sizeof buffer - 1, file);
    fclose(file);
    BT_CHECK_EQ(got, strlen(payload));
    BT_CHECK(strcmp(buffer, payload) == 0);
    char tmp[BT_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    struct stat info;
    BT_CHECK(stat(tmp, &info) != 0);
    const char *next = "{\"b\":2}";
    BT_CHECK_EQ(bt_paths_write_atomic(path, next, strlen(next)), 0);
    file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    memset(buffer, 0, sizeof buffer);
    got = fread(buffer, 1, sizeof buffer - 1, file);
    fclose(file);
    BT_CHECK(strcmp(buffer, next) == 0);
}

BT_TEST(paths, exe_dir_points_at_an_existing_directory) {
    char out[BT_PATH_MAX];
    bt_paths_exe_dir(out);
    struct stat info;
    BT_CHECK(stat(out, &info) == 0);
    BT_CHECK(out[0] != '\0');
}

BT_TEST(paths, is_absolute_requires_prefix_and_root) {
    BT_CHECK_EQ(bt_paths_is_absolute("C:/hub.json"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("C:\\hub.json"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("C:/"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("\\\\srv\\share\\ga"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("//srv/share/ga"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("\\\\srv\\share\\"), 1);
    BT_CHECK_EQ(bt_paths_is_absolute("C:ga"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("C"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("C:"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("/out"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("\\out"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("out/x"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute(""), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("\\\\srv\\share"), 0);
    BT_CHECK_EQ(bt_paths_is_absolute("\\\\srv"), 0);
}

BT_TEST(paths, join_follows_pathbuf_push_semantics) {
    char out[BT_PATH_MAX];
    bt_paths_join("R:/ws", "/out", out);
    BT_CHECK(strcmp(out, "R:/out") == 0);
    bt_paths_join("R:/ws", "\\out", out);
    BT_CHECK(strcmp(out, "R:\\out") == 0);
    bt_paths_join("R:/ws", "C:ga", out);
    BT_CHECK(strcmp(out, "C:ga") == 0);
    bt_paths_join("R:/ws", "C:/abs", out);
    BT_CHECK(strcmp(out, "C:/abs") == 0);
    bt_paths_join("R:/ws", "\\\\other\\sh\\ga", out);
    BT_CHECK(strcmp(out, "\\\\other\\sh\\ga") == 0);
    bt_paths_join("R:/ws", "leaf/x", out);
    BT_CHECK(strcmp(out, "R:/ws/leaf/x") == 0);
    bt_paths_join("R:/ws/", "leaf", out);
    BT_CHECK(strcmp(out, "R:/ws/leaf") == 0);
    bt_paths_join("C:", "x", out);
    BT_CHECK(strcmp(out, "C:x") == 0);
    bt_paths_join("R:/ws", "", out);
    BT_CHECK(strcmp(out, "R:/ws/") == 0);
    bt_paths_join("R:/ws/", "", out);
    BT_CHECK(strcmp(out, "R:/ws/") == 0);
    bt_paths_join("C:", "", out);
    BT_CHECK(strcmp(out, "C:") == 0);
    bt_paths_join("", "leaf", out);
    BT_CHECK(strcmp(out, "leaf") == 0);
}

BT_TEST(paths, join_keeps_the_base_prefix_on_root_push) {
    char out[BT_PATH_MAX];
    bt_paths_join("\\\\srv\\share\\data", "/out", out);
    BT_CHECK(strcmp(out, "\\\\srv\\share/out") == 0);
    bt_paths_join("\\\\srv\\share\\data", "\\\\other\\sh\\x", out);
    BT_CHECK(strcmp(out, "\\\\other\\sh\\x") == 0);
    bt_paths_join("\\\\srv\\share\\data", "leaf", out);
    BT_CHECK(strcmp(out, "\\\\srv\\share\\data/leaf") == 0);
}
