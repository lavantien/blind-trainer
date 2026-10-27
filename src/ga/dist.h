#ifndef BT_GA_DIST_H
#define BT_GA_DIST_H

#include <stddef.h>
#include <stdint.h>

#include "common/json.h"

#include "ga/inventory.h"

typedef struct {
    char orbit[16];
    char letters[8];
    uint64_t count;
} bt_ga_case_row;

typedef struct {
    uint32_t from;
    uint32_t to;
    uint64_t count;
} bt_ga_pair_count;

typedef struct {
    bt_ga_case_row *cases;
    size_t cases_count;
    bt_ga_pair_count *adjacent;
    size_t adjacent_count;
    bt_ga_pair_count *cooccurring;
    size_t cooccurring_count;
    uint64_t total_draws;
    uint64_t seed;
} bt_ga_freq_data;

int bt_ga_load_freq_data(const char *path, bt_ga_freq_data *out, char err[BT_GA_ERR_MAX]);
void bt_ga_freq_data_free(bt_ga_freq_data *data);
int bt_ga_freq_data_eq(const bt_ga_freq_data *a, const bt_ga_freq_data *b);

int bt_ga_config_memofreq(const bt_json_value *config, uint64_t *samples, uint64_t *seed,
                          char err[BT_GA_ERR_MAX]);

#endif
