#include "visual_diff_test_util.h"

#include "common/strbuf.h"

static bt_json_value *parse_config(const char *text) {
    bt_json_value *config = NULL;
    bt_json_error error;
    int rc = bt_json_parse(text, strlen(text), &config, &error);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "config parse failed: %s", error.message);
        return NULL;
    }
    return config;
}

static btvd_pair_report plain_report(const char *name, btvd_verdict verdict) {
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.name, sizeof report.name, "%s", name);
    report.verdict = verdict;
    return report;
}

BT_TEST(visual_diff_lib, parse_args_reads_two_dirs_and_update_flag) {
    char *two[] = {"shots", "base"};
    btvd_args parsed;
    BT_CHECK_EQ(btvd_parse_args(two, 2, &parsed), 0);
    BT_CHECK(strcmp(parsed.screenshots, "shots") == 0);
    BT_CHECK(strcmp(parsed.baselines, "base") == 0);
    BT_CHECK_EQ(parsed.update, 0);
    char *updated[] = {"shots", "base", "--update"};
    BT_CHECK_EQ(btvd_parse_args(updated, 3, &parsed), 0);
    BT_CHECK_EQ(parsed.update, 1);
    char *flag_first[] = {"--update", "shots", "base"};
    BT_CHECK_EQ(btvd_parse_args(flag_first, 3, &parsed), 0);
    BT_CHECK_EQ(parsed.update, 1);
}

BT_TEST(visual_diff_lib, parse_args_rejects_wrong_counts) {
    btvd_args parsed;
    BT_CHECK_EQ(btvd_parse_args(NULL, 0, &parsed), -1);
    char *one[] = {"shots"};
    BT_CHECK_EQ(btvd_parse_args(one, 1, &parsed), -1);
    char *three[] = {"shots", "base", "extra"};
    BT_CHECK_EQ(btvd_parse_args(three, 3, &parsed), -1);
}

BT_TEST(visual_diff_lib, config_path_from_honors_env_and_default) {
    char out[BT_PATH_MAX];
    btvd_config_path_from("cfg/alt.json", "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/cfg/alt.json") == 0);
    btvd_config_path_from("C:/abs/config.json", "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "C:/abs/config.json") == 0);
    btvd_config_path_from(NULL, "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/config/config.json") == 0);
    btvd_config_path_from("", "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/config/config.json") == 0);
}

BT_TEST(visual_diff_lib, read_config_rejects_missing_file_and_bad_json) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("cfg", dir);
    char missing[VD_UTIL_PATH_MAX];
    vd_util_join(missing, dir, "missing.json");
    char err[BTVD_ERR_MAX];
    BT_CHECK(btvd_read_config(missing, err, sizeof err) == NULL);
    BT_CHECK(strncmp(err, "cannot read config", 18) == 0);
    char bad[VD_UTIL_PATH_MAX];
    vd_util_join(bad, dir, "bad.json");
    vd_util_write(bad, "{not json");
    BT_CHECK(btvd_read_config(bad, err, sizeof err) == NULL);
    BT_CHECK(strncmp(err, "cannot parse config", 19) == 0);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, visual_cfg_from_reads_tolerance_and_rejects_bad_values) {
    bt_json_value *ok = parse_config("{\"visual\": {\"tolerance\": 8}}");
    uint16_t tolerance = 0;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_visual_cfg_from(ok, &tolerance, err, sizeof err), 0);
    BT_CHECK_EQ(tolerance, 8);
    bt_json_free(ok);
    bt_json_value *missing = parse_config("{\"app\": {}}");
    BT_CHECK(btvd_visual_cfg_from(missing, &tolerance, err, sizeof err) != 0);
    bt_json_free(missing);
    bt_json_value *too_big = parse_config("{\"visual\": {\"tolerance\": 256}}");
    BT_CHECK(btvd_visual_cfg_from(too_big, &tolerance, err, sizeof err) != 0);
    bt_json_free(too_big);
    bt_json_value *negative = parse_config("{\"visual\": {\"tolerance\": -1}}");
    BT_CHECK(btvd_visual_cfg_from(negative, &tolerance, err, sizeof err) != 0);
    bt_json_free(negative);
    bt_json_value *text = parse_config("{\"visual\": {\"tolerance\": \"8\"}}");
    BT_CHECK(btvd_visual_cfg_from(text, &tolerance, err, sizeof err) != 0);
    bt_json_free(text);
}

