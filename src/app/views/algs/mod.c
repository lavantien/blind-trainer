#include "app/views/algs/mod.h"

#include "common/bt_trace.h"

bt_view_list_span bt_view_list_window(size_t total, size_t selected, size_t capacity) {
    BT_TRACE_ENTER("app.algs_list_window");
    bt_view_list_span window = {0, 0};
    size_t clamped = capacity;
    if (clamped > total) {
        clamped = total;
    }
    if (clamped < 1) {
        clamped = 1;
    }
    size_t start = selected >= clamped / 2 ? selected - clamped / 2 : 0;
    size_t last_start = total >= clamped ? total - clamped : 0;
    if (start > last_start) {
        start = last_start;
    }
    window.first = start;
    window.count = clamped;
    BT_TRACE_EXIT("app.algs_list_window");
    return window;
}
