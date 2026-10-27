#include "visual_diff_test_util.h"

#include "visual-diff/selftest.h"

static btvd_pair_report plain_report(const char *name, btvd_verdict verdict) {
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.name, sizeof report.name, "%s", name);
    report.verdict = verdict;
    return report;
}

static btvd_report_vec matching_reports(void) {
    static btvd_pair_report reports[4];
    reports[0] = plain_report("identical.png", (btvd_verdict){.kind = BTVD_VERDICT_OK});
    reports[1] =
        plain_report("over.png", (btvd_verdict){.kind = BTVD_VERDICT_OVER, .pixels_over = 1});
    reports[2] = plain_report(
        "missing.png", (btvd_verdict){.kind = BTVD_VERDICT_MISSING, .side = BTVD_SIDE_BASE});
    reports[3] = plain_report("at-tolerance.png", (btvd_verdict){.kind = BTVD_VERDICT_OK});
    return (btvd_report_vec){reports, 4, 0};
}

BT_TEST(visual_diff_selftest, within_pixel_lands_exactly_at_tolerance) {
    unsigned char within[4];
    btvd_within_pixel(BTVD_SELFTEST_TOLERANCE, within);
    BT_CHECK_EQ(within[0], BTVD_SELFTEST_TOLERANCE);
    btvd_within_pixel(255, within);
    BT_CHECK_EQ(within[0], 255);
    btvd_within_pixel(BTVD_SELFTEST_TOLERANCE, within);
    unsigned char over[4];
    btvd_over_pixel(BTVD_SELFTEST_TOLERANCE, over);
    BT_CHECK(memcmp(within, over, 4) != 0);
}

BT_TEST(visual_diff_selftest, verdict_kind_names_every_verdict) {
    BT_CHECK(strcmp(btvd_verdict_kind_label(&(btvd_verdict){.kind = BTVD_VERDICT_OK}),
                    "ok") == 0);
    BT_CHECK(strcmp(btvd_verdict_kind_label(
                        &(btvd_verdict){.kind = BTVD_VERDICT_OVER, .pixels_over = 9}),
                    "over") == 0);
    btvd_verdict dim = {
        .kind = BTVD_VERDICT_DIM_MISMATCH, .shot_width = 1, .shot_height = 2,
        .base_width = 2, .base_height = 1,
    };
    BT_CHECK(strcmp(btvd_verdict_kind_label(&dim), "dim-mismatch") == 0);
    BT_CHECK(strcmp(btvd_verdict_kind_label(
                        &(btvd_verdict){.kind = BTVD_VERDICT_MISSING, .side = BTVD_SIDE_SHOT}),
                    "missing-shot") == 0);
    BT_CHECK(strcmp(btvd_verdict_kind_label(
                        &(btvd_verdict){.kind = BTVD_VERDICT_MISSING, .side = BTVD_SIDE_BASE}),
                    "missing-baseline") == 0);
    btvd_verdict decode = {.kind = BTVD_VERDICT_DECODE, .side = BTVD_SIDE_SHOT};
    snprintf(decode.error, sizeof decode.error, "boom");
    BT_CHECK(strcmp(btvd_verdict_kind_label(&decode), "decode") == 0);
}

BT_TEST(visual_diff_selftest, evaluate_passes_when_reports_match_expectations) {
    btvd_report_vec reports = matching_reports();
    btvd_selftest_checks checks;
    btvd_evaluate(&reports, &checks);
    BT_CHECK_EQ(checks.count, 4);
    int all_passed = 1;
    for (size_t at = 0; at < checks.count; at++) {
        all_passed = all_passed && checks.items[at].passed;
    }
    BT_CHECK_EQ(all_passed, 1);
    BT_CHECK(strcmp(checks.items[0].name, "identical.png") == 0);
    BT_CHECK(strcmp(checks.items[1].name, "over.png") == 0);
    BT_CHECK(strcmp(checks.items[2].name, "missing.png") == 0);
    BT_CHECK(strcmp(checks.items[3].name, "at-tolerance.png") == 0);
}

