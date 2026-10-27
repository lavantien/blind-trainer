#include "app/views/images.h"

#include "app/views/images_table.h"

#include "app/views/state.h"
#include "common/bt_trace.h"

#include <string.h>

const char *bt_view_images_for(const char letters[2]) {
    BT_TRACE_ENTER("app.view_images_for");
    char key[3] = {letters[0], letters[1], '\0'};
    size_t low = 0;
    size_t high = BT_VIEW_IMAGES_COUNT;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int cmp = strcmp(BT_VIEW_IMAGES[mid].key, key);
        if (cmp == 0) {
            BT_TRACE_EXIT("app.view_images_for");
            return BT_VIEW_IMAGES[mid].word;
        }
        if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    BT_TRACE_EXIT("app.view_images_for");
    return NULL;
}

void bt_view_images_install(void) {
    BT_TRACE_ENTER("app.view_images_install");
    bt_views_set_image_lookup(bt_view_images_for);
    BT_TRACE_EXIT("app.view_images_install");
}