BT_TEST(visual_diff_lib, artifacts_root_from_prefers_env_then_config_then_default) {
    bt_json_value *config = parse_config("{\"paths\": {\"artifacts\": \"out\"}}");
    char out[BT_PATH_MAX];
    btvd_artifacts_root_from("env-out", config, "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/env-out") == 0);
    btvd_artifacts_root_from(NULL, config, "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/out") == 0);
    btvd_artifacts_root_from(NULL, NULL, "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "/repo/artifacts") == 0);
    btvd_artifacts_root_from("D:/abs", config, "/repo", out, sizeof out);
    BT_CHECK(strcmp(out, "D:/abs") == 0);
    bt_json_free(config);
}

BT_TEST(visual_diff_lib, compare_images_flags_pixel_only_when_a_channel_exceeds_tolerance) {
    btvd_image base;
    unsigned char base_rgba[4] = {10, 10, 10, 255};
    vd_util_solid_image(&base, 4, 4, base_rgba);
    btvd_image same;
    vd_util_solid_image(&same, 4, 4, base_rgba);
    btvd_comparison comparison;
    btvd_compare_images(&same, &base, 0, &comparison);
    BT_CHECK_EQ(comparison.max_delta, 0);
    BT_CHECK_EQ(comparison.pixels_over, 0);
    btvd_image within;
    unsigned char within_rgba[4] = {12, 8, 10, 255};
    vd_util_solid_image(&within, 4, 4, within_rgba);
    btvd_compare_images(&within, &base, 2, &comparison);
    BT_CHECK_EQ(comparison.max_delta, 2);
    BT_CHECK_EQ(comparison.pixels_over, 0);
    btvd_image over;
    vd_util_solid_image(&over, 4, 4, base_rgba);
    unsigned char over_rgba[4] = {10, 13, 10, 255};
    vd_util_put_pixel(&over, 1, 1, over_rgba);
    btvd_compare_images(&over, &base, 2, &comparison);
    BT_CHECK_EQ(comparison.max_delta, 3);
    BT_CHECK_EQ(comparison.pixels_over, 1);
    vd_util_free_image(&base);
    vd_util_free_image(&same);
    vd_util_free_image(&within);
    vd_util_free_image(&over);
}

BT_TEST(visual_diff_lib, compare_images_alpha_channel_participates) {
    btvd_image base;
    unsigned char base_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&base, 2, 2, base_rgba);
    btvd_image alpha_shift;
    unsigned char alpha_rgba[4] = {0, 0, 0, 250};
    vd_util_solid_image(&alpha_shift, 2, 2, alpha_rgba);
    btvd_comparison comparison;
    btvd_compare_images(&alpha_shift, &base, 4, &comparison);
    BT_CHECK_EQ(comparison.max_delta, 5);
    BT_CHECK_EQ(comparison.pixels_over, 4);
    vd_util_free_image(&base);
    vd_util_free_image(&alpha_shift);
}

