#ifndef BT_GA_FEATURES_H
#define BT_GA_FEATURES_H

#include <stddef.h>

#include "ga/inventory.h"

#define BT_GA_PARSE_MAX 128

typedef enum {
    BT_GA_PLACE_LABIAL,
    BT_GA_PLACE_DENTAL,
    BT_GA_PLACE_RETROFLEX,
    BT_GA_PLACE_PALATAL,
    BT_GA_PLACE_VELAR,
    BT_GA_PLACE_GLOTTAL,
} bt_ga_place;

typedef enum {
    BT_GA_MANNER_STOP,
    BT_GA_MANNER_ASPIRATED,
    BT_GA_MANNER_IMPLOSIVE,
    BT_GA_MANNER_AFFRICATE,
    BT_GA_MANNER_NASAL,
    BT_GA_MANNER_FRICATIVE,
    BT_GA_MANNER_APPROXIMANT,
} bt_ga_manner;

typedef struct {
    bt_ga_place place;
    bt_ga_manner manner;
} bt_ga_onset_features;

typedef struct {
    bt_ga_tone tone;
    int has_onset;
    bt_ga_onset_features onset;
    bt_ga_nucleus nucleus;
    bt_ga_coda coda;
} bt_ga_features;

int bt_ga_parse(const char *text, bt_ga_features *out);
int bt_ga_onset_features_of(bt_ga_onset onset, bt_ga_onset_features *out);
float bt_ga_confusion(const bt_ga_features *a, const bt_ga_features *b);
float bt_ga_flow_penalty(const bt_ga_features *a, size_t a_len, const bt_ga_features *b,
                         size_t b_len);

typedef struct {
    const char *marked;
    const char *base;
    bt_ga_tone tone;
} bt_ga_tone_marked_row;

typedef struct {
    const char *letters;
    bt_ga_nucleus nucleus;
    bt_ga_coda coda;
} bt_ga_rime_row;

const bt_ga_tone_marked_row *bt_ga_tone_marked_table(size_t *count);
const bt_ga_rime_row *bt_ga_rimes_table(size_t *count);

#endif
