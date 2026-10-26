#ifndef BT_CORE_ALG_H
#define BT_CORE_ALG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/strbuf.h"
#include "core/moves.h"

typedef struct {
    bt_move *items;
    size_t len;
    size_t cap;
} bt_alg;

void bt_alg_init(bt_alg *alg);
void bt_alg_free(bt_alg *alg);
bool bt_alg_push(bt_alg *alg, bt_move mv);
size_t bt_alg_len(const bt_alg *alg);
bool bt_alg_is_empty(const bt_alg *alg);

bool bt_alg_parse(const char *text, bt_alg *out, char *err_token, size_t err_token_size);
bool bt_alg_try_parse(const char *text, bt_alg *out);
void bt_alg_format(const bt_alg *alg, bt_strbuf *out);
bt_alg bt_alg_inverse(const bt_alg *alg);
uint32_t bt_alg_qtm(const bt_alg *alg);
uint32_t bt_alg_htm(const bt_alg *alg);
bool bt_alg_eq(const bt_alg *a, const bt_alg *b);

bool bt_alg_parse_token(const char *token, bt_move *out);

#endif
