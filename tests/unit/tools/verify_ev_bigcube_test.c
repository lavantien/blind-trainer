#include "verify_test_util.h"

#include <string.h>

#include "data/seedpack/mod.h"
#include "verify/ev_bigcube.h"

static void grid_after(const char *alg, size_t n, bt_pz_grid *out) {
    bt_pz_nxn engine;
    BT_CHECK(bt_pz_nxn_init(&engine, n));
    bt_pz_error err;
    void *alg_ptr = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    BT_CHECK(eng.parse_alg(&eng, alg, strlen(alg), alg_ptr, &err));
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    BT_CHECK(bt_pz_nxn_apply_alg(&engine, &state, alg_ptr, &err));
    bt_pz_alg_destroy(alg_ptr);
    bt_pz_nxn_facelets(&engine, &state, out);
}

static bool err_contains(const char *err, const char *needle) {
    return strstr(err, needle) != NULL;
}

BT_TEST(verify_ev_bigcube, sizes_render_from_all_nspec_shapes) {
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    bt_nspec fixed = {BT_NSPEC_FIXED, 4, 0, {0}};
    BT_CHECK_EQ(verify_bigcube_sizes_of(&fixed, sizes, VERIFY_BIGCUBE_MAX_SIZES), 1);
    BT_CHECK_EQ(sizes[0], 4);
    bt_nspec multi = {BT_NSPEC_SIZES, 0, 3, {5, 6, 7}};
    BT_CHECK_EQ(verify_bigcube_sizes_of(&multi, sizes, VERIFY_BIGCUBE_MAX_SIZES), 3);
    BT_CHECK_EQ(sizes[0], 5);
    BT_CHECK_EQ(sizes[1], 6);
    BT_CHECK_EQ(sizes[2], 7);
    bt_nspec unset = {BT_NSPEC_UNSET, 0, 0, {0}};
    BT_CHECK_EQ(verify_bigcube_sizes_of(&unset, sizes, VERIFY_BIGCUBE_MAX_SIZES), 0);
}

BT_TEST(verify_ev_bigcube, tagged_sizes_read_the_trailing_digit) {
    size_t size = 0;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_tagged_size("oll4", &size, err, sizeof err), 0);
    BT_CHECK_EQ(size, 4);
    BT_CHECK_EQ(verify_bigcube_tagged_size("oll4flip", &size, err, sizeof err), 0);
    BT_CHECK_EQ(size, 4);
    BT_CHECK_EQ(verify_bigcube_tagged_size("wing5", &size, err, sizeof err), 0);
    BT_CHECK_EQ(size, 5);
    BT_CHECK(verify_bigcube_tagged_size("parity", &size, err, sizeof err) != 0);
}

BT_TEST(verify_ev_bigcube, the_solved_grid_audits_clean_with_no_disturbed_lines) {
    bt_pz_nxn engine;
    BT_CHECK(bt_pz_nxn_init(&engine, 5));
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&engine, &solved);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_reduction_state(&engine, &solved, err, sizeof err), 0);
}

BT_TEST(verify_ev_bigcube, a_single_wide_turn_breaks_center_uniformity) {
    bt_pz_nxn engine;
    BT_CHECK(bt_pz_nxn_init(&engine, 4));
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    BT_CHECK(eng.parse_alg(&eng, "Rw", 2, alg, &pz_err));
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    BT_CHECK(bt_pz_nxn_apply_alg(&engine, &state, alg, &pz_err));
    bt_pz_alg_destroy(alg);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_reduction_state(&engine, &state, err, sizeof err), -1);
    BT_CHECK(err_contains(err, "center block"));
}

BT_TEST(verify_ev_bigcube, an_inner_slice_turn_only_disturbs_wing_lines) {
    bt_pz_nxn engine;
    BT_CHECK(bt_pz_nxn_init(&engine, 5));
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    BT_CHECK(eng.parse_alg(&eng, "3Rw'", 4, alg, &pz_err));
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    BT_CHECK(bt_pz_nxn_apply_alg(&engine, &state, alg, &pz_err));
    bt_pz_alg_destroy(alg);
    char err[VERIFY_ERR_MAX];
    int lines = verify_bigcube_reduction_state(&engine, &state, err, sizeof err);
    BT_CHECK(lines == -1 || lines <= 2);
}

