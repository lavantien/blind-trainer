#ifndef BT_APP_VIEWS_ALGS_MOD_H
#define BT_APP_VIEWS_ALGS_MOD_H

#include "app/config.h"
#include "app/logic.h"
#include "app/states.h"
#include "app/views/state.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/corpus.h"

#include <stddef.h>

#define BT_ALGS_QUERY_MAX 32
#define BT_ALGS_LABEL_MAX 16
#define BT_ALGS_ROWS_MAX 64
#define BT_ALGS_TEXT_MAX 256
#define BT_ALGS_SPANS_MAX 32

typedef struct {
    size_t first;
    size_t count;
} bt_view_list_span;

typedef struct {
    char label[BT_ALGS_TEXT_MAX];
    int selected;
} bt_view_case_row;

typedef enum {
    BT_ALGS_STYLE_FOCAL,
    BT_ALGS_STYLE_MUTED,
    BT_ALGS_STYLE_HEADING,
    BT_ALGS_STYLE_ALG,
    BT_ALGS_STYLE_BODY,
    BT_ALGS_STYLE_SMALL,
    BT_ALGS_STYLE_MICRO,
} bt_algs_style;

bt_view_list_span bt_view_list_window(size_t total, size_t selected, size_t capacity);

#endif
