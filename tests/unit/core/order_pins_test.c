#include "common.h"

static uint32_t order_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(text, &alg, err, sizeof err)) {
        BT_ABORT("parse failed for %s: %s", text, err);
    }
    bt_cube cube = bt_cube_solved();
    uint32_t n = 0;
    for (;;) {
        bt_cube_apply_alg(&cube, &alg);
        n += 1;
        if (n >= 2000) {
            BT_ABORT("order runaway for %s", text);
        }
        if (bt_cube_is_solved(&cube)) {
            bt_alg_free(&alg);
            return n;
        }
    }
}

BT_TEST(order_pins, sexy_move_has_order_6) {
    BT_CHECK_EQ(order_of("R U R' U'"), 6U);
}

BT_TEST(order_pins, r_u_has_order_105) {
    BT_CHECK_EQ(order_of("R U"), 105U);
}

BT_TEST(order_pins, slices_have_order_4) {
    const char *tokens[] = {"M", "E", "S"};
    for (size_t i = 0; i < 3; i++) {
        BT_CHECK_EQ(order_of(tokens[i]), 4U);
    }
}

BT_TEST(order_pins, rotations_have_order_4) {
    const char *tokens[] = {"x", "y", "z"};
    for (size_t i = 0; i < 3; i++) {
        BT_CHECK_EQ(order_of(tokens[i]), 4U);
    }
}

BT_TEST(order_pins, centers_stay_fixed_under_all_vendor_algs) {
    static tc_vendor_rows rows;
    tc_load_vendor_rows(&rows);
    BT_CHECK_EQ(rows.count, (size_t)1077);
    for (size_t i = 0; i < rows.count; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(rows.text[i], &alg, err, sizeof err));
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        if (!bt_cube_centers_fixed(&cube)) {
            BT_ABORT("%s alg %s", rows.rel[i], rows.text[i]);
        }
        bt_alg_free(&alg);
    }
}

BT_TEST(order_pins, inverse_law) {
    bt_rng rng;
    bt_rng_seed(&rng, 20261013);
    char text[256];
    for (size_t case_index = 0; case_index < 2000; case_index++) {
        tc_alg_text(&rng, text, sizeof text);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(text, &alg, err, sizeof err));
        bt_alg inverse = bt_alg_inverse(&alg);
        bt_cube cube = bt_cube_solved();
        bt_cube_apply_alg(&cube, &alg);
        bt_cube_apply_alg(&cube, &inverse);
        if (!bt_cube_is_solved(&cube)) {
            BT_ABORT("inverse law failed for %s", text);
        }
        bt_alg_free(&alg);
        bt_alg_free(&inverse);
    }
}
