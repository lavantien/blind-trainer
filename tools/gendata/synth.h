#ifndef GD_SYNTH_H
#define GD_SYNTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/alg.h"
#include "gendata/model.h"
#include "gendata/scoring.h"
#include "scheme/scheme.h"

typedef struct {
    size_t max_setup;
    size_t max_insert;
    size_t node_budget;
} bt_gd_synth_limits;

typedef struct {
    bt_alg alg;
    char *commutator;
} bt_gd_synth_hit;

typedef struct {
    bt_gd_synth_hit *items;
    size_t count;
    size_t cap_alloc;
} bt_gd_synth_hits;

void bt_gd_synth_hit_free(bt_gd_synth_hit *hit);
void bt_gd_synth_hits_free(bt_gd_synth_hits *hits);
int bt_gd_synth_candidates(bt_orbit orbit, uint8_t t1, uint8_t t2, const bt_gd_synth_limits *limits,
                           bt_gd_synth_hits *out);
bool bt_gd_synth_case(bt_orbit orbit, uint8_t t1, uint8_t t2, const bt_gd_synth_limits *limits,
                      const bt_gd_scoring_cfg *scoring, bt_gd_synth_hit *out);
void bt_gd_synthesize(bt_gd_cases_file *cases, const bt_gd_synth_limits *limits,
                      const bt_gd_scoring_cfg *scoring, size_t *edge_added, size_t *corner_added);

#endif
