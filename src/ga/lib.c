#include "ga/lib.h"

#include "common/bt_trace.h"

const char *bt_ga_crate_id(void) {
    BT_TRACE_ENTER("ga.crate_id");
    BT_TRACE_EXIT("ga.crate_id");
    return "bt-ga";
}
