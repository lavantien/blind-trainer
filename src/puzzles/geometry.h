#ifndef BT_PUZZLES_GEOMETRY_H
#define BT_PUZZLES_GEOMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"

#define BT_PZ_TOKEN_MAX 128
#define BT_PZ_MAX_FACES 12
#define BT_PZ_MAX_ROWS 12
#define BT_PZ_MAX_COLS 12
#define BT_PZ_MAX_MOVE_BYTES 32

typedef enum {
    BT_PZ_ERR_PARSE = 0,
    BT_PZ_ERR_APPLY = 1,
} bt_pz_error_kind;

typedef struct {
    bt_pz_error_kind kind;
    char token[BT_PZ_TOKEN_MAX];
} bt_pz_error;

typedef struct {
    uint8_t cells[BT_PZ_MAX_COLS];
    size_t cols;
} bt_pz_row;

typedef struct {
    bt_pz_row rows[BT_PZ_MAX_ROWS];
    size_t row_count;
} bt_pz_face;

typedef struct {
    bt_pz_face faces[BT_PZ_MAX_FACES];
    size_t face_count;
} bt_pz_grid;

typedef struct {
    uint8_t *items;
    size_t len;
    size_t cap;
} bt_pz_alg;

typedef struct bt_pz_engine bt_pz_engine;

struct bt_pz_engine {
    const char *name;
    const void *self;
    size_t move_size;
    size_t state_size;
    bool (*parse_move)(const bt_pz_engine *eng, const char *token, size_t len, void *out_move,
                       bt_pz_error *err);
    bool (*parse_alg)(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                      bt_pz_error *err);
    void (*format_alg)(const bt_pz_engine *eng, const void *alg, bt_strbuf *out);
    void (*invert_alg)(const bt_pz_engine *eng, const void *alg, void *out);
    bool (*apply_alg)(const bt_pz_engine *eng, void *state, const void *alg, bt_pz_error *err);
    void (*solved_state)(const bt_pz_engine *eng, void *out_state);
    bool (*is_solved)(const bt_pz_engine *eng, const void *state);
    void (*facelets)(const bt_pz_engine *eng, const void *state, bt_pz_grid *out);
};

void bt_pz_error_set(bt_pz_error *err, bt_pz_error_kind kind, const char *token, size_t len);
void bt_pz_error_message(const bt_pz_error *err, bt_strbuf *out);
bool bt_pz_grid_eq(const bt_pz_grid *a, const bt_pz_grid *b);

void *bt_pz_alg_create(void);
void bt_pz_alg_destroy(void *alg);
void bt_pz_alg_push(const bt_pz_engine *eng, void *alg, const void *move);
const void *bt_pz_alg_at(const bt_pz_engine *eng, const void *alg, size_t index);
size_t bt_pz_alg_len(const void *alg);
bool bt_pz_alg_is_empty(const void *alg);
bool bt_pz_alg_default_parse(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                             bt_pz_error *err);

#endif
