#include "puzzles/geometry.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/space.h"

void bt_pz_error_set(bt_pz_error *err, bt_pz_error_kind kind, const char *token, size_t len) {
    BT_TRACE_ENTER("bt_pz_error_set");
    if (err == NULL) {
        BT_TRACE_EXIT("bt_pz_error_set");
        return;
    }
    err->kind = kind;
    size_t at = 0;
    while (token != NULL && at < len && at + 1 < sizeof err->token) {
        err->token[at] = token[at];
        at++;
    }
    err->token[at] = '\0';
    BT_TRACE_EXIT("bt_pz_error_set");
}

void bt_pz_error_message(const bt_pz_error *err, bt_strbuf *out) {
    BT_TRACE_ENTER("bt_pz_error_message");
    const char *prefix = err->kind == BT_PZ_ERR_APPLY ? "illegal move: " : "unknown token: ";
    bt_strbuf_append_str(out, prefix);
    bt_strbuf_append_str(out, err->token);
    BT_TRACE_EXIT("bt_pz_error_message");
}

bool bt_pz_grid_eq(const bt_pz_grid *a, const bt_pz_grid *b) {
    BT_TRACE_ENTER("bt_pz_grid_eq");
    if (a->face_count != b->face_count) {
        BT_TRACE_EXIT("bt_pz_grid_eq");
        return false;
    }
    for (size_t f = 0; f < a->face_count; f++) {
        if (a->faces[f].row_count != b->faces[f].row_count) {
            BT_TRACE_EXIT("bt_pz_grid_eq");
            return false;
        }
        for (size_t r = 0; r < a->faces[f].row_count; r++) {
            const bt_pz_row *ra = &a->faces[f].rows[r];
            const bt_pz_row *rb = &b->faces[f].rows[r];
            if (ra->cols != rb->cols) {
                BT_TRACE_EXIT("bt_pz_grid_eq");
                return false;
            }
            if (memcmp(ra->cells, rb->cells, ra->cols) != 0) {
                BT_TRACE_EXIT("bt_pz_grid_eq");
                return false;
            }
        }
    }
    BT_TRACE_EXIT("bt_pz_grid_eq");
    return true;
}

static void bt_pz_fatal(const char *unit, const char *what) {
    BT_TRACE_FAIL(unit, "%s failed", what);
    bt_trace_dump();
    abort();
}

void *bt_pz_alg_create(void) {
    BT_TRACE_ENTER("bt_pz_alg_create");
    bt_pz_alg *alg = malloc(sizeof *alg);
    if (alg == NULL) {
        bt_pz_fatal("bt_pz_alg_create", "malloc");
    }
    alg->items = NULL;
    alg->len = 0;
    alg->cap = 0;
    BT_TRACE_EXIT("bt_pz_alg_create");
    return alg;
}

void bt_pz_alg_destroy(void *alg) {
    BT_TRACE_ENTER("bt_pz_alg_destroy");
    bt_pz_alg *vec = alg;
    if (vec != NULL) {
        free(vec->items);
    }
    free(vec);
    BT_TRACE_EXIT("bt_pz_alg_destroy");
}

void bt_pz_alg_push(const bt_pz_engine *eng, void *alg, const void *move) {
    BT_TRACE_ENTER("bt_pz_alg_push");
    bt_pz_alg *vec = alg;
    if (vec->len == vec->cap) {
        size_t want = vec->cap == 0 ? 8 : vec->cap * 2;
        uint8_t *grown = realloc(vec->items, want * eng->move_size);
        if (grown == NULL) {
            bt_pz_fatal("bt_pz_alg_push", "realloc");
        }
        vec->items = grown;
        vec->cap = want;
    }
    memcpy(vec->items + vec->len * eng->move_size, move, eng->move_size);
    vec->len++;
    BT_TRACE_EXIT("bt_pz_alg_push");
}

const void *bt_pz_alg_at(const bt_pz_engine *eng, const void *alg, size_t index) {
    BT_TRACE_ENTER("bt_pz_alg_at");
    const bt_pz_alg *vec = alg;
    if (index >= vec->len) {
        bt_pz_fatal("bt_pz_alg_at", "index out of range");
    }
    const void *move = vec->items + index * eng->move_size;
    BT_TRACE_EXIT("bt_pz_alg_at");
    return move;
}

size_t bt_pz_alg_len(const void *alg) {
    BT_TRACE_ENTER("bt_pz_alg_len");
    const bt_pz_alg *vec = alg;
    size_t len = vec->len;
    BT_TRACE_EXIT("bt_pz_alg_len");
    return len;
}

bool bt_pz_alg_is_empty(const void *alg) {
    BT_TRACE_ENTER("bt_pz_alg_is_empty");
    bool empty = bt_pz_alg_len(alg) == 0;
    BT_TRACE_EXIT("bt_pz_alg_is_empty");
    return empty;
}

bool bt_pz_alg_default_parse(const bt_pz_engine *eng, const char *text, size_t len, void *alg,
                             bt_pz_error *err) {
    BT_TRACE_ENTER("bt_pz_alg_default_parse");
    if (eng->move_size > BT_PZ_MAX_MOVE_BYTES) {
        bt_pz_fatal("bt_pz_alg_default_parse", "move size over the scratch cap");
    }
    bt_pz_alg staged = {0};
    _Alignas(max_align_t) uint8_t scratch[BT_PZ_MAX_MOVE_BYTES];
    size_t at = 0;
    while (at < len) {
        size_t space = bt_space_len(text, at, len);
        while (space > 0) {
            at += space;
            space = bt_space_len(text, at, len);
        }
        if (at >= len) {
            break;
        }
        size_t start = at;
        while (at < len && bt_space_len(text, at, len) == 0) {
            at++;
        }
        if (!eng->parse_move(eng, text + start, at - start, scratch, err)) {
            BT_TRACE_FAIL("bt_pz_alg_default_parse", "token rejected at %zu", start);
            free(staged.items);
            BT_TRACE_EXIT("bt_pz_alg_default_parse");
            return false;
        }
        bt_pz_alg_push(eng, &staged, scratch);
    }
    bt_pz_alg *vec = alg;
    free(vec->items);
    *vec = staged;
    BT_TRACE_EXIT("bt_pz_alg_default_parse");
    return true;
}
