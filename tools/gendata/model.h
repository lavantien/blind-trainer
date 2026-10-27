#ifndef GD_MODEL_H
#define GD_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/json.h"
#include "common/strbuf.h"

typedef struct {
    char *solver;
    char *sheet;
    size_t rows;
    char *sha256;
} bt_gd_corpus_file_entry;

typedef struct {
    char *solver;
    size_t rowid;
    char *position;
    char *alg;
    uint32_t qtm;
    uint32_t htm;
    bool ok;
    char **problems;
    size_t problem_count;
    char **notes;
    size_t note_count;
    char *commutator;
    char *recon_link;
    char *youtube_link;
} bt_gd_vendor_alg;

typedef struct {
    char *orbit;
    uint8_t t1;
    uint8_t t2;
    char t1_letter;
    char t2_letter;
    char *t1_name;
    char *t2_name;
    bool covered;
    bt_gd_vendor_alg *vendor_algs;
    size_t vendor_count;
} bt_gd_case_entry;

typedef struct {
    bt_gd_case_entry *cases;
    size_t count;
} bt_gd_cases_file;

typedef struct {
    char *class_name;
    char *solver;
    size_t rowid;
    char *position;
    char *alg;
    uint32_t qtm;
    uint32_t htm;
    bool ok;
    char **problems;
    size_t problem_count;
    char **notes;
    size_t note_count;
    char *commutator;
    char *recon_link;
    char *youtube_link;
} bt_gd_special_entry;

typedef struct {
    bt_gd_special_entry *specials;
    size_t count;
} bt_gd_specials_file;

typedef struct {
    size_t total;
    size_t covered;
    size_t uncovered;
} bt_gd_orbit_coverage;

typedef struct {
    char *solver;
    char *class_name;
    size_t rows;
} bt_gd_class_count;

typedef struct {
    char *path;
    char *sha256;
    size_t entries;
    bt_gd_orbit_coverage edge;
    bt_gd_orbit_coverage corner;
} bt_gd_cases_info;

typedef struct {
    char *path;
    char *sha256;
    size_t entries;
    bt_gd_class_count *counts;
    size_t count_count;
} bt_gd_specials_info;

typedef struct {
    char *generated_at;
    char *generator;
    char *synth_params_sha256;
    char *corpus_sha256;
    size_t corpus_rows;
    bt_gd_corpus_file_entry *corpus_files;
    size_t corpus_file_count;
    size_t rows_in_pair_space;
    size_t rows_other_buffer;
    bt_gd_cases_info cases;
    bt_gd_specials_info specials;
} bt_gd_manifest;

int bt_gd_json_pretty(const bt_json_value *value, bt_strbuf *out);

void bt_gd_string_list_free(char **items, size_t count);

void bt_gd_vendor_alg_free(bt_gd_vendor_alg *alg);
bool bt_gd_vendor_alg_eq(const bt_gd_vendor_alg *a, const bt_gd_vendor_alg *b);
bt_json_value *bt_gd_vendor_alg_to_json(const bt_gd_vendor_alg *alg);
int bt_gd_vendor_alg_from_json(const bt_json_value *value, bt_gd_vendor_alg *out, char *err,
                               size_t err_len);

void bt_gd_cases_file_free(bt_gd_cases_file *file);
bool bt_gd_cases_file_eq(const bt_gd_cases_file *a, const bt_gd_cases_file *b);
bt_json_value *bt_gd_cases_file_to_json(const bt_gd_cases_file *file);
int bt_gd_cases_file_from_json(const bt_json_value *value, bt_gd_cases_file *out, char *err,
                               size_t err_len);

void bt_gd_specials_file_free(bt_gd_specials_file *file);
bool bt_gd_specials_file_eq(const bt_gd_specials_file *a, const bt_gd_specials_file *b);
bt_json_value *bt_gd_specials_file_to_json(const bt_gd_specials_file *file);
int bt_gd_specials_file_from_json(const bt_json_value *value, bt_gd_specials_file *out, char *err,
                                  size_t err_len);

void bt_gd_manifest_free(bt_gd_manifest *manifest);
bool bt_gd_manifest_eq(const bt_gd_manifest *a, const bt_gd_manifest *b);
bt_json_value *bt_gd_manifest_to_json(const bt_gd_manifest *manifest);
int bt_gd_manifest_from_json(const bt_json_value *value, bt_gd_manifest *out, char *err,
                             size_t err_len);

#endif
