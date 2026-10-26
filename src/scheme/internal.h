#ifndef BT_SCHEME_INTERNAL_H
#define BT_SCHEME_INTERNAL_H

#include "common/bt_harness.h"
#include "common/bt_trace.h"

#define BT_SCHEME_ABORT(unit, ...)                                                                 \
    do {                                                                                           \
        BT_TRACE_FAIL(unit, __VA_ARGS__);                                                          \
        bt_harness_fail(__FILE__, __LINE__, __VA_ARGS__);                                          \
        bt_harness_exit_failure();                                                                 \
    } while (0)

#endif
