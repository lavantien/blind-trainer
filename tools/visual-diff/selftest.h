#ifndef VISUAL_DIFF_SELFTEST_H
#define VISUAL_DIFF_SELFTEST_H

#include <stddef.h>
#include <stdint.h>

#include "lib.h"

#define BTVD_SELFTEST_TOLERANCE 16
#define BTVD_SELFTEST_WIDTH 4
#define BTVD_SELFTEST_HEIGHT 4
#define BTVD_SELFTEST_OVER_X 2
#define BTVD_SELFTEST_OVER_Y 1
#define BTVD_ALPHA_OPAQUE 255

typedef struct {
    char name[BTVD_NAME_MAX];
    const char *expected;
    const char *actual;
    int passed;
} btvd_selftest_check;

typedef struct {
    btvd_selftest_check items[4];
    size_t count;
} btvd_selftest_checks;

void btvd_over_pixel(uint16_t tolerance, unsigned char out[4]);
void btvd_within_pixel(uint16_t tolerance, unsigned char out[4]);
const char *btvd_verdict_kind_label(const btvd_verdict *verdict);
void btvd_evaluate(const btvd_report_vec *reports, btvd_selftest_checks *out);
int btvd_selftest_exit_code(const btvd_selftest_checks *checks);
int btvd_write_selftest_config(const char *workdir, char *err, size_t err_len);
int btvd_tolerance_from_config(const char *workdir, uint16_t *tolerance, char *err, size_t err_len);
int btvd_materialize(const char *workdir, uint16_t tolerance, char shots[BT_PATH_MAX],
                     char bases[BT_PATH_MAX], char *err, size_t err_len);
int btvd_execute(const char *shots, const char *bases, uint16_t tolerance,
                 btvd_selftest_checks *checks, char *err, size_t err_len);
int btvd_run_selftest(const char *workdir, char *err, size_t err_len);

#endif
