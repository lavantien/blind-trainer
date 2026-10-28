#ifndef VERIFY_LIB_H
#define VERIFY_LIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/paths.h"
#include "data/corpus.h"
#include "data/digest.h"

#define VERIFY_SIMCHECK_HEADER "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote"
#define VERIFY_SIM_POSITION_MAX 128
#define VERIFY_ERR_MAX 256
#define VERIFY_ERR_STRBUF_INIT 64
#define VERIFY_LINE_MAX 512

typedef struct {
    char *position;
    uint32_t qtm;
} verify_sim_row;

typedef struct {
    bt_solver solver;
    bt_sheet_class sheet;
    size_t rowid;
    verify_sim_row row;
} verify_sim_entry;

typedef struct {
    verify_sim_entry *items;
    size_t count;
    size_t cap;
} verify_sim_map;

typedef struct {
    const char *solver;
    const char *sheet;
    size_t rows;
} verify_sheet_pin;

extern const verify_sheet_pin VERIFY_SHEET_EXPECTED_ROWS[BT_DIGEST_FILES];
extern const size_t VERIFY_SHEET_PIN_COUNT;

void verify_sim_map_init(verify_sim_map *map);
void verify_sim_map_free(verify_sim_map *map);
void verify_sim_map_insert(verify_sim_map *map, bt_solver solver, bt_sheet_class sheet,
                           size_t rowid, const char *position, uint32_t qtm);
const verify_sim_row *verify_sim_map_get(const verify_sim_map *map, bt_solver solver,
                                         bt_sheet_class sheet, size_t rowid);
int verify_load_simcheck(const char *root, verify_sim_map *out, char *err, size_t err_len);
const char *verify_crate_id(void);
char *verify_xstrdup(const char *text);
void verify_artifacts_dir_from(const char *raw_value, const char *workspace_root,
                               char out[BT_PATH_MAX]);

#endif
