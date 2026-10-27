#ifndef GD_LIB_H
#define GD_LIB_H

#include <stddef.h>

#include "common/json.h"
#include "common/paths.h"
#include "gendata/synth.h"

#define BT_GD_EDGE_CASES 440
#define BT_GD_CORNER_CASES 378
#define BT_GD_TOTAL_CASES 818
#define BT_GD_EDGE_COVERED 372
#define BT_GD_CORNER_COVERED 288
#define BT_GD_EDGE_GENERATED 68
#define BT_GD_CORNER_GENERATED 89
#define BT_GD_EDGE_TOTAL_COVERED 440
#define BT_GD_CORNER_TOTAL_COVERED 377
#define BT_GD_TOTAL_COVERED 817
#define BT_GD_TOTAL_SPECIALS 89

#define BT_GD_SPECIAL_COUNTS_LEN 8

#define BT_GD_SYNTH_ALGORITHM "first-setup-plateau-1"

#define BT_GD_ERR_MAX 192

typedef struct {
    const char *solver;
    const char *class_name;
    size_t rows;
} bt_gd_special_count;

extern const bt_gd_special_count BT_GD_SPECIAL_COUNTS[BT_GD_SPECIAL_COUNTS_LEN];

const char *bt_gd_crate_id(void);

int bt_gd_synth_params_sha256(const bt_json_value *config, const char *generator, char out[65],
                              char *err, size_t err_len);
int bt_gd_limits_from_config(const bt_json_value *config, bt_gd_synth_limits *out, char *err,
                             size_t err_len);
int bt_gd_data_vendor_path(const bt_json_value *config, char out[BT_PATH_MAX], char *err,
                           size_t err_len);

#endif