BT_TEST(visual_diff_selftest, evaluate_flags_absent_and_unexpected_verdicts) {
    btvd_report_vec empty = {NULL, 0, 0};
    btvd_selftest_checks checks;
    btvd_evaluate(&empty, &checks);
    BT_CHECK_EQ(checks.count, 4);
    int all_failed = 1;
    for (size_t at = 0; at < checks.count; at++) {
        all_failed = all_failed && !checks.items[at].passed;
        BT_CHECK(strcmp(checks.items[at].actual, "absent") == 0);
    }
    BT_CHECK_EQ(all_failed, 1);
    btvd_report_vec reports = matching_reports();
    reports.items[1] = plain_report("over.png", (btvd_verdict){.kind = BTVD_VERDICT_OK});
    btvd_evaluate(&reports, &checks);
    BT_CHECK_EQ(checks.items[0].passed, 1);
    BT_CHECK_EQ(checks.items[1].passed, 0);
    BT_CHECK(strcmp(checks.items[1].expected, "over") == 0);
    BT_CHECK(strcmp(checks.items[1].actual, "ok") == 0);
    BT_CHECK_EQ(checks.items[2].passed, 1);
}

BT_TEST(visual_diff_selftest, exit_code_is_zero_only_when_every_check_passes) {
    btvd_report_vec reports = matching_reports();
    btvd_selftest_checks checks;
    btvd_evaluate(&reports, &checks);
    BT_CHECK_EQ(btvd_selftest_exit_code(&checks), 0);
    reports.items[2] = plain_report("missing.png", (btvd_verdict){.kind = BTVD_VERDICT_OK});
    btvd_evaluate(&reports, &checks);
    BT_CHECK_EQ(btvd_selftest_exit_code(&checks), BTVD_EXIT_DIFF);
}

BT_TEST(visual_diff_selftest, over_pixel_lands_exactly_one_above_tolerance) {
    unsigned char pixel[4];
    btvd_over_pixel(BTVD_SELFTEST_TOLERANCE, pixel);
    BT_CHECK_EQ(pixel[0], BTVD_SELFTEST_TOLERANCE + 1);
    BT_CHECK_EQ(pixel[1], 0);
    BT_CHECK_EQ(pixel[2], 0);
    BT_CHECK_EQ(pixel[3], 255);
    btvd_over_pixel(254, pixel);
    BT_CHECK_EQ(pixel[0], 255);
    btvd_over_pixel(255, pixel);
    BT_CHECK_EQ(pixel[0], 255);
}