BT_TEST(visual_diff_lib, compare_pair_passes_when_delta_equals_tolerance) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("boundary-eq", dir);
    char shot[VD_UTIL_PATH_MAX];
    char base[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    vd_util_join(base, dir, "base.png");
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    vd_util_write_png(shot, &solid);
    btvd_image shifted;
    vd_util_solid_image(&shifted, 2, 1, solid_rgba);
    unsigned char shifted_rgba[4] = {48, 0, 0, 255};
    vd_util_put_pixel(&shifted, 0, 0, shifted_rgba);
    vd_util_write_png(base, &shifted);
    btvd_pair_report report = btvd_compare_pair("pair.png", shot, base, 48);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_OK);
    BT_CHECK_EQ(report.has_numbers, 1);
    BT_CHECK_EQ(report.max_delta, 48);
    BT_CHECK_EQ(report.pixels_over, 0);
    BT_CHECK_EQ(report.pixels, 2);
    vd_util_free_image(&solid);
    vd_util_free_image(&shifted);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, compare_pair_fails_when_delta_is_one_above_tolerance) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("boundary-over", dir);
    char shot[VD_UTIL_PATH_MAX];
    char base[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    vd_util_join(base, dir, "base.png");
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 1, solid_rgba);
    vd_util_write_png(shot, &solid);
    btvd_image shifted;
    vd_util_solid_image(&shifted, 2, 1, solid_rgba);
    unsigned char shifted_rgba[4] = {49, 0, 0, 255};
    vd_util_put_pixel(&shifted, 0, 0, shifted_rgba);
    vd_util_write_png(base, &shifted);
    btvd_pair_report report = btvd_compare_pair("pair.png", shot, base, 48);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_OVER);
    BT_CHECK_EQ(report.verdict.pixels_over, 1);
    BT_CHECK_EQ(report.max_delta, 49);
    vd_util_free_image(&solid);
    vd_util_free_image(&shifted);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, compare_pair_identical_images_pass) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("identical", dir);
    char shot[VD_UTIL_PATH_MAX];
    char base[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    vd_util_join(base, dir, "base.png");
    btvd_image solid;
    unsigned char solid_rgba[4] = {7, 8, 9, 255};
    vd_util_solid_image(&solid, 3, 2, solid_rgba);
    vd_util_write_png(shot, &solid);
    vd_util_write_png(base, &solid);
    btvd_pair_report report = btvd_compare_pair("pair.png", shot, base, 0);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_OK);
    BT_CHECK_EQ(report.max_delta, 0);
    BT_CHECK_EQ(report.pixels_over, 0);
    BT_CHECK_EQ(report.pixels, 6);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, compare_pair_reports_missing_sides) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("missing", dir);
    char shot[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    vd_util_write_png(shot, &solid);
    char absent[VD_UTIL_PATH_MAX];
    vd_util_join(absent, dir, "base.png");
    btvd_pair_report report = btvd_compare_pair("shot.png", shot, absent, 0);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_MISSING);
    BT_CHECK_EQ(report.verdict.side, BTVD_SIDE_BASE);
    report = btvd_compare_pair("shot.png", absent, shot, 0);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_MISSING);
    BT_CHECK_EQ(report.verdict.side, BTVD_SIDE_SHOT);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, compare_pair_reports_dimension_mismatch_and_over) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("dims", dir);
    char shot[VD_UTIL_PATH_MAX];
    char base[VD_UTIL_PATH_MAX];
    vd_util_join(shot, dir, "shot.png");
    vd_util_join(base, dir, "base.png");
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    btvd_image wide;
    vd_util_solid_image(&wide, 4, 2, solid_rgba);
    vd_util_write_png(shot, &wide);
    btvd_image tall;
    vd_util_solid_image(&tall, 2, 4, solid_rgba);
    vd_util_write_png(base, &tall);
    btvd_pair_report report = btvd_compare_pair("pair.png", shot, base, 0);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_DIM_MISMATCH);
    BT_CHECK_EQ(report.verdict.shot_width, 4);
    BT_CHECK_EQ(report.verdict.shot_height, 2);
    BT_CHECK_EQ(report.verdict.base_width, 2);
    BT_CHECK_EQ(report.verdict.base_height, 4);
    btvd_image dotted;
    vd_util_solid_image(&dotted, 4, 2, solid_rgba);
    unsigned char dotted_rgba[4] = {9, 0, 0, 255};
    vd_util_put_pixel(&dotted, 3, 1, dotted_rgba);
    vd_util_write_png(base, &dotted);
    report = btvd_compare_pair("pair.png", shot, base, 5);
    BT_CHECK_EQ(report.verdict.kind, BTVD_VERDICT_OVER);
    BT_CHECK_EQ(report.verdict.pixels_over, 1);
    BT_CHECK_EQ(report.max_delta, 9);
    BT_CHECK_EQ(report.pixels, 8);
    vd_util_free_image(&wide);
    vd_util_free_image(&tall);
    vd_util_free_image(&dotted);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, png_names_lists_only_png_files_sorted) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("names", dir);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, dir, "b.png");
    vd_util_write(path, "");
    vd_util_join(path, dir, "a.png");
    vd_util_write(path, "");
    vd_util_join(path, dir, "notes.txt");
    vd_util_write(path, "");
    char nested[VD_UTIL_PATH_MAX];
    vd_util_join(nested, dir, "nested.png");
