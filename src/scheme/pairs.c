#include "scheme/pairs.h"

#include "internal.h"

#include <stddef.h>

size_t bt_scheme_enumerate_cases(bt_orbit orbit, bt_case out[BT_PAIRS_MAX]) {
    BT_TRACE_ENTER("scheme.enumerate_cases");
    uint8_t buffer =
        orbit == BT_ORBIT_CORNER ? BT_SCHEME_CORNER_BUFFER_STICKER : BT_SCHEME_EDGE_BUFFER_STICKER;
    size_t count = 0;
    for (uint8_t t1 = bt_scheme_orbit_lo(orbit); t1 < bt_scheme_orbit_hi(orbit); t1++) {
        if (bt_scheme_same_piece(t1, buffer)) {
            continue;
        }
        for (uint8_t t2 = bt_scheme_orbit_lo(orbit); t2 < bt_scheme_orbit_hi(orbit); t2++) {
            if (bt_scheme_same_piece(t2, buffer) || bt_scheme_same_piece(t2, t1)) {
                continue;
            }
            out[count].orbit = orbit;
            out[count].t1 = t1;
            out[count].t2 = t2;
            count++;
        }
    }
    BT_TRACE_EXIT("scheme.enumerate_cases");
    return count;
}

void bt_scheme_case_letters(const bt_case *case_id, char out[2]) {
    BT_TRACE_ENTER("scheme.case_letters");
    out[0] = bt_scheme_letter_of(case_id->t1);
    out[1] = bt_scheme_letter_of(case_id->t2);
    BT_TRACE_EXIT("scheme.case_letters");
}
