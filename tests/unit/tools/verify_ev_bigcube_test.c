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

static void bigcube_entry(const char *method, long long first, size_t size_count,
                          bt_pack_entry *out) {
    memset(out, 0, sizeof *out);
    snprintf(out->file, sizeof out->file, "%s", "bigcube_parity.tsv");
    snprintf(out->event, sizeof out->event, "%s", "bigcube");
    snprintf(out->method, sizeof out->method, "%s", method);
    snprintf(out->stage, sizeof out->stage, "%s", "parity");
    out->n.kind = BT_NSPEC_SIZES;
    out->n.size_count = size_count;
    out->n.sizes[0] = first;
    out->n.sizes[1] = first + 1;
    out->n.sizes[2] = first + 2;
    out->n.sizes[3] = first + 3;
    out->expected_rows = 1;
}

static void bigcube_row(const char *case_key, const char *front_json, const char *alg,
                        bt_seed_rows *out, bt_seed_row *storage) {
    snprintf(storage->case_key, sizeof storage->case_key, "%s", case_key);
    storage->alg = (char *)alg;
    storage->front_json = (char *)front_json;
    storage->source_url = (char *)"https://example.test";
    storage->source_at = (char *)"2026-10-10";
    out->items = storage;
    out->count = 1;
}

BT_TEST(verify_ev_bigcube, a_row_without_a_case_tag_names_the_missing_tag) {
    bt_pack_entry entry;
    bigcube_entry("yau", 4, 4, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("parity.x", "{\"nope\":1}", "R", &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "front_json holds no case tag");
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, an_oversized_digit_tag_holds_no_cube_size) {
    bt_pack_entry entry;
    bigcube_entry("yau", 4, 4, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("parity.big", "{\"case\":\"parity99999999999999999999\"}", "R", &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier1_err, "tag parity99999999999999999999 holds no cube size");
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, a_long_in_range_tag_keeps_every_digit) {
    bt_pack_entry entry;
    bigcube_entry("yau", 4, 4, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("parity.long", "{\"case\":\"parity11111111111111111111\"}", "R", &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK(err_contains(section.rows[0].tier1_err,
                          "tag size 11111111111111111111 sits outside the registry sizes"));
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, an_engine_init_failure_fails_the_row) {
    bt_pack_entry entry;
    bigcube_entry("reduction", 9996, 4, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("l2e.probe", "{\"case\":\"l2e01\"}", "R", &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK(err_contains(section.rows[0].tier1_err, "engine init failed"));
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_SKIPPED);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, an_overflowing_digit_run_skips_its_remaining_digits) {
    size_t size = 0;
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(
        verify_bigcube_tagged_size("parity9999999999999999999999999", &size, err, sizeof err), 1);
    BT_CHECK_EQ(err, "tag parity9999999999999999999999999 holds no cube size");
}

static void crafted_state(size_t n, size_t index, uint8_t color, bt_pz_nxn *engine,
                          bt_pz_nxn_state *out) {
    BT_CHECK(bt_pz_nxn_init(engine, n));
    bt_pz_nxn_solved_state(engine, out);
    out->facelet[index] = color;
}

BT_TEST(verify_ev_bigcube, a_foreign_center_color_collapses_the_distinct_count) {
    bt_pz_nxn engine;
    bt_pz_nxn_state state;
    BT_CHECK(bt_pz_nxn_init(&engine, 5));
    bt_pz_nxn_solved_state(&engine, &state);
    size_t base = 4 * BT_PZ_NXN_STRIDE * BT_PZ_NXN_STRIDE;
    for (size_t row = 1; row + 1 < 5; row++) {
        for (size_t col = 1; col + 1 < 5; col++) {
            state.facelet[base + row * BT_PZ_NXN_STRIDE + col] = 0;
        }
    }
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_reduction_state(&engine, &state, err, sizeof err), -1);
    BT_CHECK_EQ(err, "the six center blocks hold only 5 distinct colors");
}

BT_TEST(verify_ev_bigcube, a_foreign_corner_sticker_fails_the_corner_audit) {
    bt_pz_nxn engine;
    bt_pz_nxn_state state;
    crafted_state(5, 0, 4, &engine, &state);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_reduction_state(&engine, &state, err, sizeof err), -1);
    BT_CHECK_EQ(err, "some corner sticker left its face color");
}

BT_TEST(verify_ev_bigcube, a_foreign_midge_sticker_fails_the_midge_audit) {
    bt_pz_nxn engine;
    bt_pz_nxn_state state;
    crafted_state(5, 2, 4, &engine, &state);
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_reduction_state(&engine, &state, err, sizeof err), -1);
    BT_CHECK_EQ(err, "some midge sticker left its face color");
}

BT_TEST(verify_ev_bigcube, an_empty_registry_holds_no_cube_sizes) {
    bt_pack_entry entry = {0};
    entry.n.kind = BT_NSPEC_SIZES;
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(
        verify_bigcube_row_sizes(&entry, "l2e01", sizes, VERIFY_BIGCUBE_MAX_SIZES, err, sizeof err),
        -1);
    BT_CHECK_EQ(err, "registry holds no cube sizes for this pack");
}

BT_TEST(verify_ev_bigcube, a_small_out_cap_clips_the_registry_sizes) {
    bt_pack_entry entry;
    bigcube_entry("reduction", 5, 3, &entry);
    size_t sizes[VERIFY_BIGCUBE_MAX_SIZES];
    char err[VERIFY_ERR_MAX];
    BT_CHECK_EQ(verify_bigcube_row_sizes(&entry, "l2e01", sizes, 1, err, sizeof err), 1);
    BT_CHECK_EQ(sizes[0], 5);
}

BT_TEST(verify_ev_bigcube, broken_front_json_names_the_json_error) {
    char tag[64];
    BT_CHECK_EQ(verify_bigcube_tag_of("{\"case\",", tag, sizeof tag), 1);
    BT_CHECK(tag[0] != '\0');
}

BT_TEST(verify_ev_bigcube, reduction_rows_name_the_parse_and_empty_failures) {
    bt_pack_entry entry;
    bigcube_entry("reduction", 4, 1, &entry);
    bt_seed_row storage[2];
    bt_seed_rows first;
    bigcube_row("l2e.bad", "{\"case\":\"l2e01\"}", "Z", &first, &storage[0]);
    bt_seed_rows second;
    bigcube_row("l2e.empty", "{\"case\":\"l2e01\"}", "", &second, &storage[1]);
    bt_seed_rows rows = {storage, 2};
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 2);
    BT_CHECK(!section.rows[0].tier1_ok);
    BT_CHECK(err_contains(section.rows[0].tier1_err, "n=4:"));
    BT_CHECK(err_contains(section.rows[0].tier1_err, "Z"));
    BT_CHECK(!section.rows[1].tier1_ok);
    BT_CHECK(err_contains(section.rows[1].tier1_err, "n=4: alg is empty"));
    verify_section_free(&section);
}

static const char DOUBLE_SLICE_WING_PARITY[] = "Rw2 2U2 2F2 2U2 Rw2";

BT_TEST(verify_ev_bigcube, the_double_slice_wing_parity_is_a_self_inverse_disturbance) {
    bt_pz_nxn engine;
    BT_CHECK(bt_pz_nxn_init(&engine, 4));
    bt_pz_error pz_err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    BT_CHECK(eng.parse_alg(&eng, DOUBLE_SLICE_WING_PARITY, strlen(DOUBLE_SLICE_WING_PARITY), alg,
                           &pz_err));
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    bt_pz_nxn_apply_alg(&engine, &state, alg, &pz_err);
    bt_pz_nxn_state twice = state;
    bt_pz_nxn_apply_alg(&engine, &twice, alg, &pz_err);
    bt_pz_nxn_state solved;
    bt_pz_nxn_solved_state(&engine, &solved);
    BT_CHECK(!bt_pz_nxn_state_eq(&state, &solved));
    BT_CHECK(bt_pz_nxn_state_eq(&twice, &solved));
    bt_pz_alg_destroy(alg);
}

BT_TEST(verify_ev_bigcube, a_two_line_disturbance_passes_the_reduction_band) {
    bt_pack_entry entry;
    bigcube_entry("reduction", 4, 1, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("l2e.wing", "{\"case\":\"l2e01\"}", DOUBLE_SLICE_WING_PARITY, &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_PASS);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, a_two_line_disturbance_passes_the_yau_band_at_its_tag) {
    bt_pack_entry entry;
    bigcube_entry("yau", 4, 1, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("parity.wing", "{\"case\":\"wing4\"}", DOUBLE_SLICE_WING_PARITY, &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_PASS);
    verify_section_free(&section);
}

BT_TEST(verify_ev_bigcube, a_rotation_only_alg_disturbs_no_lines_and_fails_the_band) {
    bt_pack_entry entry;
    bigcube_entry("reduction", 4, 1, &entry);
    bt_seed_row storage;
    bt_seed_rows rows;
    bigcube_row("l2e.rot", "{\"case\":\"l2e01\"}", "x", &rows, &storage);
    verify_section_result section;
    verify_bigcube_verify_parity(&entry, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK_EQ(section.rows[0].tier2.kind, VERIFY_TIER2_FAIL);
    BT_CHECK(err_contains(section.rows[0].tier2.fail, "disturbs 0 dedge lines"));
    verify_section_free(&section);
}