#ifdef _WIN32
    _mkdir(nested);
#else
    mkdir(nested, 0755);
#endif
    btvd_strvec names = {0};
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_png_names(dir, &names, err, sizeof err), 0);
    BT_CHECK_EQ(names.count, 2);
    if (names.count == 2) {
        BT_CHECK(strcmp(names.items[0], "a.png") == 0);
        BT_CHECK(strcmp(names.items[1], "b.png") == 0);
    }
    btvd_strvec_free(&names);
    btvd_strvec absent = {0};
    char gone[VD_UTIL_PATH_MAX];
    vd_util_join(gone, dir, "absent");
    BT_CHECK_EQ(btvd_png_names(gone, &absent, err, sizeof err), 0);
    BT_CHECK_EQ(absent.count, 0);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_lib, run_compare_unions_names_from_both_sides) {
    char shot_dir[VD_UTIL_PATH_MAX];
    char base_dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("union-shot", shot_dir);
    vd_util_scratch("union-base", base_dir);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shot_dir, "both.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, base_dir, "both.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, shot_dir, "extra-shot.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, base_dir, "extra-base.png");
    vd_util_write_png(path, &solid);
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shot_dir);
    snprintf(args.baselines, sizeof args.baselines, "%s", base_dir);
    args.update = 0;
    btvd_report_vec reports = {0};
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run_compare(&args, 0, &reports, err, sizeof err), 0);
    BT_CHECK_EQ(reports.count, 3);
    if (reports.count == 3) {
        BT_CHECK(strcmp(reports.items[0].name, "both.png") == 0);
        BT_CHECK(strcmp(reports.items[1].name, "extra-base.png") == 0);
        BT_CHECK(strcmp(reports.items[2].name, "extra-shot.png") == 0);
        BT_CHECK_EQ(reports.items[0].verdict.kind, BTVD_VERDICT_OK);
        BT_CHECK_EQ(reports.items[1].verdict.kind, BTVD_VERDICT_MISSING);
        BT_CHECK_EQ(reports.items[1].verdict.side, BTVD_SIDE_SHOT);
        BT_CHECK_EQ(reports.items[2].verdict.kind, BTVD_VERDICT_MISSING);
        BT_CHECK_EQ(reports.items[2].verdict.side, BTVD_SIDE_BASE);
    }
    btvd_report_vec_free(&reports);
    vd_util_free_image(&solid);
    vd_util_rm_rf(shot_dir);
    vd_util_rm_rf(base_dir);
}

