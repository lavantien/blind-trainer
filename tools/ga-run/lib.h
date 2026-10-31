#ifndef BT_GR_LIB_H
#define BT_GR_LIB_H

#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "ga/dist.h"
#include "ga/ga.h"
#include "ga/hash.h"
#include "ga/inventory.h"

#include <stddef.h>
#include <stdint.h>

#define BT_GR_VIETNAMESE_DIR "vietnamese"
#define BT_GR_WORDLIST_FILE "syllables-7184-abc.txt"
#define BT_GR_FREQS_FILE "vn-word-frequencies.tsv"
#define BT_GR_POOL_TOP_MAX 10

const char *bt_gr_tool_id(void);

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} bt_gr_offenders;

int bt_gr_offenders_push(bt_gr_offenders *list, const char *fmt, ...);
void bt_gr_offenders_free(bt_gr_offenders *list);

int bt_gr_config_path(const char *relative, char out[BT_PATH_MAX]);

typedef struct {
    char orbit[16];
    char letters[8];
    char syllable[BT_GA_TEXT_MAX];
    uint64_t frequency;
} bt_gr_syllable_case;

typedef struct {
    bt_gr_syllable_case *cases;
    size_t cases_count;
    size_t pool_size;
    uint64_t seed;
    float best_fitness;
    char freq_sha256[BT_GA_SHA256_HEX_LEN];
    char syllables_sha256[BT_GA_SHA256_HEX_LEN];
} bt_gr_syllable_assignment;

typedef struct {
    const bt_json_value *run_config;
    bt_ga_inventory_report inventory;
    size_t pool_size;
    bt_ga_pool_entry pool_top[BT_GR_POOL_TOP_MAX];
    size_t pool_top_count;
    float best_fitness;
    float best_parts[3];
    size_t best_generation;
    size_t generations_run;
    float *per_gen_best;
    size_t per_gen_count;
    uint32_t stagnation_tail;
    uint32_t max_stagnation;
    uint64_t evaluations;
    char freq_sha256[BT_GA_SHA256_HEX_LEN];
    char syllables_sha256[BT_GA_SHA256_HEX_LEN];
} bt_gr_ga_report;

typedef struct {
    bt_ga_params params;
    bt_ga_upgrades upgrades;
    bt_ga_env env;
    bt_ga_pool_entry *pool;
    size_t pool_count;
    bt_ga_inventory_report inventory_report;
    bt_ga_freq_data freq;
    char *freq_bytes;
    size_t freq_bytes_len;
} bt_gr_run_inputs;

int bt_gr_load_run_inputs(bt_gr_run_inputs *inputs, bt_gr_offenders *offenders);
void bt_gr_run_inputs_free(bt_gr_run_inputs *inputs);

int bt_gr_cross_check_cases(const bt_json_value *config, bt_gr_offenders *offenders);

int bt_gr_build_assignment(const bt_gr_run_inputs *inputs, const bt_ga_run_outcome *outcome,
                           bt_gr_syllable_assignment *assignment, bt_gr_offenders *offenders);
void bt_gr_assignment_free(bt_gr_syllable_assignment *assignment);

void bt_gr_build_report(const bt_gr_run_inputs *inputs, const bt_ga_run_outcome *outcome,
                        const bt_gr_syllable_assignment *assignment, const bt_json_value *config,
                        bt_gr_ga_report *report);
void bt_gr_report_free(bt_gr_ga_report *report);

int bt_gr_render_markdown(const bt_gr_ga_report *report, bt_strbuf *out);

int bt_gr_run_all(bt_gr_offenders *offenders);

#endif
