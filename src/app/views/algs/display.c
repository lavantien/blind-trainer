#include "app/views/algs/display.h"

#include "common/bt_trace.h"

#include <stdint.h>
#include <string.h>

#define BT_NET_PLAN_PAD ((size_t)-1)

static void net_face_of(size_t face, const bt_pz_face *view, bt_net_face *out) {
    BT_TRACE_ENTER("app.net_display_face");
    memset(out, 0, sizeof *out);
    out->face = face;
    out->row_count = view->row_count;
    for (size_t row = 0; row < view->row_count; row++) {
        size_t cells = view->rows[row].cols;
        if (cells > out->width) {
            out->width = cells;
        }
    }
    for (size_t row = 0; row < view->row_count; row++) {
        size_t cells = view->rows[row].cols;
        size_t lead = (out->width - cells) / 2;
        size_t at = 0;
        for (; at < lead; at++) {
            out->rows[row][at].kind = BT_NET_SLOT_PAD;
        }
        for (size_t col = 0; col < cells; col++) {
            out->rows[row][at].kind = BT_NET_SLOT_CELL;
            out->rows[row][at].cell.face = face;
            out->rows[row][at].cell.row = row;
            out->rows[row][at].cell.col = col;
            out->rows[row][at].cell.color = view->rows[row].cells[col];
            at++;
        }
        for (; at < out->width; at++) {
            out->rows[row][at].kind = BT_NET_SLOT_PAD;
        }
        out->row_lengths[row] = out->width;
    }
    BT_TRACE_EXIT("app.net_display_face");
}

static size_t band_plan(size_t face_count, size_t plan[BT_NET_LAYOUT_MAX_BANDS][BT_NET_LAYOUT_MAX_SLOTS],
                        size_t counts[BT_NET_LAYOUT_MAX_BANDS]) {
    BT_TRACE_ENTER("app.net_display_band_plan");
    memset(counts, 0, BT_NET_LAYOUT_MAX_BANDS * sizeof *counts);
    size_t band_count = 0;
    if (face_count == 0) {
        BT_TRACE_EXIT("app.net_display_band_plan");
        return 0;
    }
    if (face_count == 1 || face_count == 2) {
        for (size_t face = 0; face < face_count; face++) {
            plan[face][0] = face;
            counts[face] = 1;
        }
        band_count = face_count;
    } else if (face_count == 3) {
        plan[0][0] = 0;
        plan[0][1] = 1;
        plan[0][2] = 2;
        counts[0] = 3;
        band_count = 1;
    } else if (face_count == 4) {
        plan[0][0] = 0;
        plan[0][1] = 1;
        plan[1][0] = 2;
        plan[1][1] = 3;
        counts[0] = 2;
        counts[1] = 2;
        band_count = 2;
    } else if (face_count == 6) {
        plan[0][0] = BT_NET_PLAN_PAD;
        plan[0][1] = 0;
        counts[0] = 2;
        plan[1][0] = 1;
        plan[1][1] = 2;
        plan[1][2] = 3;
        plan[1][3] = 4;
        counts[1] = 4;
        plan[2][0] = BT_NET_PLAN_PAD;
        plan[2][1] = 5;
        counts[2] = 2;
        band_count = 3;
    } else {
        size_t at = 0;
        while (at < face_count) {
            size_t slot = 0;
            for (; slot < 4 && at < face_count; slot++) {
                plan[band_count][slot] = at;
                at++;
            }
            counts[band_count] = slot;
            band_count++;
        }
        size_t last = band_count - 1;
        size_t pad = (4 - counts[last]) / 2;
        for (size_t shift = counts[last]; shift > 0; shift--) {
            plan[last][shift + pad - 1] = plan[last][shift - 1];
        }
        for (size_t slot = 0; slot < pad; slot++) {
            plan[last][slot] = BT_NET_PLAN_PAD;
        }
        counts[last] += pad;
    }
    BT_TRACE_EXIT("app.net_display_band_plan");
    return band_count;
}

void bt_net_display_layout(const bt_pz_grid *grid, bt_net_layout *out) {
    BT_TRACE_ENTER("app.net_display_layout");
    memset(out, 0, sizeof *out);
    if (grid->face_count == 0 || grid->face_count > BT_PZ_MAX_FACES) {
        BT_TRACE_EXIT("app.net_display_layout");
        return;
    }
    bt_net_face faces[BT_PZ_MAX_FACES];
    for (size_t face = 0; face < grid->face_count; face++) {
        net_face_of(face, &grid->faces[face], &faces[face]);
    }
    size_t spacer = faces[0].width;
    size_t plan[BT_NET_LAYOUT_MAX_BANDS][BT_NET_LAYOUT_MAX_SLOTS];
    size_t counts[BT_NET_LAYOUT_MAX_BANDS];
    out->band_count = band_plan(grid->face_count, plan, counts);
    for (size_t band = 0; band < out->band_count; band++) {
        for (size_t slot = 0; slot < counts[band]; slot++) {
            size_t wanted = plan[band][slot];
            bt_net_band_slot *dst = &out->bands[band][slot];
            if (wanted == BT_NET_PLAN_PAD) {
                dst->kind = BT_NET_BAND_SPACER;
                dst->spacer = spacer;
            } else {
                dst->kind = BT_NET_BAND_FACE;
                dst->face = faces[wanted];
            }
        }
        out->band_slot_count[band] = counts[band];
    }
    BT_TRACE_EXIT("app.net_display_layout");
}

bt_theme_color bt_net_display_cell_color(const bt_net_cell *cell) {
    BT_TRACE_ENTER("app.net_display_cell_color");
    bt_theme_color color = bt_theme_palette_color(cell->color);
    BT_TRACE_EXIT("app.net_display_cell_color");
    return color;
}
