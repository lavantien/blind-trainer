#include "scheme/lib.h"

#include "common/bt_trace.h"

const char *bt_scheme_crate_id(void) {
    BT_TRACE_ENTER("scheme.crate_id");
    BT_TRACE_EXIT("scheme.crate_id");
    return "bt-scheme";
}