BT_TEST(visual_diff_selftest, write_config_round_trips_tolerance_from_workdir) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("config", dir);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_write_selftest_config(dir, err, sizeof err), 0);
    char config[VD_UTIL_PATH_MAX];
    vd_util_join(config, dir, "config.json");
    BT_CHECK(vd_util_is_file(config));
    uint16_t tolerance = 0;
    BT_CHECK_EQ(btvd_tolerance_from_config(dir, &tolerance, err, sizeof err), 0);
    BT_CHECK_EQ(tolerance, BTVD_SELFTEST_TOLERANCE);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, materialize_writes_every_scenario) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("materialize", dir);
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    char path[VD_UTIL_PATH_MAX];
    const char *sides[] = {shots, bases, shots, bases, shots, shots, bases};
    const char *leaves[] = {"identical.png", "identical.png", "over.png",
                            "over.png", "missing.png", "at-tolerance.png",
                            "at-tolerance.png"};
    for (size_t at = 0; at < 7; at++) {
        vd_util_join(path, sides[at], leaves[at]);
        BT_CHECK(vd_util_is_file(path));
    }
    vd_util_join(path, bases, "missing.png");
    BT_CHECK(!vd_util_is_file(path));
    btvd_image shot;
    btvd_image base;
    char shot_path[VD_UTIL_PATH_MAX];
    char base_path[VD_UTIL_PATH_MAX];
    vd_util_join(shot_path, shots, "over.png");
    vd_util_join(base_path, bases, "over.png");
    BT_CHECK_EQ(btvd_decode(shot_path, &shot, err, sizeof err), 0);
    BT_CHECK_EQ(btvd_decode(base_path, &base, err, sizeof err), 0);
    size_t differing = 0;
    for (size_t at = 0; at < (size_t)shot.width * shot.height * 4; at += 4) {
        if (memcmp(shot.pixels + at, base.pixels + at, 4) != 0) {
            differing++;
        }
    }
    BT_CHECK_EQ(differing, 1);
    size_t over_at = ((size_t)BTVD_SELFTEST_OVER_Y * shot.width + BTVD_SELFTEST_OVER_X) * 4;
    BT_CHECK(memcmp(shot.pixels + over_at, base.pixels + over_at, 4) != 0);
    BT_CHECK(memcmp(shot.pixels, base.pixels, 4) == 0);
    vd_util_free_image(&shot);
    vd_util_free_image(&base);
    vd_util_join(shot_path, shots, "at-tolerance.png");
    vd_util_join(base_path, bases, "at-tolerance.png");
    BT_CHECK_EQ(btvd_decode(shot_path, &shot, err, sizeof err), 0);
    BT_CHECK_EQ(btvd_decode(base_path, &base, err, sizeof err), 0);
    differing = 0;
    for (size_t at = 0; at < (size_t)shot.width * shot.height * 4; at += 4) {
        if (memcmp(shot.pixels + at, base.pixels + at, 4) != 0) {
            differing++;
        }
    }
    BT_CHECK_EQ(differing, 1);
    vd_util_free_image(&shot);
    vd_util_free_image(&base);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, materialize_is_idempotent_and_clears_stale_files) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("idempotent", dir);
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    char stale[VD_UTIL_PATH_MAX];
    vd_util_join(stale, bases, "missing.png");
    vd_util_write(stale, "");
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    BT_CHECK(!vd_util_is_file(stale));
    char shot_missing[VD_UTIL_PATH_MAX];
    vd_util_join(shot_missing, shots, "missing.png");
    BT_CHECK(vd_util_is_file(shot_missing));
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, execute_detects_every_planted_scenario) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("execute", dir);
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    btvd_selftest_checks checks;
    BT_CHECK_EQ(btvd_execute(shots, bases, BTVD_SELFTEST_TOLERANCE, &checks, err,
                             sizeof err),
                0);
    int all_passed = 1;
    for (size_t at = 0; at < checks.count; at++) {
        all_passed = all_passed && checks.items[at].passed;
    }
    BT_CHECK_EQ(all_passed, 1);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, a_broken_over_fixture_fails_the_selftest) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("broken", dir);
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, BTVD_SELFTEST_WIDTH, BTVD_SELFTEST_HEIGHT, solid_rgba);
    char broken[VD_UTIL_PATH_MAX];
    vd_util_join(broken, bases, "over.png");
    vd_util_write_png(broken, &solid);
    btvd_selftest_checks checks;
    BT_CHECK_EQ(btvd_execute(shots, bases, BTVD_SELFTEST_TOLERANCE, &checks, err,
                             sizeof err),
                0);
    BT_CHECK_EQ(checks.items[0].passed, 1);
    BT_CHECK_EQ(checks.items[1].passed, 0);
    BT_CHECK_EQ(checks.items[2].passed, 1);
    BT_CHECK_EQ(checks.items[3].passed, 1);
    BT_CHECK(strcmp(checks.items[1].actual, "ok") == 0);
    BT_CHECK_EQ(btvd_selftest_exit_code(&checks), BTVD_EXIT_DIFF);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, a_broken_missing_fixture_fails_the_selftest) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("broken-missing", dir);
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_materialize(dir, BTVD_SELFTEST_TOLERANCE, shots, bases, err,
                                 sizeof err),
                0);
    btvd_image solid;
    unsigned char solid_rgba[4] = {0, 0, 0, 255};
    vd_util_solid_image(&solid, BTVD_SELFTEST_WIDTH, BTVD_SELFTEST_HEIGHT, solid_rgba);
    char broken[VD_UTIL_PATH_MAX];
    vd_util_join(broken, bases, "missing.png");
    vd_util_write_png(broken, &solid);
    btvd_selftest_checks checks;
    BT_CHECK_EQ(btvd_execute(shots, bases, BTVD_SELFTEST_TOLERANCE, &checks, err,
                             sizeof err),
                0);
    BT_CHECK_EQ(checks.items[0].passed, 1);
    BT_CHECK_EQ(checks.items[1].passed, 1);
    BT_CHECK_EQ(checks.items[2].passed, 0);
    BT_CHECK_EQ(checks.items[3].passed, 1);
    BT_CHECK_EQ(btvd_selftest_exit_code(&checks), BTVD_EXIT_DIFF);
    vd_util_free_image(&solid);
    vd_util_rm_rf(dir);
}

BT_TEST(visual_diff_selftest, run_selftest_passes_and_leaves_only_workdir_state) {
    char dir[VD_UTIL_PATH_MAX];
    vd_util_scratch("run", dir);
    char err[BTVD_ERR_MAX];
    BT_CHECK_EQ(btvd_run_selftest(dir, err, sizeof err), 0);
    char probe[VD_UTIL_PATH_MAX];
    vd_util_join(probe, dir, "config.json");
    BT_CHECK(vd_util_is_file(probe));
    vd_util_join(probe, dir, "screenshots/identical.png");
    BT_CHECK(vd_util_is_file(probe));
    vd_util_join(probe, dir, "baselines/over.png");
    BT_CHECK(vd_util_is_file(probe));
    BT_CHECK_EQ(btvd_run_selftest(dir, err, sizeof err), 0);
    vd_util_rm_rf(dir);
}
