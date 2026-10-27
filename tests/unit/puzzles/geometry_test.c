#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "puzzles/geometry.h"

#include <string.h>

BT_TEST(geometry, parse_error_message_names_the_token) {
    bt_pz_error err;
    bt_pz_error_set(&err, BT_PZ_ERR_PARSE, "wut", strlen("wut"));
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    bt_pz_error_message(&err, &sb);
    BT_CHECK_EQ(sb.data, "unknown token: wut");
    bt_strbuf_free(&sb);
}

BT_TEST(geometry, apply_error_message_names_the_move) {
    bt_pz_error err;
    bt_pz_error_set(&err, BT_PZ_ERR_APPLY, "/", strlen("/"));
    bt_strbuf sb;
    bt_strbuf_init(&sb, 32);
    bt_pz_error_message(&err, &sb);
    BT_CHECK_EQ(sb.data, "illegal move: /");
    bt_strbuf_free(&sb);
}

BT_TEST(geometry, error_set_tolerates_a_null_sink) {
    bt_pz_error_set(NULL, BT_PZ_ERR_PARSE, "x", strlen("x"));
    BT_CHECK(true);
}

BT_TEST(geometry, error_set_truncates_only_beyond_the_token_cap) {
    bt_pz_error err;
    char long_token[BT_PZ_TOKEN_MAX + 8];
    memset(long_token, 'q', sizeof long_token - 1);
    long_token[sizeof long_token - 1] = '\0';
    bt_pz_error_set(&err, BT_PZ_ERR_PARSE, long_token, strlen(long_token));
    BT_CHECK_EQ(strlen(err.token), (size_t)(BT_PZ_TOKEN_MAX - 1));
    bt_pz_error_set(&err, BT_PZ_ERR_PARSE, "z", strlen("z"));
    BT_CHECK_EQ(err.kind, BT_PZ_ERR_PARSE);
    BT_CHECK_EQ(err.token[0], 'z');
    BT_CHECK_EQ(err.token[1], '\0');
}

static void fill_grid(bt_pz_grid *grid, size_t face_count, size_t row_count, size_t cols) {
    grid->face_count = face_count;
    for (size_t f = 0; f < face_count; f++) {
        grid->faces[f].row_count = row_count;
        for (size_t r = 0; r < row_count; r++) {
            grid->faces[f].rows[r].cols = cols;
            for (size_t c = 0; c < cols; c++) {
                grid->faces[f].rows[r].cells[c] = (uint8_t)(f * 100 + r * 10 + c);
            }
        }
    }
}

BT_TEST(geometry, grid_eq_compares_faces_rows_and_cells) {
    bt_pz_grid a;
    bt_pz_grid b;
    fill_grid(&a, 2, 2, 3);
    fill_grid(&b, 2, 2, 3);
    BT_CHECK(bt_pz_grid_eq(&a, &b));
    b.faces[1].rows[0].cells[2] = 42;
    BT_CHECK(!bt_pz_grid_eq(&a, &b));
    fill_grid(&b, 2, 2, 3);
    b.faces[0].rows[1].cols = 2;
    BT_CHECK(!bt_pz_grid_eq(&a, &b));
    fill_grid(&b, 2, 2, 3);
    b.faces[0].row_count = 1;
    BT_CHECK(!bt_pz_grid_eq(&a, &b));
    fill_grid(&b, 2, 2, 3);
    b.face_count = 1;
    BT_CHECK(!bt_pz_grid_eq(&a, &b));
    fill_grid(&b, 2, 2, 3);
    BT_CHECK(bt_pz_grid_eq(&a, &b));
    BT_CHECK(bt_pz_grid_eq(&a, &a));
}
