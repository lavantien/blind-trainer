#include "app/theme.h"
#include "app/views/algs/display.h"
#include "puzzles/geometry.h"

#include "common.h"

static void square_grid(size_t n, unsigned char (*color_of)(size_t, size_t, size_t),
                        bt_pz_grid *out) {
    out->face_count = 6;
    for (size_t face = 0; face < 6; face++) {
        out->faces[face].row_count = n;
        for (size_t row = 0; row < n; row++) {
            out->faces[face].rows[row].cols = n;
            for (size_t col = 0; col < n; col++) {
                out->faces[face].rows[row].cells[col] = color_of(face, row, col);
            }
        }
    }
}

static void ragged_grid(size_t face_count, const size_t *row_lengths, bt_pz_grid *out) {
    size_t lengths[BT_PZ_MAX_ROWS];
    size_t row_total = 0;
    while (row_lengths[row_total] != 0) {
        lengths[row_total] = row_lengths[row_total];
        row_total++;
    }
    out->face_count = face_count;
    for (size_t face = 0; face < face_count; face++) {
        out->faces[face].row_count = row_total;
        for (size_t row = 0; row < row_total; row++) {
            out->faces[face].rows[row].cols = lengths[row];
            for (size_t col = 0; col < lengths[row]; col++) {
                out->faces[face].rows[row].cells[col] = (unsigned char)(face % 12);
            }
        }
    }
}

static unsigned char square_mod12(size_t face, size_t row, size_t col) {
    return (unsigned char)((face * 9 + row * 3 + col) % 12);
}

static unsigned char stride_mod12(size_t face, size_t row, size_t col) {
    return (unsigned char)((face + row + col) % 12);
}

static unsigned char face_index(size_t face, size_t row, size_t col) {
    (void)row;
    (void)col;
    return (unsigned char)face;
}

static size_t band_face_ids(const bt_netdisp_layout *layout, size_t band, size_t *out_faces) {
    size_t count = 0;
    for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
        if (layout->bands[band][slot].kind == BT_NETDISP_BAND_FACE) {
            out_faces[count++] = layout->bands[band][slot].face.face;
        }
    }
    return count;
}

static size_t collect_cells(const bt_netdisp_layout *layout, bt_netdisp_cell *out, size_t cap) {
    size_t count = 0;
    for (size_t band = 0; band < layout->band_count; band++) {
        for (size_t slot = 0; slot < layout->band_slot_count[band]; slot++) {
            if (layout->bands[band][slot].kind != BT_NETDISP_BAND_FACE) {
                continue;
            }
            const bt_netdisp_face *face = &layout->bands[band][slot].face;
            for (size_t row = 0; row < face->row_count; row++) {
                for (size_t at = 0; at < face->row_lengths[row]; at++) {
                    if (face->rows[row][at].kind != BT_NETDISP_SLOT_CELL) {
                        continue;
                    }
                    if (count < cap) {
                        out[count] = face->rows[row][at].cell;
                    }
                    count++;
                }
            }
        }
    }
    return count;
}

static size_t face_cell_count(const bt_netdisp_face *face) {
    size_t count = 0;
    for (size_t row = 0; row < face->row_count; row++) {
        for (size_t at = 0; at < face->row_lengths[row]; at++) {
            count += face->rows[row][at].kind == BT_NETDISP_SLOT_CELL;
        }
    }
    return count;
}

static int slot_is_pad_with_face(const bt_netdisp_band_slot *slot, size_t width,
                                 size_t face_index_wanted) {
    if (slot->kind == BT_NETDISP_BAND_SPACER && slot->spacer == width) {
        return 1;
    }
    return slot->kind == BT_NETDISP_BAND_FACE && slot->face.face == face_index_wanted;
}

