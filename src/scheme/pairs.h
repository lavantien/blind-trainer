#ifndef BT_SCHEME_PAIRS_H
#define BT_SCHEME_PAIRS_H

#include "scheme/scheme.h"

#include <stddef.h>
#include <stdint.h>

#define BT_PAIRS_MAX 440

typedef struct {
    bt_orbit orbit;
    uint8_t t1;
    uint8_t t2;
} bt_case;

size_t bt_scheme_enumerate_cases(bt_orbit orbit, bt_case out[BT_PAIRS_MAX]);
void bt_scheme_case_letters(const bt_case *case_id, char out[2]);

#endif
