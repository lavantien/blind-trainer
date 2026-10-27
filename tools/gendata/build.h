#ifndef GD_BUILD_H
#define GD_BUILD_H

#include <stdbool.h>
#include <stddef.h>

#include "data/corpus.h"
#include "data/digest.h"
#include "gendata/model.h"

typedef struct {
    bt_gd_cases_file cases;
    bt_gd_specials_file specials;
    size_t rows_in_pair_space;
    size_t rows_other_buffer;
    char **offenders;
    size_t offender_count;
} bt_gd_built;

void bt_gd_built_free(bt_gd_built *built);
void bt_gd_build(const bt_corpus *corpus, bt_gd_built *out);
bool bt_gd_regen_needed(const bt_gd_manifest *existing, bool has_existing,
                        const char *synth_params_sha256, const char *corpus_sha256);
void bt_gd_manifest_make(const bt_gd_built *built, const bt_corpus_digest *digest,
                         const char *cases_sha256, const char *specials_sha256,
                         const char *synth_params_sha256, const char *generated_at,
                         bt_gd_manifest *out);

#endif