BT_TEST(visual_diff_lib, run_update_mirrors_screenshots_into_baselines) {
    char shot_dir[VD_UTIL_PATH_MAX];
    char base_dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("update-shot", shot_dir);
    vd_util_scratch("update-base", base_dir);
    btvd_image stale;
    unsigned char stale_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&stale, 1, 1, stale_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, base_dir, "stale.png");
    vd_util_write_png(path, &stale);
    vd_util_join(path, base_dir, "keep.txt");
    vd_util_write(path, "");
    btvd_image fresh;
    unsigned char fresh_rgba[4] = {7, 7, 7, 255};
    vd_util_solid_image(&fresh, 2, 1, fresh_rgba);
    vd_util_join(path, shot_dir, "fresh.png");
    vd_util_write_png(path, &fresh);
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shot_dir);
    snprintf(args.baselines, sizeof args.baselines, "%s", base_dir);
    args.update = 1;
    btvd_strvec updated = {0};
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run_update(&args, &updated, err, sizeof err), 0);
    BT_CHECK_EQ(updated.count, 1);
    if (updated.count == 1) {
        BT_CHECK(strcmp(updated.items[0], "fresh.png") == 0);
    }
    vd_util_join(path, base_dir, "fresh.png");
    BT_CHECK(vd_util_is_file(path));
    vd_util_join(path, base_dir, "stale.png");
    BT_CHECK(!vd_util_is_file(path) && !vd_util_is_dir(path));
    vd_util_join(path, base_dir, "keep.txt");
    BT_CHECK(vd_util_is_file(path));
    btvd_image reloaded;
    char reloaded_path[VD_UTIL_PATH_MAX];
    vd_util_join(reloaded_path, base_dir, "fresh.png");
    BT_CHECK_EQ(btvd_decode(reloaded_path, &reloaded, err, sizeof err), 0);
    BT_CHECK_EQ(reloaded.width, 2);
    BT_CHECK_EQ(reloaded.height, 1);
    vd_util_free_image(&reloaded);
    btvd_strvec_free(&updated);
    vd_util_free_image(&stale);
    vd_util_free_image(&fresh);
    vd_util_rm_rf(shot_dir);
    vd_util_rm_rf(base_dir);
}

BT_TEST(visual_diff_lib, render_report_lists_pairs_and_totals) {
    btvd_pair_report reports[2];
    reports[0] = plain_report("ok.png", (btvd_verdict){.kind = BTVD_VERDICT_OK});
    reports[0].has_numbers = 1;
    reports[0].max_delta = 3;
    reports[0].pixels_over = 0;
    reports[0].pixels = 4;
    reports[1] = plain_report(
        "bad.png", (btvd_verdict){.kind = BTVD_VERDICT_MISSING, .side = BTVD_SIDE_BASE});
    btvd_report_vec vec = {reports, 2, 0};
    bt_strbuf text;
    bt_strbuf_init(&text, 256);
    btvd_render_report(&vec, 7, "shots", "base", &text);
    BT_CHECK(strstr(text.data, "# visual diff report") != NULL);
    BT_CHECK(strstr(text.data, "tolerance: 7") != NULL);
    BT_CHECK(strstr(text.data, "| ok.png | ok | 3 | 0 | 4 |") != NULL);
    BT_CHECK(strstr(text.data, "missing on baselines side") != NULL);
    BT_CHECK(strstr(text.data, "2 pairs, 1 ok, 1 failed") != NULL);
    bt_strbuf_free(&text);
}

BT_TEST(visual_diff_lib, render_report_prints_dirs_and_escapes_pipes) {
    btvd_pair_report report =
        plain_report("a|b.png", (btvd_verdict){.kind = BTVD_VERDICT_OVER, .pixels_over = 2});
    report.has_numbers = 1;
    report.max_delta = 9;
    report.pixels_over = 2;
    report.pixels = 4;
    btvd_report_vec vec = {&report, 1, 0};
    bt_strbuf text;
    bt_strbuf_init(&text, 256);
    btvd_render_report(&vec, 5, "shot-dir", "base-dir", &text);
    BT_CHECK(strstr(text.data, "screenshots: shot-dir\n") != NULL);
    BT_CHECK(strstr(text.data, "baselines: base-dir\n") != NULL);
    BT_CHECK(strstr(text.data, "| pair | verdict | max delta | pixels over | pixels |") != NULL);
    BT_CHECK(strstr(text.data, "| a/b.png | over tolerance (2 px) | 9 | 2 | 4 |") != NULL);
    bt_strbuf_free(&text);
}

