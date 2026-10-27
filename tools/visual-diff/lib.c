#include "lib.h"

#include "selftest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "raylib.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

static void *xalloc(size_t size) {
    void *block = malloc(size);
    if (block == NULL) {
        BT_TRACE_FAIL("visual_diff.alloc", "out of memory at %zu", size);
        fputs("bt-visual-diff: out of memory\n", stderr);
        exit(BTVD_EXIT_USAGE);
    }
    return block;
}

static char *xstrdup(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = xalloc(len);
    memcpy(copy, text, len);
    return copy;
}

static int path_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static int path_is_file(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
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

static int join_path(char out[BT_PATH_MAX], const char *base, const char *leaf) {
    int wrote = snprintf(out, BT_PATH_MAX, "%s/%s", base, leaf);
    return wrote < 0 || (size_t)wrote >= BT_PATH_MAX ? -1 : 0;
}

int btvd_parse_args(char *const *argv, size_t argc, btvd_args *out) {
    BT_TRACE_ENTER("visual_diff.parse_args");
    memset(out, 0, sizeof *out);
    size_t positional = 0;
    for (size_t at = 0; at < argc; at++) {
        if (strcmp(argv[at], "--update") == 0) {
            out->update = 1;
            continue;
        }
        if (positional == 0) {
            snprintf(out->screenshots, sizeof out->screenshots, "%s", argv[at]);
        } else if (positional == 1) {
            snprintf(out->baselines, sizeof out->baselines, "%s", argv[at]);
        }
        positional++;
    }
    if (positional != 2) {
        BT_TRACE_FAIL("visual_diff.parse_args", "usage reject with %zu positionals", positional);
        BT_TRACE_EXIT("visual_diff.parse_args");
        return -1;
    }
    BT_TRACE_EXIT("visual_diff.parse_args");
    return 0;
}

int btvd_parse_cli(char *const *argv, size_t argc, btvd_cli *out) {
    BT_TRACE_ENTER("visual_diff.parse_cli");
    int selftest = 0;
    for (size_t at = 0; at < argc; at++) {
        if (strcmp(argv[at], "--selftest") == 0) {
            selftest = 1;
        }
    }
    if (selftest) {
        if (argc != 2 || strcmp(argv[0], "--selftest") != 0 || argv[1][0] == '\0') {
            BT_TRACE_FAIL("visual_diff.parse_cli", "selftest needs one workdir");
            BT_TRACE_EXIT("visual_diff.parse_cli");
            return -1;
        }
        out->kind = BTVD_CLI_SELFTEST;
        snprintf(out->workdir, sizeof out->workdir, "%s", argv[1]);
        BT_TRACE_EXIT("visual_diff.parse_cli");
        return 0;
    }
    out->kind = BTVD_CLI_COMPARE;
    int rc = btvd_parse_args(argv, argc, &out->args);
    BT_TRACE_EXIT("visual_diff.parse_cli");
    return rc;
}

const char *btvd_side_label(btvd_side side) {
    return side == BTVD_SIDE_SHOT ? "screenshots" : "baselines";
}

int btvd_verdict_is_failure(const btvd_verdict *verdict) {
    return verdict->kind != BTVD_VERDICT_OK;
}

void btvd_verdict_label(const btvd_verdict *verdict, char *out, size_t out_len) {
    switch (verdict->kind) {
    case BTVD_VERDICT_OK:
        snprintf(out, out_len, "ok");
        break;
    case BTVD_VERDICT_OVER:
        snprintf(out, out_len, "over tolerance (%llu px)",
                 (unsigned long long)verdict->pixels_over);
        break;
    case BTVD_VERDICT_DIM_MISMATCH:
        snprintf(out, out_len, "dimension mismatch %ux%u vs %ux%u", verdict->shot_width,
                 verdict->shot_height, verdict->base_width, verdict->base_height);
        break;
    case BTVD_VERDICT_MISSING:
        snprintf(out, out_len, "missing on %s side", btvd_side_label(verdict->side));
        break;
    case BTVD_VERDICT_DECODE:
        snprintf(out, out_len, "decode failed on %s side: %s", btvd_side_label(verdict->side),
                 verdict->error);
        break;
    }
}

void btvd_compare_images(const btvd_image *shot, const btvd_image *base, uint16_t tolerance,
                         btvd_comparison *out) {
    BT_TRACE_ENTER("visual_diff.compare_images");
    out->max_delta = 0;
    out->pixels_over = 0;
    size_t bytes = (size_t)shot->width * shot->height * 4;
    for (size_t at = 0; at < bytes; at += 4) {
        int pixel_over = 0;
        for (size_t channel = 0; channel < 4; channel++) {
            unsigned char a = shot->pixels[at + channel];
            unsigned char b = base->pixels[at + channel];
            unsigned delta = a > b ? (unsigned)a - b : (unsigned)b - a;
            if ((uint16_t)delta > out->max_delta) {
                out->max_delta = (uint16_t)delta;
            }
            pixel_over |= delta > tolerance;
        }
        out->pixels_over += pixel_over ? 1 : 0;
    }
    BT_TRACE_EXIT("visual_diff.compare_images");
}

int btvd_decode(const char *path, btvd_image *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.decode");
    out->pixels = NULL;
    out->width = 0;
    out->height = 0;
    Image image = LoadImage(path);
    if (image.data == NULL) {
        snprintf(err, err_len, "%s: raylib load failed", path);
        BT_TRACE_FAIL("visual_diff.decode", "%s", err);
        BT_TRACE_EXIT("visual_diff.decode");
        return -1;
    }
    if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    if (image.data == NULL || image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        snprintf(err, err_len, "%s: raylib decode failed", path);
        BT_TRACE_FAIL("visual_diff.decode", "%s", err);
        BT_TRACE_EXIT("visual_diff.decode");
        return -1;
    }
    size_t bytes = (size_t)image.width * (size_t)image.height * 4;
    unsigned char *pixels = xalloc(bytes);
    memcpy(pixels, image.data, bytes);
    UnloadImage(image);
    out->pixels = pixels;
    out->width = (uint32_t)image.width;
    out->height = (uint32_t)image.height;
    BT_TRACE_EXIT("visual_diff.decode");
    return 0;
}

static btvd_pair_report missing_report(const char *name, btvd_side side) {
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.name, sizeof report.name, "%s", name);
    report.verdict.kind = BTVD_VERDICT_MISSING;
    report.verdict.side = side;
    return report;
}

