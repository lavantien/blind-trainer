#ifndef BT_FUZZ_CLASSIFY_H
#define BT_FUZZ_CLASSIFY_H

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/rng.h"
#include "scheme/classifier.h"
#include "scheme/core_deps.h"
#include "scheme/memo.h"
#include "scheme/scheme.h"
#include "scheme/scramble.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BT_FUZZ_GOLDEN 0x9E3779B97F4A7C15ULL

static uint64_t bt_fuzz_seed_from(const uint8_t *data, size_t size) {
    uint8_t bytes[8] = {0};
    size_t take = size < 8 ? size : 8;
    for (size_t i = 0; i < take; i++) {
        bytes[i] = data[i];
    }
    uint64_t value;
    memcpy(&value, bytes, sizeof value);
    return value ^ ((uint64_t)size * BT_FUZZ_GOLDEN);
}

static void bt_fuzz_shuffle(bt_rng *rng, uint8_t *values, size_t n) {
    for (size_t i = n; i > 1; i--) {
        size_t j = (size_t)bt_rng_below(rng, (uint64_t)i);
        uint8_t tmp = values[i - 1];
        values[i - 1] = values[j];
        values[j] = tmp;
    }
}

static void bt_fuzz_repaired_occ(uint64_t seed, uint8_t occ[54]) {
    bt_rng rng;
    bt_rng_seed(&rng, seed);
    uint8_t corners[8];
    uint8_t edges[12];
    for (uint8_t i = 0; i < 8; i++) {
        corners[i] = i;
    }
    for (uint8_t i = 0; i < 12; i++) {
        edges[i] = i;
    }
    bt_fuzz_shuffle(&rng, corners, 8);
    bt_fuzz_shuffle(&rng, edges, 12);
    for (uint8_t i = 0; i < 54; i++) {
        occ[i] = i;
    }
    for (uint8_t slot = 0; slot < 8; slot++) {
        uint8_t piece[3];
        uint8_t positions[3];
        bt_corner_stickers(corners[slot], piece);
        bt_corner_stickers(slot, positions);
        size_t twist = (size_t)bt_rng_below(&rng, 3);
        for (uint8_t j = 0; j < 3; j++) {
            occ[positions[(j + twist) % 3]] = piece[j];
        }
    }
    for (uint8_t slot = 0; slot < 12; slot++) {
        uint8_t occupants[2];
        uint8_t positions[2];
        bt_edge_stickers(edges[slot], occupants);
        bt_edge_stickers(slot, positions);
        if (bt_rng_below(&rng, 2) == 1) {
            occ[positions[0]] = occupants[1];
            occ[positions[1]] = occupants[0];
        } else {
            occ[positions[0]] = occupants[0];
            occ[positions[1]] = occupants[1];
        }
    }
}

static bool bt_fuzz_within_bounds(void) {
    static bool inited;
    static bool has_runs;
    static long long runs;
    static bool has_secs;
    static long long secs;
    static clock_t start;
    static uint64_t count;
    if (!inited) {
        const bt_json_value *config = bt_config_load();
        long long value = 0;
        has_runs = (bt_config_int(config, "fuzz.runs", &value) == 0 && value > 0) != 0;
        runs = value;
        value = 0;
        has_secs = (bt_config_int(config, "fuzz.secs", &value) == 0 && value > 0) != 0;
        secs = value;
        start = clock();
        inited = true;
    }
    count++;
    if (has_runs && count > (uint64_t)runs) {
        return false;
    }
    if (has_secs && (double)(clock() - start) / CLOCKS_PER_SEC >= (double)secs) {
        return false;
    }
    return true;
}