BT_TEST(verify_ev_bigcube, a_plain_corner_turn_breaks_corners) {
    bt_pz_grid grid;
    grid_after("R", 4, &grid);
    verify_bigcube_face_audit audit;
    verify_bigcube_audit_face(&grid, 0, 4, &audit);
    BT_CHECK(!audit.corners_solved);
}

BT_TEST(verify_ev_bigcube, yau_rows_verify_at_their_tagged_size_only) {
    bt_pack_entry entry = {0};
    snprintf(entry.file, sizeof entry.file, "%s", "bigcube_parity.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "bigcube");
    snprintf(entry.method, sizeof entry.method, "%s", "yau");
    snprintf(entry.stage, sizeof entry.stage, "%s", "parity");
    entry.n.kind = BT_NSPEC_SIZES;
    entry.n.size_count = 4;
    entry.n.sizes[0] = 4;
    entry.n.sizes[1] = 5;
    entry.n.sizes[2] = 6;
    entry.n.sizes[3] = 7;
    entry.expected_rows = 5;
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(
        verify_bigcube_row_sizes(&entry, "oll4", sizes, VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err),
        1);
    BT_CHECK_EQ(sizes[0], 4);
    BT_CHECK_EQ(
        verify_bigcube_row_sizes(&entry, "wing5", sizes, VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err),
        1);
    BT_CHECK_EQ(sizes[0], 5);
    BT_CHECK(verify_bigcube_row_sizes(&entry, "parity", sizes, VERIFY_BIGCUBE_MAX_SIZES, err,
                                      sizeof err) < 0);
    bt_pack_entry foreign = entry;
    foreign.n.size_count = 3;
    foreign.n.sizes[0] = 5;
    foreign.n.sizes[1] = 6;
    foreign.n.sizes[2] = 7;
    BT_CHECK(verify_bigcube_row_sizes(&foreign, "oll4", sizes, VERIFY_BIGCUBE_MAX_SIZES, err,
                                      sizeof err) < 0);
}

BT_TEST(verify_ev_bigcube, reduction_rows_verify_at_every_registry_size) {
    bt_pack_entry entry = {0};
    snprintf(entry.file, sizeof entry.file, "%s", "bigcube_l2e.tsv");
    snprintf(entry.event, sizeof entry.event, "%s", "bigcube");
    snprintf(entry.method, sizeof entry.method, "%s", "reduction");
    snprintf(entry.stage, sizeof entry.stage, "%s", "parity");
    entry.n.kind = BT_NSPEC_SIZES;
    entry.n.size_count = 3;
    entry.n.sizes[0] = 5;
    entry.n.sizes[1] = 6;
    entry.n.sizes[2] = 7;
    entry.expected_rows = 12;
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(
        verify_bigcube_row_sizes(&entry, "l2e01", sizes, VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err),
        3);
    BT_CHECK_EQ(sizes[0], 5);
    BT_CHECK_EQ(sizes[1], 6);
    BT_CHECK_EQ(sizes[2], 7);
}

BT_TEST(verify_ev_bigcube, wide_and_rotation_tokens_match_canonical_decompositions) {
    bt_pz_grid wide;
    bt_pz_grid plain;
    grid_after("Rw", 4, &wide);
    grid_after("R 2R", 4, &plain);
    BT_CHECK(bt_pz_grid_eq(&wide, &plain));
    grid_after("3Lw'", 5, &wide);
    grid_after("L' 2L' 3L'", 5, &plain);
    BT_CHECK(bt_pz_grid_eq(&wide, &plain));
    grid_after("x", 4, &wide);
    grid_after("R 2R 3R 4R", 4, &plain);
    BT_CHECK(bt_pz_grid_eq(&wide, &plain));
    grid_after("u2", 5, &wide);
    grid_after("U2 2U2", 5, &plain);
    BT_CHECK(bt_pz_grid_eq(&wide, &plain));
}