btvd_pair_report btvd_compare_pair(const char *name, const char *shot, const char *base,
                                   uint16_t tolerance) {
    BT_TRACE_ENTER("visual_diff.compare_pair");
    char err[BTVD_ERR_MAX];
    if (!path_is_file(shot)) {
        btvd_pair_report report = missing_report(name, BTVD_SIDE_SHOT);
        BT_TRACE_EXIT("visual_diff.compare_pair");
        return report;
    }
    if (!path_is_file(base)) {
        btvd_pair_report report = missing_report(name, BTVD_SIDE_BASE);
        BT_TRACE_EXIT("visual_diff.compare_pair");
        return report;
    }
    btvd_image shot_img;
    btvd_image base_img;
    btvd_pair_report report;
    memset(&report, 0, sizeof report);
    snprintf(report.name, sizeof report.name, "%s", name);
    if (btvd_decode(shot, &shot_img, err, sizeof err) != 0) {
        report.verdict.kind = BTVD_VERDICT_DECODE;
        report.verdict.side = BTVD_SIDE_SHOT;
        snprintf(report.verdict.error, sizeof report.verdict.error, "%s", err);
        BT_TRACE_EXIT("visual_diff.compare_pair");
        return report;
    }
    if (btvd_decode(base, &base_img, err, sizeof err) != 0) {
        report.verdict.kind = BTVD_VERDICT_DECODE;
        report.verdict.side = BTVD_SIDE_BASE;
        snprintf(report.verdict.error, sizeof report.verdict.error, "%s", err);
        free(shot_img.pixels);
        BT_TRACE_EXIT("visual_diff.compare_pair");
        return report;
    }
    if (shot_img.width != base_img.width || shot_img.height != base_img.height) {
        report.verdict.kind = BTVD_VERDICT_DIM_MISMATCH;
        report.verdict.shot_width = shot_img.width;
        report.verdict.shot_height = shot_img.height;
        report.verdict.base_width = base_img.width;
        report.verdict.base_height = base_img.height;
        free(shot_img.pixels);
        free(base_img.pixels);
        BT_TRACE_EXIT("visual_diff.compare_pair");
        return report;
    }
    btvd_comparison comparison;
    btvd_compare_images(&shot_img, &base_img, tolerance, &comparison);
    report.verdict.kind = comparison.pixels_over > 0 ? BTVD_VERDICT_OVER : BTVD_VERDICT_OK;
    report.verdict.pixels_over = comparison.pixels_over;
    report.has_numbers = 1;
    report.max_delta = comparison.max_delta;
    report.pixels_over = comparison.pixels_over;
    report.pixels = (uint64_t)shot_img.width * shot_img.height;
    free(shot_img.pixels);
    free(base_img.pixels);
    BT_TRACE_EXIT("visual_diff.compare_pair");
    return report;
}

