#ifndef BT_GA_INVENTORY_H
#define BT_GA_INVENTORY_H

#include <stddef.h>
#include <stdint.h>

#include "common/json.h"

#define BT_GA_ERR_MAX 128
#define BT_GA_TEXT_MAX 16

typedef enum {
    BT_GA_TONE_NGANG,
    BT_GA_TONE_HUYEN,
    BT_GA_TONE_SAC,
    BT_GA_TONE_HOI,
    BT_GA_TONE_NGA,
    BT_GA_TONE_NANG,
} bt_ga_tone;

typedef enum {
    BT_GA_CODA_OPEN,
    BT_GA_CODA_P,
    BT_GA_CODA_T,
    BT_GA_CODA_K,
    BT_GA_CODA_M,
    BT_GA_CODA_N,
    BT_GA_CODA_NG,
} bt_ga_coda;

typedef enum {
    BT_GA_NUC_I,
    BT_GA_NUC_UW,
    BT_GA_NUC_U,
    BT_GA_NUC_EH,
    BT_GA_NUC_OR,
    BT_GA_NUC_OH,
    BT_GA_NUC_E,
    BT_GA_NUC_AH,
    BT_GA_NUC_A,
    BT_GA_NUC_AB,
    BT_GA_NUC_O,
    BT_GA_NUC_IA,
    BT_GA_NUC_WA,
    BT_GA_NUC_UA,
} bt_ga_nucleus;

typedef enum {
    BT_GA_ONSET_B,
    BT_GA_ONSET_DD,
    BT_GA_ONSET_T,
    BT_GA_ONSET_TH,
    BT_GA_ONSET_CH,
    BT_GA_ONSET_TR,
    BT_GA_ONSET_K,
    BT_GA_ONSET_M,
    BT_GA_ONSET_N,
    BT_GA_ONSET_NH,
    BT_GA_ONSET_NG,
    BT_GA_ONSET_PH,
    BT_GA_ONSET_X,
    BT_GA_ONSET_S,
    BT_GA_ONSET_KH,
    BT_GA_ONSET_H,
    BT_GA_ONSET_V,
    BT_GA_ONSET_Z,
    BT_GA_ONSET_G,
    BT_GA_ONSET_L,
    BT_GA_ONSET_ZERO,
} bt_ga_onset;

size_t bt_ga_case_count(void);

typedef struct {
    char **items;
    size_t count;
} bt_ga_strset;

int bt_ga_load_wordlist(const char *path, bt_ga_strset *out, char err[BT_GA_ERR_MAX]);
void bt_ga_strset_free(bt_ga_strset *set);
int bt_ga_strset_contains(const bt_ga_strset *set, const char *text);

typedef struct {
    char **keys;
    uint64_t *values;
    size_t count;
} bt_ga_freqs;

int bt_ga_load_syllable_frequencies(const char *path, bt_ga_freqs *out, char err[BT_GA_ERR_MAX]);
void bt_ga_freqs_free(bt_ga_freqs *freqs);
uint64_t bt_ga_freqs_get(const bt_ga_freqs *freqs, const char *key);

typedef struct {
    char text[BT_GA_TEXT_MAX];
    bt_ga_onset onset;
    bt_ga_nucleus nucleus;
    bt_ga_coda coda;
    bt_ga_tone tone;
} bt_ga_inventory_entry;

typedef struct {
    size_t generated;
    size_t kept;
    size_t dropped_not_in_wordlist;
    size_t wordlist_size;
    size_t wordlist_covered;
} bt_ga_inventory_report;

void bt_ga_build_inventory(const bt_ga_strset *wordlist, bt_ga_inventory_entry **entries,
                           size_t *count, bt_ga_inventory_report *report);
void bt_ga_inventory_free(bt_ga_inventory_entry *entries);

int bt_ga_nucleus_rime(bt_ga_nucleus nucleus, bt_ga_coda coda, const char **rime, size_t *mark_at);
const char *bt_ga_tone_char(const char *base, bt_ga_tone tone);

typedef struct {
    const char *items[4];
    size_t count;
} bt_ga_onset_spelling_list;

void bt_ga_onset_spellings(bt_ga_onset onset, bt_ga_nucleus nucleus,
                           bt_ga_onset_spelling_list *out);

typedef struct {
    char items[6][BT_GA_TEXT_MAX];
    size_t count;
} bt_ga_form_list;

void bt_ga_zero_onset_variants(bt_ga_nucleus nucleus, bt_ga_coda coda, bt_ga_tone tone,
                               bt_ga_form_list *out);
int bt_ga_render(const char *spelling, bt_ga_nucleus nucleus, bt_ga_coda coda, bt_ga_tone tone,
                 char out[BT_GA_TEXT_MAX]);

typedef struct {
    char text[BT_GA_TEXT_MAX];
    uint64_t frequency;
} bt_ga_pool_entry;

void bt_ga_build_pool(const bt_ga_inventory_entry *entries, size_t count, const bt_ga_freqs *freqs,
                      size_t top_k, bt_ga_pool_entry **pool, size_t *pool_count);
void bt_ga_pool_free(bt_ga_pool_entry *pool);

int bt_ga_config_pool_top_k(const bt_json_value *config, size_t *out, char err[BT_GA_ERR_MAX]);
int bt_ga_is_nfc(const char *text);

#endif
