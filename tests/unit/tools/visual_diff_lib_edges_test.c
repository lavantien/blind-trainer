#include "visual_diff_lib_test_util.h"

#include <string.h>

#include "common/alloc.h"
#include "common/strbuf.h"
#include "unit/tools/visual_death_test.h"

static btvd_verdict verdict_of(btvd_verdict_kind kind) {
    btvd_verdict verdict;
    memset(&verdict, 0, sizeof verdict);
    verdict.kind = kind;
    return verdict;
}

BT_TEST(visual_diff_edges, verdict_labels_name_every_kind) {
    char label[BTVD_ERR_MAX];
    btvd_verdict ok = verdict_of(BTVD_VERDICT_OK);
    btvd_verdict_label(&ok, label, sizeof label);
    BT_CHECK_EQ(label, "ok");
    btvd_verdict over = verdict_of(BTVD_VERDICT_OVER);
    over.pixels_over = 7;
    btvd_verdict_label(&over, label, sizeof label);
    BT_CHECK_EQ(label, "over tolerance (7 px)");
    btvd_verdict dim = verdict_of(BTVD_VERDICT_DIM_MISMATCH);
    dim.shot_width = 2;
    dim.shot_height = 3;
    dim.base_width = 4;
    dim.base_height = 5;
    btvd_verdict_label(&dim, label, sizeof label);
    BT_CHECK_EQ(label, "dimension mismatch 2x3 vs 4x5");
    btvd_verdict missing = verdict_of(BTVD_VERDICT_MISSING);
    missing.side = BTVD_SIDE_BASE;
    btvd_verdict_label(&missing, label, sizeof label);
    BT_CHECK_EQ(label, "missing on baselines side");
    btvd_verdict decode = verdict_of(BTVD_VERDICT_DECODE);
    decode.side = BTVD_SIDE_SHOT;
    snprintf(decode.error, sizeof decode.error, "%s", "boom");
    btvd_verdict_label(&decode, label, sizeof label);
    BT_CHECK_EQ(label, "decode failed on screenshots side: boom");
    BT_CHECK(!btvd_verdict_is_failure(&ok));
    BT_CHECK(btvd_verdict_is_failure(&decode));
}

BT_TEST(visual_diff_edges, decode_rejects_a_missing_file) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-miss", dir);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "absent.png");
    btvd_image image;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_decode(path, &image, err, sizeof err), -1);
    BT_CHECK(err[0] != '\0');
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, decode_rejects_a_bad_signature) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-sig", dir);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "not-a.png");
    vd_util_write_bytes(path, (const unsigned char *)"plainly not png", 15);
    btvd_image image;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_decode(path, &image, err, sizeof err), -1);
    BT_CHECK(strstr(err, "png chunk crc invalid") != NULL);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, decode_rejects_a_chunk_that_overruns_the_file) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-trunc", dir);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "overrun.png");
    unsigned char bytes[16] = {
        0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0x7f, 0xff, 0xff, 0xff, 'I', 'H', 'D', 'R',
    };
    vd_util_write_bytes(path, bytes, sizeof bytes);
    btvd_image image;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_decode(path, &image, err, sizeof err), -1);
    BT_CHECK(strstr(err, "png chunk crc invalid") != NULL);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, decode_rejects_a_png_without_iend) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-noend", dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {9, 9, 9, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "cut.png");
    vd_util_write_png(path, &solid);
    unsigned char bytes[4096];
    size_t len = vd_util_read_bytes(path, bytes, sizeof bytes);
    BT_CHECK(len > 12);
    if (len > 12) {
        vd_util_write_bytes(path, bytes, len - 12);
    }
    btvd_image image;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_decode(path, &image, err, sizeof err), -1);
    BT_CHECK(strstr(err, "png chunk crc invalid") != NULL);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, decode_rejects_a_png_with_no_image_data) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-nodata", dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {5, 6, 7, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "whole.png");
    vd_util_write_png(path, &solid);
    unsigned char bytes[4096];
    size_t len = vd_util_read_bytes(path, bytes, sizeof bytes);
    BT_CHECK(len > 8 + 12 + 13 + 12);
    if (len > 8 + 12 + 13 + 12) {
        size_t ihdr_end = 8 + 12 + 13;
        size_t iend_at = len - 12;
        unsigned char soup[64];
        memcpy(soup, bytes, ihdr_end);
        memcpy(soup + ihdr_end, bytes + iend_at, 12);
        vd_util_join(path, dir, "soup.png");
        vd_util_write_bytes(path, soup, ihdr_end + 12);
    }
    btvd_image image;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_decode(path, &image, err, sizeof err), -1);
    BT_CHECK(strstr(err, "raylib load failed") != NULL);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, compare_pair_reports_decode_failure_on_the_shot_side) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dec-shot", dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    unsigned char distinct[4] = {200, 100, 50, 255};
    vd_util_put_pixel(&solid, 1, 0, distinct);
    char shot[VD_UTIL_PATH_MAX];
    char base[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    vd_util_join(base, dir, "base.png");
    vd_util_write_png(base, &solid);
    vd_util_write(shot, "corrupt");
    btvd_pair_report report = btvd_compare_pair("pair.png", shot, base, 0);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_DECODE);
    BT_CHECK_EQ(report.verdict.side, BTVD_SIDE_SHOT);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, strvec_grows_past_sixteen_and_sorts) {
    btvd_strvec vec;
    memset(&vec, 0, sizeof vec);
    for (int at = 0; at < 20; at++) {
        char name[16];
        snprintf(name, sizeof name, "n%02d", 19 - at);
        btvd_strvec_push(&vec, _strdup(name));
    }
    BT_CHECK_EQ(vec.count, 20U);
    btvd_strvec_sort(&vec);
    BT_CHECK_EQ(vec.items[0], "n00");
    BT_CHECK_EQ(vec.items[19], "n19");
    btvd_strvec_free(&vec);
}