void btvd_strvec_push(btvd_strvec *vec, char *item) {
    if (vec->count == vec->cap) {
        vec->cap = vec->cap == 0 ? 16 : vec->cap * 2;
        char **grown = realloc(vec->items, vec->cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("visual_diff.strvec", "growth failed at %zu", vec->cap);
            fputs("bt-visual-diff: out of memory\n", stderr);
            exit(BTVD_EXIT_USAGE);
        }
        vec->items = grown;
    }
    vec->items[vec->count++] = item;
}

static int strvec_cmp(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

void btvd_strvec_sort(btvd_strvec *vec) {
    if (vec->count > 1) {
        qsort(vec->items, vec->count, sizeof *vec->items, strvec_cmp);
    }
}

void btvd_strvec_free(btvd_strvec *vec) {
    for (size_t at = 0; at < vec->count; at++) {
        free(vec->items[at]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->cap = 0;
}

static int eq_ignore_ascii_case(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        char a = *left >= 'A' && *left <= 'Z' ? (char)(*left - 'A' + 'a') : *left;
        char b = *right >= 'A' && *right <= 'Z' ? (char)(*right - 'A' + 'a') : *right;
        if (a != b) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int has_png_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL || dot == name) {
        return 0;
    }
    return eq_ignore_ascii_case(dot + 1, "png");
}

int btvd_png_names(const char *dir, btvd_strvec *names, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.png_names");
    if (!path_is_dir(dir)) {
        BT_TRACE_EXIT("visual_diff.png_names");
        return 0;
    }
    btvd_strvec raw = {0};
#ifdef _WIN32
    char pattern[BT_PATH_MAX];
    int wrote = snprintf(pattern, sizeof pattern, "%s/*", dir);
    if (wrote < 0 || (size_t)wrote >= sizeof pattern) {
        snprintf(err, err_len, "path too long under %s", dir);
        BT_TRACE_FAIL("visual_diff.png_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.png_names");
        return -1;
    }
    struct _finddata_t entry;
    intptr_t handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        snprintf(err, err_len, "cannot list %s", dir);
        BT_TRACE_FAIL("visual_diff.png_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.png_names");
        return -1;
    }
    do {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        btvd_strvec_push(&raw, xstrdup(entry.name));
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
#else
    DIR *handle = opendir(dir);
    if (handle == NULL) {
        snprintf(err, err_len, "cannot list %s", dir);
        BT_TRACE_FAIL("visual_diff.png_names", "%s", err);
        BT_TRACE_EXIT("visual_diff.png_names");
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        btvd_strvec_push(&raw, xstrdup(entry->d_name));
    }
    closedir(handle);
#endif
    btvd_strvec_sort(&raw);
    for (size_t at = 0; at < raw.count; at++) {
        char full[BT_PATH_MAX];
        if (join_path(full, dir, raw.items[at]) != 0) {
            free(raw.items[at]);
            continue;
        }
        if (path_is_file(full) && has_png_extension(raw.items[at])) {
            btvd_strvec_push(names, raw.items[at]);
        } else {
            free(raw.items[at]);
        }
    }
    free(raw.items);
    BT_TRACE_EXIT("visual_diff.png_names");
    return 0;
}

int btvd_pair_names(const char *shot_dir, const char *base_dir, btvd_strvec *names, char *err,
                    size_t err_len) {
    BT_TRACE_ENTER("visual_diff.pair_names");
    if (btvd_png_names(shot_dir, names, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.pair_names");
        return -1;
    }
    if (btvd_png_names(base_dir, names, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.pair_names");
        return -1;
    }
    btvd_strvec_sort(names);
    size_t used = 0;
    for (size_t at = 0; at < names->count; at++) {
        if (used > 0 && strcmp(names->items[used - 1], names->items[at]) == 0) {
            free(names->items[at]);
            continue;
        }
        names->items[used++] = names->items[at];
    }
    names->count = used;
    BT_TRACE_EXIT("visual_diff.pair_names");
    return 0;
}

void btvd_report_vec_push(btvd_report_vec *vec, btvd_pair_report report) {
    if (vec->count == vec->cap) {
        vec->cap = vec->cap == 0 ? 16 : vec->cap * 2;
        btvd_pair_report *grown = realloc(vec->items, vec->cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("visual_diff.report_vec", "growth failed at %zu", vec->cap);
            fputs("bt-visual-diff: out of memory\n", stderr);
            exit(BTVD_EXIT_USAGE);
        }
        vec->items = grown;
    }
    vec->items[vec->count++] = report;
}

void btvd_report_vec_free(btvd_report_vec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->cap = 0;
}

int btvd_run_compare(const btvd_args *args, uint16_t tolerance, btvd_report_vec *reports, char *err,
                     size_t err_len) {
    BT_TRACE_ENTER("visual_diff.run_compare");
    btvd_strvec names = {0};
    if (btvd_pair_names(args->screenshots, args->baselines, &names, err, err_len) != 0) {
        BT_TRACE_EXIT("visual_diff.run_compare");
        return -1;
    }
    for (size_t at = 0; at < names.count; at++) {
        char shot[BT_PATH_MAX];
        char base[BT_PATH_MAX];
        if (join_path(shot, args->screenshots, names.items[at]) != 0 ||
            join_path(base, args->baselines, names.items[at]) != 0) {
            snprintf(err, err_len, "path too long under %s", names.items[at]);
            BT_TRACE_FAIL("visual_diff.run_compare", "%s", err);
            btvd_strvec_free(&names);
            BT_TRACE_EXIT("visual_diff.run_compare");
            return -1;
        }
        btvd_report_vec_push(reports, btvd_compare_pair(names.items[at], shot, base, tolerance));
    }
    btvd_strvec_free(&names);
    BT_TRACE_EXIT("visual_diff.run_compare");
    return 0;
}

static int copy_file(const char *from, const char *to, char *err, size_t err_len) {
    FILE *src = fopen(from, "rb");
    if (src == NULL) {
        snprintf(err, err_len, "cannot read %s: %s", from, strerror(errno));
        return -1;
    }
    FILE *dst = fopen(to, "wb");
    if (dst == NULL) {
        fclose(src);
        snprintf(err, err_len, "cannot write %s: %s", to, strerror(errno));
        return -1;
    }
    char buffer[4096];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, src)) > 0) {
        if (fwrite(buffer, 1, got, dst) != got) {
            snprintf(err, err_len, "short write on %s", to);
            fclose(src);
            fclose(dst);
            return -1;
        }
    }
    int broken = ferror(src);
    fclose(src);
    fclose(dst);
    if (broken) {
        snprintf(err, err_len, "read failed on %s", from);
        return -1;
    }
    return 0;
}

static int strvec_contains(const btvd_strvec *vec, const char *name) {
    for (size_t at = 0; at < vec->count; at++) {
        if (strcmp(vec->items[at], name) == 0) {
            return 1;
        }
    }
    return 0;
}

int btvd_run_update(const btvd_args *args, btvd_strvec *updated, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.run_update");
    char scratch[BTVD_ERR_MAX];
    if (!path_is_dir(args->baselines)) {
        mkdir_p(args->baselines);
    }
    if (!path_is_dir(args->baselines)) {
        snprintf(err, err_len, "cannot create %s", args->baselines);
        BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
        BT_TRACE_EXIT("visual_diff.run_update");
        return -1;
    }
    btvd_strvec current = {0};
    if (btvd_png_names(args->screenshots, &current, scratch, sizeof scratch) != 0) {
        snprintf(err, err_len, "%s", scratch);
        BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
        BT_TRACE_EXIT("visual_diff.run_update");
        return -1;
    }
    for (size_t at = 0; at < current.count; at++) {
        char from[BT_PATH_MAX];
        char to[BT_PATH_MAX];
        if (join_path(from, args->screenshots, current.items[at]) != 0 ||
            join_path(to, args->baselines, current.items[at]) != 0) {
            snprintf(err, err_len, "path too long under %s", current.items[at]);
            BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
            btvd_strvec_free(&current);
            BT_TRACE_EXIT("visual_diff.run_update");
            return -1;
        }
        if (copy_file(from, to, scratch, sizeof scratch) != 0) {
            snprintf(err, err_len, "%s", scratch);
            BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
            btvd_strvec_free(&current);
            BT_TRACE_EXIT("visual_diff.run_update");
            return -1;
        }
        btvd_strvec_push(updated, xstrdup(current.items[at]));
    }
    btvd_strvec stale = {0};
    if (btvd_png_names(args->baselines, &stale, scratch, sizeof scratch) != 0) {
        snprintf(err, err_len, "%s", scratch);
        BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
        btvd_strvec_free(&current);
        BT_TRACE_EXIT("visual_diff.run_update");
        return -1;
    }
    for (size_t at = 0; at < stale.count; at++) {
        if (!strvec_contains(&current, stale.items[at])) {
            char path[BT_PATH_MAX];
            if (join_path(path, args->baselines, stale.items[at]) != 0 || remove(path) != 0) {
                snprintf(err, err_len, "cannot remove %s", stale.items[at]);
                BT_TRACE_FAIL("visual_diff.run_update", "%s", err);
                btvd_strvec_free(&stale);
                btvd_strvec_free(&current);
                BT_TRACE_EXIT("visual_diff.run_update");
                return -1;
            }
        }
    }
    btvd_strvec_free(&stale);
    btvd_strvec_free(&current);
    BT_TRACE_EXIT("visual_diff.run_update");
    return 0;
}

static void replace_pipes(const char *text, char *out, size_t out_len) {
    size_t at = 0;
    for (; text[at] != '\0' && at + 1 < out_len; at++) {
        out[at] = text[at] == '|' ? '/' : text[at];
    }
    out[at] = '\0';
}

void btvd_render_report(const btvd_report_vec *reports, uint16_t tolerance, const char *shot_dir,
                        const char *base_dir, bt_strbuf *out) {
    BT_TRACE_ENTER("visual_diff.render_report");
    bt_strbuf_append_str(out, "# visual diff report\n\n");
    bt_strbuf_appendf(out, "screenshots: %s\n", shot_dir);
    bt_strbuf_appendf(out, "baselines: %s\n", base_dir);
    bt_strbuf_appendf(out, "tolerance: %u\n\n", tolerance);
    bt_strbuf_append_str(out, "| pair | verdict | max delta | pixels over | pixels |\n");
    bt_strbuf_append_str(out, "| ---- | ------- | --------- | ----------- | ------ |\n");
    for (size_t at = 0; at < reports->count; at++) {
        const btvd_pair_report *report = &reports->items[at];
        char name[BTVD_NAME_MAX];
        char label[BTVD_ERR_MAX];
        char max_delta[32];
        char pixels_over[32];
        char pixels[32];
        replace_pipes(report->name, name, sizeof name);
        btvd_verdict_label(&report->verdict, label, sizeof label);
        replace_pipes(label, label, sizeof label);
        snprintf(max_delta, sizeof max_delta, report->has_numbers ? "%u" : "-", report->max_delta);
        snprintf(pixels_over, sizeof pixels_over, report->has_numbers ? "%llu" : "-",
                 (unsigned long long)report->pixels_over);
        snprintf(pixels, sizeof pixels, report->has_numbers ? "%llu" : "-",
                 (unsigned long long)report->pixels);
        bt_strbuf_appendf(out, "| %s | %s | %s | %s | %s |\n", name, label, max_delta, pixels_over,
                          pixels);
    }
    size_t ok_count = 0;
    for (size_t at = 0; at < reports->count; at++) {
        if (!btvd_verdict_is_failure(&reports->items[at].verdict)) {
            ok_count++;
        }
    }
    bt_strbuf_appendf(out, "\n%zu pairs, %zu ok, %zu failed\n", reports->count, ok_count,
                      reports->count - ok_count);
    BT_TRACE_EXIT("visual_diff.render_report");
}

int btvd_write_report(const char *text, const char *artifacts_root, char *out_path, size_t out_len,
                      char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.write_report");
    char dir[BT_PATH_MAX];
    char path[BT_PATH_MAX];
    if (join_path(dir, artifacts_root, "visual") != 0 || join_path(path, dir, "report.md") != 0) {
        snprintf(err, err_len, "path too long under %s", artifacts_root);
        BT_TRACE_FAIL("visual_diff.write_report", "%s", err);
        BT_TRACE_EXIT("visual_diff.write_report");
        return -1;
    }
    mkdir_p(dir);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        snprintf(err, err_len, "cannot write %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("visual_diff.write_report", "%s", err);
        BT_TRACE_EXIT("visual_diff.write_report");
        return -1;
    }
    size_t len = strlen(text);
    int broken = fwrite(text, 1, len, file) != len;
    if (fclose(file) != 0) {
        broken = 1;
    }
    if (broken) {
        snprintf(err, err_len, "short write on %s", path);
        BT_TRACE_FAIL("visual_diff.write_report", "%s", err);
        BT_TRACE_EXIT("visual_diff.write_report");
        return -1;
    }
    snprintf(out_path, out_len, "%s", path);
    BT_TRACE_EXIT("visual_diff.write_report");
    return 0;
}

void btvd_config_path_from(const char *env_value, const char *root, char out[BT_PATH_MAX]) {
    bt_paths_config_path_from(env_value, root, out);
}

bt_json_value *btvd_read_config(const char *path, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.read_config");
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_len, "cannot read config %s", path);
        BT_TRACE_FAIL("visual_diff.read_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.read_config");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, err_len, "cannot read config %s", path);
        BT_TRACE_FAIL("visual_diff.read_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.read_config");
        return NULL;
    }
    char *text = xalloc((size_t)size + 1);
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        free(text);
        snprintf(err, err_len, "cannot read config %s", path);
        BT_TRACE_FAIL("visual_diff.read_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.read_config");
        return NULL;
    }
    text[size] = '\0';
    bt_json_value *config = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, (size_t)size, &config, &json_err) != 0) {
        free(text);
        snprintf(err, err_len, "cannot parse config %s: %s", path, json_err.message);
        BT_TRACE_FAIL("visual_diff.read_config", "%s", err);
        BT_TRACE_EXIT("visual_diff.read_config");
        return NULL;
    }
    free(text);
    BT_TRACE_EXIT("visual_diff.read_config");
    return config;
}

