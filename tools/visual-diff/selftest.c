#include "selftest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common/bt_trace.h"
#include "raylib.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

static const struct {
    const char *name;
    const char *expected;
} BTVD_SCENARIOS[] = {
    {"identical.png", "ok"},
    {"over.png", "over"},
    {"missing.png", "missing-baseline"},
    {"at-tolerance.png", "ok"},
};

static void abort_png(const char *path) {
    BT_TRACE_FAIL("visual_diff.selftest_png", "cannot write %s", path);
    fputs("bt-visual-diff: cannot write selftest png\n", stderr);
    exit(BTVD_EXIT_USAGE);
}

void btvd_over_pixel(uint16_t tolerance, unsigned char out[4]) {
    uint16_t delta = tolerance + 1;
    if (delta > BTVD_ALPHA_OPAQUE) {
        delta = BTVD_ALPHA_OPAQUE;
    }
    out[0] = (unsigned char)delta;
    out[1] = 0;
    out[2] = 0;
    out[3] = BTVD_ALPHA_OPAQUE;
}

void btvd_within_pixel(uint16_t tolerance, unsigned char out[4]) {
    out[0] = (unsigned char)tolerance;
    out[1] = 0;
    out[2] = 0;
    out[3] = BTVD_ALPHA_OPAQUE;
}

const char *btvd_verdict_kind_label(const btvd_verdict *verdict) {
    switch (verdict->kind) {
    case BTVD_VERDICT_OK:
        return "ok";
    case BTVD_VERDICT_OVER:
        return "over";
    case BTVD_VERDICT_DIM_MISMATCH:
        return "dim-mismatch";
    case BTVD_VERDICT_MISSING:
        return verdict->side == BTVD_SIDE_SHOT ? "missing-shot" : "missing-baseline";
    case BTVD_VERDICT_DECODE:
        return "decode";
    }
    return "ok";
}

void btvd_evaluate(const btvd_report_vec *reports, btvd_selftest_checks *out) {
    BT_TRACE_ENTER("visual_diff.evaluate");
    out->count = 0;
    for (size_t at = 0; at < sizeof BTVD_SCENARIOS / sizeof BTVD_SCENARIOS[0]; at++) {
        const char *actual = "absent";
        for (size_t probe = 0; probe < reports->count; probe++) {
            if (strcmp(reports->items[probe].name, BTVD_SCENARIOS[at].name) == 0) {
                actual = btvd_verdict_kind_label(&reports->items[probe].verdict);
                break;
            }
        }
        btvd_selftest_check *check = &out->items[out->count++];
        snprintf(check->name, sizeof check->name, "%s", BTVD_SCENARIOS[at].name);
        check->expected = BTVD_SCENARIOS[at].expected;
        check->actual = actual;
        check->passed = strcmp(actual, BTVD_SCENARIOS[at].expected) == 0;
    }
    BT_TRACE_EXIT("visual_diff.evaluate");
}

int btvd_selftest_exit_code(const btvd_selftest_checks *checks) {
    for (size_t at = 0; at < checks->count; at++) {
        if (!checks->items[at].passed) {
            return BTVD_EXIT_DIFF;
        }
    }
    return 0;
}

static void config_file(const char *workdir, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/config.json", workdir);
}

static void write_solid_png(const char *path) {
    Image image = GenImageColor(BTVD_SELFTEST_WIDTH, BTVD_SELFTEST_HEIGHT, (Color){0, 0, 0, 255});
    if (!ExportImage(image, path)) {
        abort_png(path);
    }
    UnloadImage(image);
}

static void write_dotted_png(const char *path, uint32_t x, uint32_t y,
                             const unsigned char rgba[4]) {
    Image image = GenImageColor(BTVD_SELFTEST_WIDTH, BTVD_SELFTEST_HEIGHT, (Color){0, 0, 0, 255});
    ImageDrawPixel(&image, (int)x, (int)y, (Color){rgba[0], rgba[1], rgba[2], rgba[3]});
    if (!ExportImage(image, path)) {
        abort_png(path);
    }
    UnloadImage(image);
}

