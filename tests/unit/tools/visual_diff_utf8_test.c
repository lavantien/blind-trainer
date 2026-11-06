#include "visual_diff_lib_test_util.h"

BT_TEST(visual_diff_utf8, png_names_lists_vietnamese_file_names) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("utf8-names", dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "ảnh.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, dir, "a.png");
    vd_util_write_png(path, &solid);
    btvd_strvec names = {0};
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_png_names(dir, &names, err, sizeof err), 0);
    BT_CHECK_EQ(names.count, 2);
    if (names.count == 2) {
        BT_CHECK(strcmp(names.items[0], "a.png") == 0);
        BT_CHECK(strcmp(names.items[1], "ảnh.png") == 0);
    }
    btvd_strvec_free(&names);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_utf8, run_reports_vietnamese_dirs_and_names) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-utf8", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases_dir[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/ảnh chụp", root);
    snprintf(bases_dir, sizeof bases_dir, "%s/nền", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases_dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {1, 2, 3, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "ảnh.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases_dir, "ảnh.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, shots, "menu.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases_dir, "menu.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char artifacts[VD_UTIL_PATH_MAX];
    vd_util_join(artifacts, root, "out");
    char *argv[] = {shots, bases_dir};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    env.artifacts = artifacts;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), 0);
    char report[VD_UTIL_PATH_MAX];
    vd_util_join(report, artifacts, "visual/report.md");
    FILE *file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        char want_header[VD_UTIL_PATH_MAX + 32];
        snprintf(want_header, sizeof want_header, "screenshots: %s\n", shots);
        BT_CHECK(strstr(text, want_header) != NULL);
        BT_CHECK(strstr(text, "| ảnh.png | ok | 0 | 0 | 4 |") != NULL);
        BT_CHECK(strstr(text, "| menu.png | ok | 0 | 0 | 4 |") != NULL);
        BT_CHECK(strstr(text, "2 pairs, 2 ok, 0 failed") != NULL);
    }
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_utf8, run_fails_exit_diff_on_shot_only_vietnamese_pair) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-utf8-missing", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases_dir[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/ảnh chụp", root);
    snprintf(bases_dir, sizeof bases_dir, "%s/nền", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases_dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "ảnh.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char artifacts[VD_UTIL_PATH_MAX];
    vd_util_join(artifacts, root, "out");
    char *argv[] = {shots, bases_dir};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    env.artifacts = artifacts;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), BTVD_EXIT_DIFF);
    char report[VD_UTIL_PATH_MAX];
    vd_util_join(report, artifacts, "visual/report.md");
    FILE *file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        BT_CHECK(strstr(text, "| ảnh.png | missing on baselines side") != NULL);
        BT_CHECK(strstr(text, "1 pairs, 0 ok, 1 failed") != NULL);
    }
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_utf8, run_update_copies_vietnamese_names) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-utf8-update", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases_dir[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/ảnh chụp", root);
    snprintf(bases_dir, sizeof bases_dir, "%s/nền", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases_dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {5, 5, 5, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "ảnh.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char *argv[] = {shots, bases_dir, "--update"};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 3);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), 0);
    vd_util_join(path, bases_dir, "ảnh.png");
    BT_CHECK(vd_util_is_file(path));
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_utf8, exe_reports_vietnamese_pairs_from_a_wide_argv) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("spawn-utf8", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases_dir[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/ảnh chụp", root);
    snprintf(bases_dir, sizeof bases_dir, "%s/nền", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases_dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {1, 2, 3, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "ảnh.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases_dir, "ảnh.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char artifacts[VD_UTIL_PATH_MAX];
    vd_util_join(artifacts, root, "out");
    char env_config[VD_UTIL_PATH_MAX + 24];
    snprintf(env_config, sizeof env_config, "BT_CONFIG=%s", config);
    char env_artifacts[VD_UTIL_PATH_MAX + 24];
    snprintf(env_artifacts, sizeof env_artifacts, "BT_ARTIFACTS=%s", artifacts);
    _putenv(env_config);
    _putenv(env_artifacts);
    char *ok_args[] = {shots, bases_dir};
    int code = btvd_fs_spawn_wait("build/bin/visual-diff.exe", ok_args, 2);
    BT_CHECK_EQ(code, 0);
    char report[VD_UTIL_PATH_MAX];
    vd_util_join(report, artifacts, "visual/report.md");
    FILE *file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        BT_CHECK(strstr(text, "| ảnh.png | ok | 0 | 0 | 4 |") != NULL);
        BT_CHECK(strstr(text, "1 pairs, 1 ok, 0 failed") != NULL);
    }
    vd_util_join(path, bases_dir, "ảnh.png");
    vd_util_rm_rf(path);
    char *diff_args[] = {shots, bases_dir};
    code = btvd_fs_spawn_wait("build/bin/visual-diff.exe", diff_args, 2);
    BT_CHECK_EQ(code, BTVD_EXIT_DIFF);
    file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        BT_CHECK(strstr(text, "| ảnh.png | missing on baselines side | - | - | - |") != NULL);
        BT_CHECK(strstr(text, "1 pairs, 0 ok, 1 failed") != NULL);
    }
    _putenv("BT_CONFIG=");
    _putenv("BT_ARTIFACTS=");
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}
