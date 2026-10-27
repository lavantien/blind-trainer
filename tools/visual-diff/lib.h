#ifndef VISUAL_DIFF_LIB_H
#define VISUAL_DIFF_LIB_H

#include <stddef.h>
#include <stdint.h>

#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"

#define BTVD_ERR_MAX 512
#define BTVD_NAME_MAX 260
#define BTVD_EXIT_USAGE 2
#define BTVD_EXIT_DIFF 1
#define BTVD_USAGE_TEXT                                                                            \
    "usage: bt-visual-diff <screenshots-dir> <baselines-dir> [--update] | "                        \
    "--selftest <workdir>"

typedef struct {
    char screenshots[BT_PATH_MAX];
    char baselines[BT_PATH_MAX];
    int update;
} btvd_args;

typedef enum { BTVD_CLI_COMPARE, BTVD_CLI_SELFTEST } btvd_cli_kind;

typedef struct {
    btvd_cli_kind kind;
    btvd_args args;
    char workdir[BT_PATH_MAX];
} btvd_cli;

int btvd_parse_args(char *const *argv, size_t argc, btvd_args *out);
int btvd_parse_cli(char *const *argv, size_t argc, btvd_cli *out);

typedef enum { BTVD_SIDE_SHOT, BTVD_SIDE_BASE } btvd_side;

const char *btvd_side_label(btvd_side side);

typedef enum {
    BTVD_VERDICT_OK,
    BTVD_VERDICT_OVER,
    BTVD_VERDICT_DIM_MISMATCH,
    BTVD_VERDICT_MISSING,
    BTVD_VERDICT_DECODE,
} btvd_verdict_kind;

typedef struct {
    btvd_verdict_kind kind;
    btvd_side side;
    uint64_t pixels_over;
    uint32_t shot_width;
    uint32_t shot_height;
    uint32_t base_width;
    uint32_t base_height;
    char error[BTVD_ERR_MAX];
} btvd_verdict;

int btvd_verdict_is_failure(const btvd_verdict *verdict);
void btvd_verdict_label(const btvd_verdict *verdict, char *out, size_t out_len);

typedef struct {
    char name[BTVD_NAME_MAX];
    btvd_verdict verdict;
    int has_numbers;
    uint16_t max_delta;
    uint64_t pixels_over;
    uint64_t pixels;
} btvd_pair_report;

typedef struct {
    uint16_t max_delta;
    uint64_t pixels_over;
} btvd_comparison;

typedef struct {
    unsigned char *pixels;
    uint32_t width;
    uint32_t height;
} btvd_image;

void btvd_compare_images(const btvd_image *shot, const btvd_image *base, uint16_t tolerance,
                         btvd_comparison *out);
int btvd_decode(const char *path, btvd_image *out, char *err, size_t err_len);
btvd_pair_report btvd_compare_pair(const char *name, const char *shot, const char *base,
                                   uint16_t tolerance);

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} btvd_strvec;

void btvd_strvec_push(btvd_strvec *vec, char *item);
void btvd_strvec_sort(btvd_strvec *vec);
void btvd_strvec_free(btvd_strvec *vec);

int btvd_png_names(const char *dir, btvd_strvec *names, char *err, size_t err_len);
int btvd_pair_names(const char *shot_dir, const char *base_dir, btvd_strvec *names, char *err,
                    size_t err_len);

typedef struct {
    btvd_pair_report *items;
    size_t count;
    size_t cap;
} btvd_report_vec;

void btvd_report_vec_push(btvd_report_vec *vec, btvd_pair_report report);
void btvd_report_vec_free(btvd_report_vec *vec);

int btvd_run_compare(const btvd_args *args, uint16_t tolerance, btvd_report_vec *reports, char *err,
                     size_t err_len);
int btvd_run_update(const btvd_args *args, btvd_strvec *updated, char *err, size_t err_len);

void btvd_render_report(const btvd_report_vec *reports, uint16_t tolerance, const char *shot_dir,
                        const char *base_dir, bt_strbuf *out);
int btvd_write_report(const char *text, const char *artifacts_root, char *out_path, size_t out_len,
                      char *err, size_t err_len);

void btvd_config_path_from(const char *env_value, const char *root, char out[BT_PATH_MAX]);
bt_json_value *btvd_read_config(const char *path, char *err, size_t err_len);
int btvd_visual_cfg_from(const bt_json_value *config, uint16_t *tolerance, char *err,
                         size_t err_len);
int btvd_raylib_log_level_from(const bt_json_value *config, int *level, char *err, size_t err_len);
int btvd_artifacts_root_from(const char *env_value, const bt_json_value *config, const char *root,
                             char *out, size_t out_len);

typedef struct {
    char **argv;
    size_t argc;
    const char *config;
    const char *artifacts;
    char root[BT_PATH_MAX];
} btvd_env;

void btvd_env_from_process(btvd_env *env, char **argv, size_t argc, const char *config,
                           const char *artifacts);
int btvd_run(const btvd_env *env, char *err, size_t err_len);

#endif