int btvd_visual_cfg_from(const bt_json_value *config, uint16_t *tolerance, char *err,
                         size_t err_len) {
    BT_TRACE_ENTER("visual_diff.visual_cfg_from");
    const bt_json_value *raw = bt_config_value(config, "visual.tolerance");
    if (raw == NULL) {
        snprintf(err, err_len, "config visual.tolerance missing");
        BT_TRACE_FAIL("visual_diff.visual_cfg_from", "%s", err);
        BT_TRACE_EXIT("visual_diff.visual_cfg_from");
        return -1;
    }
    if (raw->type != BT_JSON_NUMBER || !bt_json_is_integer(raw)) {
        snprintf(err, err_len, "config visual.tolerance must be an integer 0..=255");
        BT_TRACE_FAIL("visual_diff.visual_cfg_from", "%s", err);
        BT_TRACE_EXIT("visual_diff.visual_cfg_from");
        return -1;
    }
    long long value = bt_json_integer_or(raw, -1);
    if (value < 0 || value > 255) {
        snprintf(err, err_len, "config visual.tolerance must be an integer 0..=255");
        BT_TRACE_FAIL("visual_diff.visual_cfg_from", "%s", err);
        BT_TRACE_EXIT("visual_diff.visual_cfg_from");
        return -1;
    }
    *tolerance = (uint16_t)value;
    BT_TRACE_EXIT("visual_diff.visual_cfg_from");
    return 0;
}