BT_TEST(visual_diff_edges, report_vec_grows_past_sixteen) {
    btvd_report_vec vec;
    memset(&vec, 0, sizeof vec);
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.name, sizeof report.name, "%s", "r");
    report.verdict.kind = BTVD_VERDICT_OK;
    for (int at = 0; at < 17; at++) {
        btvd_report_vec_push(&vec, report);
    }
    BT_CHECK_EQ(vec.count, 17U);
    btvd_report_vec_free(&vec);
}

static void edges_death_strvec_growth(void) {
    bt_alloc_fail_arm(0);
    btvd_strvec vec;
    memset(&vec, 0, sizeof vec);
    btvd_strvec_push(&vec, (char *)"name");
}

static void edges_death_report_vec_growth(void) {
    bt_alloc_fail_arm(0);
    btvd_report_vec vec;
    memset(&vec, 0, sizeof vec);
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    btvd_report_vec_push(&vec, report);
}

BT_TEST(visual_diff_edges, vector_growth_exits_with_usage_when_it_starves) {
    vd_death_expect_code("visual_diff_edges.strvec_growth", "out of memory", 2,
                         edges_death_strvec_growth);
    vd_death_expect_code("visual_diff_edges.report_vec_growth", "out of memory", 2,
                         edges_death_report_vec_growth);
}

BT_TEST(visual_diff_edges, read_bytes_fails_closed_when_its_buffer_starves) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("oomread", dir);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "bytes.bin");
    vd_util_write(path, "payload");
    char err[BTVD_ERR_MAX];
    size_t len = 99;
    bt_alloc_fail_arm(0);
    unsigned char *bytes = btvd_fs_read_bytes(path, &len, err, sizeof err);
    bt_alloc_fail_disarm();
    BT_CHECK(bytes == NULL);
    BT_CHECK(strstr(err, "out of memory") != NULL);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, png_names_filters_extensions_case_insensitively) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("exts", dir);
    BT_CHECK(vd_util_is_dir(dir));
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "upper.PNG");
    vd_util_write(path, "x");
    vd_util_join(path, dir, "mixed.pNg");
    vd_util_write(path, "x");
    vd_util_join(path, dir, "plain.txt");
    vd_util_write(path, "x");
    vd_util_join(path, dir, ".png");
    vd_util_write(path, "x");
    vd_util_join(path, dir, "noext");
    vd_util_write(path, "x");
    btvd_strvec names;
    memset(&names, 0, sizeof names);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_png_names(dir, &names, err, sizeof err), 0);
    BT_CHECK_EQ(names.count, 2U);
    if (names.count == 2) {
        BT_CHECK_EQ(names.items[0], "mixed.pNg");
        BT_CHECK_EQ(names.items[1], "upper.PNG");
    }
    btvd_strvec_free(&names);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_edges, env_from_process_captures_inputs_and_root) {
    char *argv[] = {"a", "b"};
    btvd_env env;
    btvd_env_from_process(&env, argv, 2, "cfg.json", "arts");
    BT_CHECK_EQ(env.argc, 2U);
    BT_CHECK(env.argv == argv);
    BT_CHECK(strcmp(env.config, "cfg.json") == 0);
    BT_CHECK(strcmp(env.artifacts, "arts") == 0);
    BT_CHECK(env.root[0] != '\0');
}

