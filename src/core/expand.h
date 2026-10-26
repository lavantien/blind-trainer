#ifndef BT_CORE_EXPAND_H
#define BT_CORE_EXPAND_H

#include <stdbool.h>

#include "core/alg.h"

#define BT_EXPAND_REPEAT_LIMIT 99U
#define BT_EXPAND_EXPAND_LIMIT 100000U
#define BT_EXPAND_MAX_DEPTH 4U

bool bt_expand(const char *text, bt_alg *out);
void bt_expand_cancel(const bt_alg *in, bt_alg *out);

#endif
