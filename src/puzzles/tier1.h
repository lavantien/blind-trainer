#ifndef BT_PUZZLES_TIER1_H
#define BT_PUZZLES_TIER1_H

#include "common/strbuf.h"
#include "puzzles/geometry.h"

typedef enum {
    BT_TIER1_OK = 0,
    BT_TIER1_PARSE = 1,
    BT_TIER1_APPLY = 2,
    BT_TIER1_FORWARD = 3,
    BT_TIER1_REVERSE = 4,
} bt_tier1_status;

typedef struct {
    bt_tier1_status status;
    bt_pz_error cause;
} bt_tier1_error;

bt_tier1_status bt_verify_tier1(const bt_pz_engine *eng, const char *text, bt_tier1_error *err);
void bt_tier1_error_message(const bt_tier1_error *err, bt_strbuf *out);

#endif
