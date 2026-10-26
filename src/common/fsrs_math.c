#include "common/fsrs_math.h"

#include <fenv.h>
#include <float.h>
#include <math.h>

#include "common/bt_trace.h"

int bt_fsrs_math_init(void) {
    BT_TRACE_ENTER("fsrs_math.init");
    if (fesetround(FE_TONEAREST) != 0) {
        BT_TRACE_FAIL("fsrs_math.init", "fesetround(FE_TONEAREST) failed");
        BT_TRACE_EXIT("fsrs_math.init");
        return -1;
    }
    if (FLT_ROUNDS != 1) {
        BT_TRACE_FAIL("fsrs_math.init", "FLT_ROUNDS is %d, want 1", FLT_ROUNDS);
        BT_TRACE_EXIT("fsrs_math.init");
        return -1;
    }
    BT_TRACE_EXIT("fsrs_math.init");
    return 0;
}

double bt_round_half_even(double value) {
    BT_TRACE_ENTER("fsrs_math.round_half_even");
    BT_TRACE_EXIT("fsrs_math.round_half_even");
    return nearbyint(value);
}