static void bt_fuzz_classify_once(const uint8_t *data, size_t size) {
    bt_cube solved = bt_cube_solved();
    uint8_t solved_occ[54];
    bt_scheme_state_array(&solved, solved_occ);
    bt_report solved_report;
    bt_scheme_classify(solved_occ, &solved_report);
    if (solved_report.class.kind != BT_CLASS_SOLVED ||
        !bt_scheme_trace_is_solved(&solved_report.edge) ||
        !bt_scheme_trace_is_solved(&solved_report.corner)) {
        BT_ABORT("solved cube must classify solved");
    }
    char position[BT_CLASS_POSITION_MAX];
    if (bt_scheme_report_position(&solved_report, position)) {
        BT_ABORT("solved cube must have no position");
    }
    uint8_t occ[54];
    bt_fuzz_repaired_occ(bt_fuzz_seed_from(data, size), occ);
    bt_cube cube = bt_cube_from_occ(occ);
    uint8_t cube_occ[54];
    bt_scheme_state_array(&cube, cube_occ);
    bt_report report;
    bt_scheme_classify(cube_occ, &report);
    (void)bt_scheme_report_position(&report, position);
    bt_trace edge_trace;
    bt_trace corner_trace;
    bt_scheme_trace(cube_occ, BT_ORBIT_EDGE, &edge_trace);
    bt_scheme_trace(cube_occ, BT_ORBIT_CORNER, &corner_trace);
    bt_case cases[BT_TRACE_BLOCKS_MAX];
    size_t edge_count = bt_scheme_trace_cases(&edge_trace, cases);
    for (size_t i = 0; i < edge_count; i++) {
        if (cases[i].t1 < bt_scheme_orbit_lo(BT_ORBIT_EDGE) ||
            cases[i].t1 >= bt_scheme_orbit_hi(BT_ORBIT_EDGE) ||
            cases[i].t2 < bt_scheme_orbit_lo(BT_ORBIT_EDGE) ||
            cases[i].t2 >= bt_scheme_orbit_hi(BT_ORBIT_EDGE)) {
            BT_ABORT("edge case left its orbit: t1 %u t2 %u", cases[i].t1, cases[i].t2);
        }
    }
    size_t corner_count = bt_scheme_trace_cases(&corner_trace, cases);
    for (size_t i = 0; i < corner_count; i++) {
        if (cases[i].t1 < bt_scheme_orbit_lo(BT_ORBIT_CORNER) ||
            cases[i].t1 >= bt_scheme_orbit_hi(BT_ORBIT_CORNER) ||
            cases[i].t2 < bt_scheme_orbit_lo(BT_ORBIT_CORNER) ||
            cases[i].t2 >= bt_scheme_orbit_hi(BT_ORBIT_CORNER)) {
            BT_ABORT("corner case left its orbit: t1 %u t2 %u", cases[i].t1, cases[i].t2);
        }
    }
    edge_count = bt_scheme_trace_cases(&report.edge, cases);
    for (size_t i = 0; i < edge_count; i++) {
        if (cases[i].t1 < bt_scheme_orbit_lo(BT_ORBIT_EDGE) ||
            cases[i].t1 >= bt_scheme_orbit_hi(BT_ORBIT_EDGE) ||
            cases[i].t2 < bt_scheme_orbit_lo(BT_ORBIT_EDGE) ||
            cases[i].t2 >= bt_scheme_orbit_hi(BT_ORBIT_EDGE)) {
            BT_ABORT("report edge case left its orbit: t1 %u t2 %u", cases[i].t1, cases[i].t2);
        }
    }
    corner_count = bt_scheme_trace_cases(&report.corner, cases);
    for (size_t i = 0; i < corner_count; i++) {
        if (cases[i].t1 < bt_scheme_orbit_lo(BT_ORBIT_CORNER) ||
            cases[i].t1 >= bt_scheme_orbit_hi(BT_ORBIT_CORNER) ||
            cases[i].t2 < bt_scheme_orbit_lo(BT_ORBIT_CORNER) ||
            cases[i].t2 >= bt_scheme_orbit_hi(BT_ORBIT_CORNER)) {
            BT_ABORT("report corner case left its orbit: t1 %u t2 %u", cases[i].t1, cases[i].t2);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!bt_fuzz_within_bounds()) {
        return 0;
    }
    bt_fuzz_classify_once(data, size);
    return 0;
}

#endif