static int path_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static void rm_rf(const char *path) {
    if (!path_is_dir(path)) {
        remove(path);
        return;
    }
#ifdef _WIN32
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            rm_rf(child);
        } while (_findnext(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
#else
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
            rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
#endif
}

static void mkdir_p(const char *path) {
    char partial[BT_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
#ifdef _WIN32
        _mkdir(partial);
#else
        mkdir(partial, 0755);
#endif
        partial[at] = keep;
    }
#ifdef _WIN32
    _mkdir(partial);
#else
    mkdir(partial, 0755);
#endif
}

int btvd_write_selftest_config(const char *workdir, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.write_selftest_config");
    char path[BT_PATH_MAX];
    config_file(workdir, path);
    char parent[BT_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash != NULL) {
        *slash = '\0';
        mkdir_p(parent);
    }
    char text[128];
    snprintf(text, sizeof text, "{\"visual\": {\"tolerance\": %u}}", BTVD_SELFTEST_TOLERANCE);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        snprintf(err, err_len, "cannot write config: %s", path);
        BT_TRACE_FAIL("visual_diff.write_selftest_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.write_selftest_config");
        return -1;
    }
    size_t len = strlen(text);
    int broken = fwrite(text, 1, len, file) != len;
    if (fclose(file) != 0) {
        broken = 1;
    }
    if (broken) {
        snprintf(err, err_len, "cannot write config: %s", path);
        BT_TRACE_FAIL("visual_diff.write_selftest_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.write_selftest_config");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.write_selftest_config");
    return 0;
}

int btvd_tolerance_from_config(const char *workdir, uint16_t *tolerance, char *err,
                               size_t err_len) {
    BT_TRACE_ENTER("visual_diff.tolerance_from_config");
    char path[BT_PATH_MAX];
    config_file(workdir, path);
    bt_json_value *config = btvd_read_config(path, err, err_len);
    if (config == NULL) {
        BT_TRACE_EXIT("visual_diff.tolerance_from_config");
        return -1;
    }
    int rc = btvd_visual_cfg_from(config, tolerance, err, err_len);
    bt_json_free(config);
    BT_TRACE_EXIT("visual_diff.tolerance_from_config");
    return rc;
}

int btvd_materialize(const char *workdir, uint16_t tolerance, char shots[BT_PATH_MAX],
                     char bases[BT_PATH_MAX], char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.materialize");
    snprintf(shots, BT_PATH_MAX, "%s/screenshots", workdir);
    snprintf(bases, BT_PATH_MAX, "%s/baselines", workdir);
    const char *dirs[] = {shots, bases};
    for (size_t at = 0; at < 2; at++) {
        if (path_is_dir(dirs[at])) {
            rm_rf(dirs[at]);
        }
        mkdir_p(dirs[at]);
        if (!path_is_dir(dirs[at])) {
            snprintf(err, err_len, "cannot create %s", dirs[at]);
            BT_TRACE_FAIL("visual_diff.materialize", "%s", err);
            BT_TRACE_EXIT("visual_diff.materialize");
            return -1;
        }
    }
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/identical.png", shots);
    write_solid_png(path);
    snprintf(path, sizeof path, "%s/identical.png", bases);
    write_solid_png(path);
    unsigned char over[4];
    btvd_over_pixel(tolerance, over);
    snprintf(path, sizeof path, "%s/over.png", shots);
    write_solid_png(path);
    snprintf(path, sizeof path, "%s/over.png", bases);
    write_dotted_png(path, BTVD_SELFTEST_OVER_X, BTVD_SELFTEST_OVER_Y, over);
    snprintf(path, sizeof path, "%s/missing.png", shots);
    write_solid_png(path);
    unsigned char within[4];
    btvd_within_pixel(tolerance, within);
    snprintf(path, sizeof path, "%s/at-tolerance.png", shots);
    write_solid_png(path);
    snprintf(path, sizeof path, "%s/at-tolerance.png", bases);
    write_dotted_png(path, BTVD_SELFTEST_OVER_X, BTVD_SELFTEST_OVER_Y, within);
    BT_TRACE_EXIT("visual_diff.materialize");
    return 0;
}

int btvd_execute(const char *shots, const char *bases, uint16_t tolerance,
                 btvd_selftest_checks *checks, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.execute");
    btvd_args args;
    snprintf(args.screenshots, sizeof args.screenshots, "%s", shots);
    snprintf(args.baselines, sizeof args.baselines, "%s", bases);
    args.update = 0;
    btvd_report_vec reports = {0};
    char scratch[BTVD_ERR_MAX];
    if (btvd_run_compare(&args, tolerance, &reports, scratch, sizeof scratch) != 0) {
        snprintf(err, err_len, "compare failed: %s", scratch);
        BT_TRACE_FAIL("visual_diff.execute", "%s", err);
        BT_TRACE_EXIT("visual_diff.execute");
        return -1;
    }
    btvd_evaluate(&reports, checks);
    btvd_report_vec_free(&reports);
    BT_TRACE_EXIT("visual_diff.execute");
    return 0;
}

int btvd_run_selftest(const char *workdir, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.run_selftest");
    if (btvd_write_selftest_config(workdir, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.run_selftest");
        return -1;
    }
    uint16_t tolerance = 0;
    if (btvd_tolerance_from_config(workdir, &tolerance, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.run_selftest");
        return -1;
    }
    char shots[BT_PATH_MAX];
    char bases[BT_PATH_MAX];
    if (btvd_materialize(workdir, tolerance, shots, bases, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.run_selftest");
        return -1;
    }
    btvd_selftest_checks checks;
    if (btvd_execute(shots, bases, tolerance, &checks, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.run_selftest");
        return -1;
    }
    size_t passed = 0;
    for (size_t at = 0; at < checks.count; at++) {
        printf("%s: expected %s, got %s\n", checks.items[at].name, checks.items[at].expected,
               checks.items[at].actual);
        passed += checks.items[at].passed ? 1 : 0;
    }
    printf("selftest: %zu/%zu checks passed\n", passed, checks.count);
    int code = btvd_selftest_exit_code(&checks);
    BT_TRACE_EXIT("visual_diff.run_selftest");
    return code;
}
