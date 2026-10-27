#ifndef BT_FUZZ_GA_H
#define BT_FUZZ_GA_H

#include "common/bt_harness.h"
#include "fuzz/fuzz_body.h"
#include "fuzz/fuzz_common.h"
#include "ga/dist.h"
#include "ga/features.h"
#include "ga/ga.h"
#include "ga/inventory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static size_t bt_fuzz_ga_truncated_tail_inputs;

static bool bt_fuzz_ga_truncated_tail(const uint8_t *data, size_t size) {
    if (size == 0) {
        return false;
    }
    uint8_t last = data[size - 1];
    return (last & 0xE0U) == 0xC0U || (last & 0xF0U) == 0xE0U || (last & 0xF8U) == 0xF0U;
}

static void bt_fuzz_ga_input_path(char *out, size_t out_size) {
    char dir[BT_PATH_MAX];
    fuzz_fuzz_dir(dir);
    snprintf(out, out_size, "%s\\ga-input.bin", dir);
}

static void bt_fuzz_ga_write_input(const uint8_t *data, size_t size) {
    char path[BT_PATH_MAX];
    bt_fuzz_ga_input_path(path, sizeof path);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("fuzz ga: cannot write %s", path);
    }
    if (size > 0) {
        fwrite(data, 1, size, file);
    }
    fclose(file);
}

static void bt_fuzz_ga_once(const uint8_t *data, size_t size) {
    char text[256];
    size_t take = size < sizeof text - 1 ? size : sizeof text - 1;
    memcpy(text, data, take);
    text[take] = '\0';
    if (bt_fuzz_ga_truncated_tail(data, size)) {
        bt_fuzz_ga_truncated_tail_inputs++;
    }
    bt_ga_features out;
    (void)bt_ga_parse(text, &out);
    bt_fuzz_ga_write_input(data, size);
    char path[BT_PATH_MAX];
    bt_fuzz_ga_input_path(path, sizeof path);
    char err[BT_GA_ERR_MAX];
    bt_ga_strset wordlist;
    (void)bt_ga_load_wordlist(path, &wordlist, err);
    bt_ga_strset_free(&wordlist);
    bt_ga_freqs freqs;
    (void)bt_ga_load_syllable_frequencies(path, &freqs, err);
    bt_ga_freqs_free(&freqs);
    bt_ga_freq_data freq_data;
    (void)bt_ga_load_freq_data(path, &freq_data, err);
    bt_ga_freq_data_free(&freq_data);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!bt_fuzz_within_bounds()) {
        return 0;
    }
    bt_fuzz_ga_once(data, size);
    return 0;
}

#endif
