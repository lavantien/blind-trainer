#ifndef BT_DATA_COMMUTATOR_H
#define BT_DATA_COMMUTATOR_H

#include <stdbool.h>
#include <stddef.h>

#include "core/alg.h"

#define BT_COMMUTATOR_MAX_DEPTH 1U

typedef struct {
    bool has_setup;
    bt_alg setup;
    bt_alg a;
    bt_alg b;
    bool b_first;
} bt_expanded;

typedef struct {
    bt_expanded *items;
    size_t count;
} bt_expanded_list;

void bt_expanded_list_init(bt_expanded_list *list);
void bt_expanded_list_free(bt_expanded_list *list);
bt_alg bt_expanded_expansion(const bt_expanded *unit);
bool bt_expanded_eq(const bt_expanded *a, const bt_expanded *b);
bool bt_decompose(const char *text, bt_expanded_list *out);
bool bt_decompose_verified(const char *text, const bt_alg *alg, bt_expanded_list *out);

#endif