BT_TEST(view_display, solved_cube_net_follows_the_app_cross) {
    bt_pz_grid grid;
    square_grid(3, face_index, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)3);
    size_t faces[4];
    BT_CHECK_EQ(band_face_ids(&layout, 0, faces), (size_t)1);
    BT_CHECK_EQ(faces[0], (size_t)0);
    BT_CHECK_EQ(band_face_ids(&layout, 1, faces), (size_t)4);
    BT_CHECK_EQ(faces[0], (size_t)1);
    BT_CHECK_EQ(faces[1], (size_t)2);
    BT_CHECK_EQ(faces[2], (size_t)3);
    BT_CHECK_EQ(faces[3], (size_t)4);
    BT_CHECK_EQ(band_face_ids(&layout, 2, faces), (size_t)1);
    BT_CHECK_EQ(faces[0], (size_t)5);
    BT_CHECK(layout.bands[0][0].kind == BT_NETDISP_BAND_SPACER && layout.bands[0][0].spacer == 3);
    BT_CHECK(layout.bands[2][0].kind == BT_NETDISP_BAND_SPACER && layout.bands[2][0].spacer == 3);
    BT_CHECK(layout.bands[0][1].kind == BT_NETDISP_BAND_FACE);
    const bt_netdisp_face *top = &layout.bands[0][1].face;
    BT_CHECK_EQ(top->width, (size_t)3);
    BT_CHECK_EQ(top->row_count, (size_t)3);
    for (size_t row = 0; row < 3; row++) {
        BT_CHECK_EQ(top->row_lengths[row], (size_t)3);
        for (size_t at = 0; at < 3; at++) {
            BT_CHECK(top->rows[row][at].kind == BT_NETDISP_SLOT_CELL);
            BT_CHECK_EQ(top->rows[row][at].cell.face, (size_t)0);
            BT_CHECK_EQ(top->rows[row][at].cell.row, row);
            BT_CHECK_EQ(top->rows[row][at].cell.col, at);
            BT_CHECK_EQ(top->rows[row][at].cell.color, (unsigned char)0);
        }
    }
    const bt_netdisp_face *bottom = &layout.bands[2][1].face;
    for (size_t row = 0; row < bottom->row_count; row++) {
        for (size_t at = 0; at < bottom->row_lengths[row]; at++) {
            BT_CHECK(bottom->rows[row][at].kind == BT_NETDISP_SLOT_CELL);
            BT_CHECK_EQ(bottom->rows[row][at].cell.color, (unsigned char)5);
        }
    }
}

BT_TEST(view_display, every_facelet_maps_exactly_once) {
    bt_pz_grid grid;
    square_grid(3, square_mod12, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    bt_netdisp_cell cells[128];
    BT_CHECK_EQ(collect_cells(&layout, cells, 128), (size_t)54);
    int seen[6][12][12] = {0};
    for (size_t i = 0; i < 54; i++) {
        BT_CHECK_EQ(cells[i].color,
                    (unsigned char)((cells[i].face * 9 + cells[i].row * 3 + cells[i].col) % 12));
        BT_CHECK(!seen[cells[i].face][cells[i].row][cells[i].col]);
        seen[cells[i].face][cells[i].row][cells[i].col] = 1;
    }
    size_t unique = 0;
    for (size_t face = 0; face < 6; face++) {
        for (size_t row = 0; row < 12; row++) {
            for (size_t col = 0; col < 12; col++) {
                unique += seen[face][row][col] != 0;
            }
        }
    }
    BT_CHECK_EQ(unique, (size_t)54);
}

BT_TEST(view_display, seven_by_seven_keeps_face_and_stride) {
    bt_pz_grid grid;
    square_grid(7, stride_mod12, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)3);
    bt_netdisp_cell cells[512];
    BT_CHECK_EQ(collect_cells(&layout, cells, 512), (size_t)(6 * 49));
    for (size_t band = 0; band < layout.band_count; band++) {
        for (size_t slot = 0; slot < layout.band_slot_count[band]; slot++) {
            if (layout.bands[band][slot].kind != BT_NETDISP_BAND_FACE) {
                continue;
            }
            const bt_netdisp_face *face = &layout.bands[band][slot].face;
            BT_CHECK_EQ(face->width, (size_t)7);
            BT_CHECK_EQ(face->row_count, (size_t)7);
            for (size_t row = 0; row < 7; row++) {
                BT_CHECK_EQ(face->row_lengths[row], (size_t)7);
                for (size_t at = 0; at < 7; at++) {
                    BT_CHECK(face->rows[row][at].kind == BT_NETDISP_SLOT_CELL);
                    BT_CHECK_EQ(face->rows[row][at].cell.color,
                                (unsigned char)((face->face + row + at) % 12));
                }
            }
        }
    }
}