BT_TEST(visual_diff_edges, raylib_log_level_maps_every_name_and_rejects_unknown) {
    struct {
        const char *name;
        int want;
        int rc;
    } rows[] = {
        {"all", LOG_ALL, 0},     {"trace", LOG_TRACE, 0},     {"debug", LOG_DEBUG, 0},
        {"info", LOG_INFO, 0},   {"warning", LOG_WARNING, 0}, {"error", LOG_ERROR, 0},
        {"fatal", LOG_FATAL, 0}, {"none", LOG_NONE, 0},       {"loud", 0, -1},
    };
    char text[128];
    snprintf(text, sizeof text, "{\"visual\": {\"raylib_log_level\": \"%s\"}}", rows[0].name);
    for (size_t at = 0; at < sizeof rows / sizeof rows[0]; at++) {
        snprintf(text, sizeof text, "{\"visual\": {\"raylib_log_level\": \"%s\"}}", rows[at].name);
        bt_json_value *config = NULL;
        bt_json_error json_err;
        if (bt_json_parse(text, strlen(text), &config, &json_err) != 0) {
            BT_ABORT("config parse failed");
        }
        int level = -1;
        char err[BTVD_ERR_MAX];
        BT_CHECK_EQ(btvd_raylib_log_level_from(config, &level, err, sizeof err), rows[at].rc);
        if (rows[at].rc == 0) {
            BT_CHECK_EQ(level, rows[at].want);
        } else {
            BT_CHECK(strstr(err, "must be one of") != NULL);
        }
        bt_json_free(config);
    }
    bt_json_value *empty = NULL;
    bt_json_error json_err;
    if (bt_json_parse("{}", 2, &empty, &json_err) != 0) {
        BT_ABORT("empty parse failed");
    }
    int level = 0;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_raylib_log_level_from(empty, &level, err, sizeof err), 0);
    BT_CHECK_EQ(level, LOG_WARNING);
    bt_json_free(empty);
}