int btvd_artifacts_root_from(const char *env_value, const bt_json_value *config, const char *root,
                             char *out, size_t out_len) {
    BT_TRACE_ENTER("visual_diff.artifacts_root_from");
    const char *raw = NULL;
    if (env_value != NULL && env_value[0] != '\0') {
        raw = env_value;
    } else if (bt_config_str(config, "paths.artifacts", &raw) != 0) {
        raw = "artifacts";
    }
    int has_prefix = (raw[0] != '\0' && raw[1] == ':') || (raw[0] == '\\' && raw[1] == '\\');
    int has_root = raw[0] == '/' || raw[0] == '\\';
    if (raw[0] == '\0') {
        snprintf(out, out_len, "%s", root);
    } else if (has_prefix != 0) {
        snprintf(out, out_len, "%s", raw);
    } else if (has_root != 0 && root[1] == ':') {
        snprintf(out, out_len, "%2.2s%s", root, raw);
    } else if (has_root != 0) {
        snprintf(out, out_len, "%s", raw);
    } else {
        snprintf(out, out_len, "%s/%s", root, raw);
    }
    BT_TRACE_EXIT("visual_diff.artifacts_root_from");
    return 0;
}

void btvd_env_from_process(btvd_env *env, char **argv, size_t argc) {
    BT_TRACE_ENTER("visual_diff.env_from_process");
    env->argv = argv;
    env->argc = argc;
    env->config = getenv("BT_CONFIG");
    env->artifacts = getenv("BT_ARTIFACTS");
    bt_paths_workspace_root(env->root);
    BT_TRACE_EXIT("visual_diff.env_from_process");
}