BT_TEST(view_display, triangle_faces_center_ragged_rows) {
    bt_pz_grid grid;
    ragged_grid(4, (const size_t[]){1, 3, 5, 0}, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)2);
    size_t faces[4];
    BT_CHECK_EQ(band_face_ids(&layout, 0, faces), (size_t)2);
    BT_CHECK_EQ(faces[0], (size_t)0);
    BT_CHECK_EQ(faces[1], (size_t)1);
    BT_CHECK_EQ(band_face_ids(&layout, 1, faces), (size_t)2);
    BT_CHECK_EQ(faces[0], (size_t)2);
    BT_CHECK_EQ(faces[1], (size_t)3);
    BT_CHECK(layout.bands[0][0].kind == BT_NETDISP_BAND_FACE);
    const bt_netdisp_face *face = &layout.bands[0][0].face;
    BT_CHECK_EQ(face->width, (size_t)5);
    BT_CHECK_EQ(face->row_count, (size_t)3);
    BT_CHECK_EQ(face->row_lengths[0], (size_t)5);
    BT_CHECK(face->rows[0][0].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK(face->rows[0][1].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK(face->rows[0][2].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK_EQ(face->rows[0][2].cell.col, (size_t)0);
    BT_CHECK(face->rows[0][3].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK(face->rows[0][4].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK_EQ(face->row_lengths[1], (size_t)5);
    BT_CHECK(face->rows[1][0].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK(face->rows[1][1].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[1][2].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[1][3].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[1][4].kind == BT_NETDISP_SLOT_PAD);
    for (size_t at = 0; at < 5; at++) {
        BT_CHECK(face->rows[2][at].kind == BT_NETDISP_SLOT_CELL);
    }
    bt_netdisp_cell cells[128];
    BT_CHECK_EQ(collect_cells(&layout, cells, 128), (size_t)(4 * 9));
}

BT_TEST(view_display, pentagon_faces_center_ragged_rows) {
    bt_pz_grid grid;
    ragged_grid(12, (const size_t[]){1, 2, 3, 3, 2, 0}, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)3);
    size_t faces[4];
    BT_CHECK_EQ(band_face_ids(&layout, 0, faces), (size_t)4);
    BT_CHECK_EQ(faces[0], (size_t)0);
    BT_CHECK_EQ(faces[3], (size_t)3);
    BT_CHECK_EQ(band_face_ids(&layout, 1, faces), (size_t)4);
    BT_CHECK_EQ(faces[2], (size_t)6);
    BT_CHECK_EQ(band_face_ids(&layout, 2, faces), (size_t)4);
    BT_CHECK_EQ(faces[3], (size_t)11);
    BT_CHECK(layout.bands[0][0].kind == BT_NETDISP_BAND_FACE);
    const bt_netdisp_face *face = &layout.bands[0][0].face;
    BT_CHECK_EQ(face->width, (size_t)3);
    BT_CHECK_EQ(face->row_count, (size_t)5);
    BT_CHECK_EQ(face->row_lengths[0], (size_t)3);
    BT_CHECK(face->rows[0][0].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK(face->rows[0][1].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[0][2].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK_EQ(face->row_lengths[1], (size_t)3);
    BT_CHECK(face->rows[1][0].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[1][1].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[1][2].kind == BT_NETDISP_SLOT_PAD);
    BT_CHECK_EQ(face->row_lengths[4], (size_t)3);
    BT_CHECK(face->rows[4][0].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[4][1].kind == BT_NETDISP_SLOT_CELL);
    BT_CHECK(face->rows[4][2].kind == BT_NETDISP_SLOT_PAD);
    bt_netdisp_cell cells[256];
    BT_CHECK_EQ(collect_cells(&layout, cells, 256), (size_t)(12 * 11));
}

BT_TEST(view_display, two_layers_stack_vertically) {
    bt_pz_grid grid;
    ragged_grid(2, (const size_t[]){12, 0}, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)2);
    for (size_t band = 0; band < 2; band++) {
        BT_CHECK_EQ(layout.band_slot_count[band], (size_t)1);
        BT_CHECK(layout.bands[band][0].kind == BT_NETDISP_BAND_FACE);
        const bt_netdisp_face *face = &layout.bands[band][0].face;
        BT_CHECK_EQ(face->face, band);
        BT_CHECK_EQ(face->width, (size_t)12);
        BT_CHECK_EQ(face_cell_count(face), (size_t)12);
    }
    bt_netdisp_cell cells[64];
    BT_CHECK_EQ(collect_cells(&layout, cells, 64), (size_t)24);
}

BT_TEST(view_display, short_odd_band_centers_with_a_leading_spacer) {
    bt_pz_grid grid;
    ragged_grid(5, (const size_t[]){3, 3, 3, 0}, &grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)2);
    size_t faces[4];
    BT_CHECK_EQ(band_face_ids(&layout, 0, faces), (size_t)4);
    BT_CHECK_EQ(layout.band_slot_count[1], (size_t)2);
    BT_CHECK(slot_is_pad_with_face(&layout.bands[1][0], 3, 0));
    BT_CHECK(layout.bands[1][1].kind == BT_NETDISP_BAND_FACE);
    BT_CHECK_EQ(layout.bands[1][1].face.face, (size_t)4);
    BT_CHECK_EQ(face_cell_count(&layout.bands[1][1].face), (size_t)9);
}

BT_TEST(view_display, empty_grid_renders_nothing) {
    bt_pz_grid grid;
    memset(&grid, 0, sizeof grid);
    bt_netdisp_layout layout;
    bt_netdisp_layout_of(&grid, &layout);
    BT_CHECK_EQ(layout.band_count, (size_t)0);
}

static int colors_equal(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

BT_TEST(view_display, cell_color_reads_the_palette) {
    bt_netdisp_cell cell = {2, 0, 0, 7};
    bt_theme_color wanted = bt_theme_palette_color(7);
    bt_theme_color got = bt_netdisp_cell_color(&cell);
    BT_CHECK(colors_equal(wanted, got));
    bt_netdisp_cell wrapped = cell;
    wrapped.color = 12;
    BT_CHECK(colors_equal(bt_theme_palette_color(0), bt_netdisp_cell_color(&wrapped)));
    BT_CHECK(!colors_equal(bt_theme_palette_color(6), bt_netdisp_cell_color(&cell)));
}