BT_TEST(visual_diff_edges, run_rejects_an_unknown_raylib_log_level) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-lvl", root);
    char config[VD_UTIL_PATH_MAX];
    vd_util_join(config, root, "config.json");
    vd_util_write(config, "{\"visual\": {\"tolerance\": 4, \"raylib_log_level\": \"loud\"}}");
    char *argv[] = {"shots", "base"};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), -1);
    BT_CHECK(strstr(err, "must be one of") != NULL);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, run_update_fails_when_baselines_is_a_file) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("up-file", root);
    char shots[VD_UTIL_PATH_MAX];
    char blocker[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/shots", root);
    vd_util_mkdir_p(shots);
    vd_util_join(blocker, root, "blocker");
    vd_util_write(blocker, "in the way");
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shots);
    snprintf(args.baselines, sizeof args.baselines, "%s", blocker);
    args.update = 1;
    btvd_strvec updated;
    memset(&updated, 0, sizeof updated);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run_update(&args, &updated, err, sizeof err), -1);
    BT_CHECK(strstr(err, "cannot create") != NULL);
    btvd_strvec_free(&updated);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, run_update_removes_stale_baselines) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("up-stale", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/shots", root);
    snprintf(bases, sizeof bases, "%s/bases", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases);
    btvd_image solid;
    unsigned char solid_rgba[4] = {3, 3, 3, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "fresh.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases, "stale.png");
    vd_util_write(path, "old");
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shots);
    snprintf(args.baselines, sizeof args.baselines, "%s", bases);
    args.update = 1;
    btvd_strvec updated;
    memset(&updated, 0, sizeof updated);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run_update(&args, &updated, err, sizeof err), 0);
    BT_CHECK_EQ(updated.count, 1U);
    if (updated.count == 1) {
        BT_CHECK_EQ(updated.items[0], "fresh.png");
    }
    char stale_path[VD_UTIL_PATH_MAX];
    vd_util_join(stale_path, bases, "stale.png");
    BT_CHECK(!vd_util_is_file(stale_path));
    vd_util_join(stale_path, bases, "fresh.png");
    BT_CHECK(vd_util_is_file(stale_path));
    btvd_strvec_free(&updated);
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, run_update_failure_surfaces_through_run) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("up-run", root);
    char shots[VD_UTIL_PATH_MAX];
    char config[VD_UTIL_PATH_MAX];
    char blocker[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/shots", root);
    vd_util_mkdir_p(shots);
    vd_util_join(config, root, "config.json");
    vd_util_write_config(root, config, 0);
    vd_util_join(blocker, root, "blocker");
    vd_util_write(blocker, "in the way");
    char *argv[] = {"--update", shots, blocker};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 3);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), -1);
    BT_CHECK(strstr(err, "update failed: cannot create") != NULL);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, write_report_fails_when_visual_is_a_file) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("wr-file", root);
    char blocker[VD_UTIL_PATH_MAX];
    vd_util_join(blocker, root, "visual");
    vd_util_write(blocker, "in the way");
    char out_path[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_write_report("text", root, out_path, sizeof out_path, err, sizeof err), -1);
    BT_CHECK(strstr(err, "cannot write") != NULL);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, run_update_success_prints_the_updated_names) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("up-print", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases[VD_UTIL_PATH_MAX];
    char config[VD_UTIL_PATH_MAX];
    snprintf(shots, sizeof shots, "%s/shots", root);
    snprintf(bases, sizeof bases, "%s/bases", root);
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases);
    btvd_image solid;
    unsigned char solid_rgba[4] = {8, 8, 8, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "fresh.png");
    vd_util_write_png(path, &solid);
    vd_util_write_config(root, config, 0);
    char out[VD_UTIL_PATH_MAX];
    vd_util_join(out, root, "stdout.txt");
    int saved = vd_util_redirect_stdout(out);
    BT_CHECK(saved >= 0);
    char *argv[] = {"--update", shots, bases};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 3);
    char err[BTVD_ERR_MAX];
    int rc = btvd_run(&env, err, sizeof err);
    vd_util_restore_stdout(saved);
    BT_CHECK_EQ(rc, 0);
    unsigned char bytes[256] = {0};
    size_t len = vd_util_read_bytes(out, bytes, sizeof bytes - 1);
    BT_CHECK(len > 0);
    if (len > 0) {
        BT_CHECK(strstr((const char *)bytes, "updated 1 baselines") != NULL);
        BT_CHECK(strstr((const char *)bytes, "updated fresh.png") != NULL);
    }
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, fs_copy_remove_and_replace_report_missing_paths) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("fs-miss", root);
    char absent[VD_UTIL_PATH_MAX];
    char dest[VD_UTIL_PATH_MAX];
    char err[BTVD_ERR_MAX];
    vd_util_join(absent, root, "absent");
    vd_util_join(dest, root, "dest");
    BT_CHECK(btvd_fs_copy(absent, dest, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot read") != NULL);
    BT_CHECK(btvd_fs_remove(absent, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot remove") != NULL);
    BT_CHECK(btvd_fs_replace(absent, dest, err, sizeof err) != 0);
    BT_CHECK(strstr(err, "cannot replace") != NULL);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_edges, fs_rm_rf_removes_files_and_nested_trees) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("rmrf", root);
    char file[VD_UTIL_PATH_MAX];
    char dir[VD_UTIL_PATH_MAX];
    vd_util_join(file, root, "one.txt");
    vd_util_write(file, "x");
    snprintf(dir, sizeof dir, "%s/deep/inner", root);
    vd_util_mkdir_p(dir);
    vd_util_join(file, dir, "two.txt");
    vd_util_write(file, "y");
    snprintf(dir, sizeof dir, "%s/deep", root);
    btvd_fs_rm_rf(dir);
    BT_CHECK(!btvd_fs_is_dir(dir));
    vd_util_join(file, root, "one.txt");
    btvd_fs_rm_rf(file);
    BT_CHECK(!btvd_fs_is_file(file));
    vd_util_rm_rf(root);
}