static void trace_from_config(const bt_json_value *config) {
    long long ring_bytes = 0;
    long long max_threads = 0;
    long long line_max = 0;
    if (bt_config_int(config, "trace.ring_bytes", &ring_bytes) != 0 ||
        bt_config_int(config, "trace.max_threads", &max_threads) != 0 ||
        bt_config_int(config, "trace.line_max", &line_max) != 0) {
        return;
    }
    if (bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max) != 0) {
        return;
    }
    bt_trace_install_crash_handler();
}

int btvd_run(const btvd_env *env, char *err, size_t err_len) {
    BT_TRACE_ENTER("visual_diff.run");
    btvd_cli cli;
    if (btvd_parse_cli(env->argv, env->argc, &cli) != 0) {
        snprintf(err, err_len, "%s", BTVD_USAGE_TEXT);
        BT_TRACE_FAIL("visual_diff.run", "cli parse rejected");
        BT_TRACE_EXIT("visual_diff.run");
        return -1;
    }
    if (cli.kind == BTVD_CLI_SELFTEST) {
        int rc = btvd_run_selftest(cli.workdir, err, err_len);
        BT_TRACE_EXIT("visual_diff.run");
        return rc;
    }
    char config_path[BT_PATH_MAX];
    btvd_config_path_from(env->config, env->root, config_path);
    bt_json_value *config = btvd_read_config(config_path, err, err_len);
    if (config == NULL) {
        BT_TRACE_EXIT("visual_diff.run");
        return -1;
    }
    trace_from_config(config);
    uint16_t tolerance = 0;
    if (btvd_visual_cfg_from(config, &tolerance, err, err_len) != 0) {
        bt_json_free(config);
        bt_trace_shutdown();
        BT_TRACE_EXIT("visual_diff.run");
        return -1;
    }
    if (cli.args.update) {
        btvd_strvec updated = {0};
        char scratch[BTVD_ERR_MAX];
        if (btvd_run_update(&cli.args, &updated, scratch, sizeof scratch) != 0) {
            snprintf(err, err_len, "update failed: %s", scratch);
            BT_TRACE_FAIL("visual_diff.run", "%s", err);
            btvd_strvec_free(&updated);
            bt_json_free(config);
            bt_trace_shutdown();
            BT_TRACE_EXIT("visual_diff.run");
            return -1;
        }
        printf("updated %zu baselines\n", updated.count);
        for (size_t at = 0; at < updated.count; at++) {
            printf("updated %s\n", updated.items[at]);
        }
        btvd_strvec_free(&updated);
        bt_json_free(config);
        bt_trace_shutdown();
        BT_TRACE_EXIT("visual_diff.run");
        return 0;
    }
    btvd_report_vec reports = {0};
    char scratch[BTVD_ERR_MAX];
    if (btvd_run_compare(&cli.args, tolerance, &reports, scratch, sizeof scratch) != 0) {
        snprintf(err, err_len, "compare failed: %s", scratch);
        BT_TRACE_FAIL("visual_diff.run", "%s", err);
        btvd_report_vec_free(&reports);
        bt_json_free(config);
        bt_trace_shutdown();
        BT_TRACE_EXIT("visual_diff.run");
        return -1;
    }
    bt_strbuf text;
    bt_strbuf_init(&text, 256);
    btvd_render_report(&reports, tolerance, cli.args.screenshots, cli.args.baselines, &text);
    char artifacts_root[BT_PATH_MAX];
    btvd_artifacts_root_from(env->artifacts, config, env->root, artifacts_root,
                             sizeof artifacts_root);
    char report_path[BT_PATH_MAX];
    if (btvd_write_report(text.data, artifacts_root, report_path, sizeof report_path, scratch,
                          sizeof scratch) != 0) {
        snprintf(err, err_len, "cannot write report: %s", scratch);
        BT_TRACE_FAIL("visual_diff.run", "%s", err);
        bt_strbuf_free(&text);
        btvd_report_vec_free(&reports);
        bt_json_free(config);
        bt_trace_shutdown();
        BT_TRACE_EXIT("visual_diff.run");
        return -1;
    }
    printf("report: %s\n", report_path);
    int failed = 0;
    for (size_t at = 0; at < reports.count; at++) {
        char label[BTVD_ERR_MAX];
        btvd_verdict_label(&reports.items[at].verdict, label, sizeof label);
        printf("%s: %s\n", reports.items[at].name, label);
        failed |= btvd_verdict_is_failure(&reports.items[at].verdict);
    }
    bt_strbuf_free(&text);
    btvd_report_vec_free(&reports);
    bt_json_free(config);
    bt_trace_shutdown();
    BT_TRACE_EXIT("visual_diff.run");
    return failed ? BTVD_EXIT_DIFF : 0;
}