BT_TEST(visual_diff_lib, write_report_creates_visual_dir_under_artifacts) {
    char artifacts[VD_UTIL_PATH_MAX];
    vd_util_scratch("report-artifacts", artifacts);
    char path[VD_UTIL_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_write_report("body", artifacts, path, sizeof path, err, sizeof err), 0);
    char want[VD_UTIL_PATH_MAX];
    vd_util_join(want, artifacts, "visual/report.md");
    BT_CHECK(strcmp(path, want) == 0);
    FILE *file = fopen(path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char body[8] = {0};
        fread(body, 1, sizeof body - 1, file);
        fclose(file);
        BT_CHECK(strcmp(body, "body") == 0);
    }
    vd_util_rm_rf(artifacts);
}

BT_TEST(visual_diff_lib, run_rejects_wrong_arg_count_with_usage_message) {
    btvd_env env;
    char *only[] = {"only"};
    vd_util_env_for(&env, "/repo", NULL, only, 1);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), -1);
    BT_CHECK(strcmp(err, BTVD_USAGE_TEXT) == 0);
}

BT_TEST(visual_diff_lib, parse_cli_routes_selftest_mode) {
    char *selftest[] = {"--selftest", "wd"};
    btvd_cli cli;
    BT_CHECK_EQ(btvd_parse_cli(selftest, 2, &cli), 0);
    BT_CHECK_EQ(cli.kind, BTVD_CLI_SELFTEST);
    BT_CHECK(strcmp(cli.workdir, "wd") == 0);
}

BT_TEST(visual_diff_lib, parse_cli_rejects_selftest_without_single_workdir) {
    btvd_cli cli;
    char *bare[] = {"--selftest"};
    BT_CHECK_EQ(btvd_parse_cli(bare, 1, &cli), -1);
    char *extra[] = {"--selftest", "wd", "extra"};
    BT_CHECK_EQ(btvd_parse_cli(extra, 3, &cli), -1);
    char *mixed[] = {"--update", "--selftest", "wd"};
    BT_CHECK_EQ(btvd_parse_cli(mixed, 3, &cli), -1);
    char *empty[] = {"--selftest", ""};
    BT_CHECK_EQ(btvd_parse_cli(empty, 2, &cli), -1);
}

BT_TEST(visual_diff_lib, run_selftest_mode_ignores_env_config_and_passes) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-selftest", root);
    char absent[VD_UTIL_PATH_MAX];
    vd_util_join(absent, root, "absent.json");
    char workdir[VD_UTIL_PATH_MAX];
    snprintf(workdir, sizeof workdir, "%s", root);
    char *argv[] = {"--selftest", workdir};
    btvd_env env;
    vd_util_env_for(&env, root, absent, argv, 2);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), 0);
    char probe[VD_UTIL_PATH_MAX];
    vd_util_join(probe, root, "config.json");
    BT_CHECK(vd_util_is_file(probe));
    vd_util_join(probe, root, "screenshots/missing.png");
    BT_CHECK(vd_util_is_file(probe));
    vd_util_join(probe, root, "baselines/missing.png");
    BT_CHECK(!vd_util_is_file(probe));
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_lib, run_fails_on_config_without_tolerance) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-no-tol", root);
    char config[VD_UTIL_PATH_MAX];
    vd_util_join(config, root, "config.json");
    vd_util_write(config, "{\"visual\": {}}");
    char *argv[] = {"shots", "base"};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), -1);
    BT_CHECK(strcmp(err, "config visual.tolerance missing") == 0);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_lib, run_returns_zero_and_writes_report_for_identical_dirs) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-ok", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases[VD_UTIL_PATH_MAX];
    vd_util_join(shots, root, "shots");
    vd_util_join(bases, root, "bases");
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases);
    btvd_image solid;
    unsigned char solid_rgba[4] = {1, 2, 3, 255};
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "scene.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases, "scene.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char out[VD_UTIL_PATH_MAX];
    vd_util_join(out, root, "out");
    char *argv[] = {shots, bases};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    env.artifacts = out;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), 0);
    char report[VD_UTIL_PATH_MAX];
    vd_util_join(report, out, "visual/report.md");
    FILE *file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        BT_CHECK(strstr(text, "| scene.png | ok | 0 | 0 | 4 |") != NULL);
        BT_CHECK(strstr(text, "1 pairs, 1 ok, 0 failed") != NULL);
    }
    vd_util_free_image(&solid);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_lib, run_returns_diff_code_for_over_tolerance_pair) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-diff", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases[VD_UTIL_PATH_MAX];
    vd_util_join(shots, root, "shots");
    vd_util_join(bases, root, "bases");
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases);
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    btvd_image solid;
    vd_util_solid_image(&solid, 2, 2, solid_rgba);
    btvd_image dotted;
    vd_util_solid_image(&dotted, 2, 2, solid_rgba);
    unsigned char dotted_rgba[4] = {1, 0, 0, 255};
    vd_util_put_pixel(&dotted, 1, 1, dotted_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "scene.png");
    vd_util_write_png(path, &solid);
    vd_util_join(path, bases, "scene.png");
    vd_util_write_png(path, &dotted);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char out[VD_UTIL_PATH_MAX];
    vd_util_join(out, root, "out");
    char *argv[] = {shots, bases};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 2);
    env.artifacts = out;
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), BTVD_EXIT_DIFF);
    char report[VD_UTIL_PATH_MAX];
    vd_util_join(report, out, "visual/report.md");
    FILE *file = fopen(report, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char text[4096] = {0};
        fread(text, 1, sizeof text - 1, file);
        fclose(file);
        BT_CHECK(strstr(text, "over tolerance (1 px)") != NULL);
        BT_CHECK(strstr(text, "1 pairs, 0 ok, 1 failed") != NULL);
    }
    vd_util_free_image(&solid);
    vd_util_free_image(&dotted);
    vd_util_rm_rf(root);
}

BT_TEST(visual_diff_lib, run_update_mode_mirrors_and_returns_zero) {
    char root[VD_UTIL_PATH_MAX];
    vd_util_scratch("run-update", root);
    char shots[VD_UTIL_PATH_MAX];
    char bases[VD_UTIL_PATH_MAX];
    vd_util_join(shots, root, "shots");
    vd_util_join(bases, root, "bases");
    vd_util_mkdir_p(shots);
    vd_util_mkdir_p(bases);
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    btvd_image solid;
    vd_util_solid_image(&solid, 1, 1, solid_rgba);
    btvd_image fresh;
    unsigned char fresh_rgba[4] = {5, 5, 5, 255};
    vd_util_solid_image(&fresh, 2, 1, fresh_rgba);
    char path[VD_UTIL_PATH_MAX];
    vd_util_join(path, shots, "fresh.png");
    vd_util_write_png(path, &fresh);
    vd_util_join(path, bases, "stale.png");
    vd_util_write_png(path, &solid);
    char config[VD_UTIL_PATH_MAX];
    vd_util_write_config(root, config, 0);
    char *argv[] = {shots, bases, "--update"};
    btvd_env env;
    vd_util_env_for(&env, root, config, argv, 3);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run(&env, err, sizeof err), 0);
    vd_util_join(path, bases, "fresh.png");
    BT_CHECK(vd_util_is_file(path));
    vd_util_join(path, bases, "stale.png");
    BT_CHECK(!vd_util_is_file(path));
    vd_util_free_image(&solid);
    vd_util_free_image(&fresh);
    vd_util_rm_rf(root);
}
