#include "common/bt_harness.h"
#include "core/alg.h"
#include "core/analysis.h"
#include "core/cube.h"
#include "data/corpus.h"
#include "data/paths.h"
#include "data/tsv.h"
#include "data/verify.h"
#include "scheme/classifier.h"
#include "scheme/scheme.h"
#include "unit/data/test_util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_env(const char *name, const char *value) {
    char entry[256];
    snprintf(entry, sizeof entry, "%s=%s", name, value);
    _putenv(entry);
}

static void remove_env(const char *name) {
    char entry[64];
    snprintf(entry, sizeof entry, "%s=", name);
    _putenv(entry);
}

static int ends_with(const char *text, const char *suffix) {
    size_t len = strlen(text);
    size_t suf = strlen(suffix);
    return len >= suf && strcmp(text + len - suf, suffix) == 0;
}

BT_TEST(fuzzkill_data, path_helpers_are_pinned) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    BT_CHECK(root[0] != '\0');
    char out[BT_PATH_MAX];
    bt_data_config_path_from("C:/somewhere/config.json", out);
    BT_CHECK(strcmp(out, "C:/somewhere/config.json") == 0);
    bt_data_config_path_from("rel/config.json", out);
    BT_CHECK(ends_with(out, "rel/config.json"));
    bt_data_config_path_from(NULL, out);
    BT_CHECK(ends_with(out, "config/config.json"));
    bt_json_value *config = bt_json_new_object();
    bt_json_value *paths = bt_json_new_object();
    bt_json_object_set(paths, "ref_ground", bt_json_new_string("rg"));
    bt_json_object_set(config, "paths", paths);
    bt_paths_error err;
    BT_CHECK(bt_data_vendor_root_from("C:/somewhere/config.json", config, out, &err) == 0);
    BT_CHECK(strcmp(out, "C:/somewhere/config.json") == 0);
    BT_CHECK(bt_data_vendor_root_from("", config, out, &err) == 0);
    BT_CHECK(ends_with(out, "rg"));
    bt_json_free(config);
    bt_json_value *loaded = bt_data_load_config();
    BT_CHECK(loaded != NULL);
    bt_json_free(loaded);
    bt_data_user_root_from("C:/users/data", NULL, NULL, out);
    BT_CHECK(strcmp(out, "C:/users/data/blind-trainer") == 0);
    bt_data_user_dir_path_at(out, "srs", out);
    BT_CHECK(ends_with(out, "srs"));
}

BT_TEST(fuzzkill_data, user_algs_round_trip_and_read_json_are_pinned) {
    const char *dir = tu_temp_dir("bt-t14", "algs");
    bt_user_algs algs;
    bt_paths_error err;
    BT_CHECK(bt_user_algs_load_at(&algs, dir, &err) == BT_DATA_READ_MISSING);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_set(&algs, "edge", "UF-FR", "R U R'");
    BT_CHECK(bt_user_algs_is_empty(&algs) == 0);
    BT_CHECK_EQ(bt_user_algs_alg_for(&algs, "edge", "UF-FR"), "R U R'");
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) == 0);
    bt_user_algs reloaded;
    bt_data_read_status read = bt_user_algs_load_at(&reloaded, dir, &err);
    BT_CHECK(read == BT_DATA_READ_OK);
    BT_CHECK(bt_user_algs_eq(&algs, &reloaded) == 1);
    bt_user_algs_free(&reloaded);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF-FR") == 1);
    BT_CHECK(bt_user_algs_remove(&algs, "edge", "UF-FR") == 0);
    BT_CHECK(bt_user_algs_is_empty(&algs) == 1);
    bt_user_algs_free(&algs);
    bt_json_value *value = bt_json_new_object();
    bt_json_object_set(value, "k", bt_json_new_number(7));
    char file[BT_TU_PATH_MAX];
    snprintf(file, sizeof file, "%s/round.json", dir);
    BT_CHECK(bt_data_write_json_atomic(file, value, &err) == 0);
    bt_json_free(value);
    bt_json_value *loaded = NULL;
    BT_CHECK(bt_data_read_json(file, &loaded) == BT_DATA_READ_OK);
    bt_json_free(loaded);
    char missing[BT_TU_PATH_MAX];
    snprintf(missing, sizeof missing, "%s/missing.json", dir);
    BT_CHECK(bt_data_read_json(missing, &loaded) == BT_DATA_READ_MISSING);
    bt_user_algs empty;
    bt_user_algs_init(&empty);
    char blocker[BT_TU_PATH_MAX];
    snprintf(blocker, sizeof blocker, "%s/blocker", dir);
    tu_write_file(blocker, "occupied");
    BT_CHECK(bt_user_algs_save_at(&empty, blocker, &err) != 0);
    bt_user_algs_free(&empty);
    tu_remove_tree(dir);
}

BT_TEST(fuzzkill_data, user_algs_load_and_save_use_the_user_root) {
    const char *dir = tu_temp_dir("bt-t14", "appdata");
    const char *stash = getenv("APPDATA");
    set_env("APPDATA", dir);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_user_algs_set(&algs, "corner", "UF-FR", "R U R'");
    bt_paths_error err;
    BT_CHECK(bt_user_algs_save_at(&algs, dir, &err) == 0);
    bt_user_algs reloaded;
    BT_CHECK(bt_user_algs_load_at(&reloaded, dir, &err) == BT_DATA_READ_OK);
    BT_CHECK_EQ(bt_user_algs_alg_for(&reloaded, "corner", "UF-FR"), "R U R'");
    bt_user_algs_free(&reloaded);
    bt_user_algs_free(&algs);
    if (stash != NULL) {
        set_env("APPDATA", stash);
    } else {
        remove_env("APPDATA");
    }
    tu_remove_tree(dir);
}

static bt_alg alg_of(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    char error[64];
    if (!bt_alg_parse(text, &alg, error, sizeof error)) {
        BT_ABORT("alg parse %s: %s", text, error);
    }
    return alg;
}

static bt_cube cube_of_alg(const char *text) {
    bt_alg alg = alg_of(text);
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &alg);
    bt_alg_free(&alg);
    return cube;
}

static uint8_t slot_of_edge(const bt_cube *cube, uint8_t piece) {
    uint8_t anchor[2];
    bt_edge_stickers(piece, anchor);
    uint8_t position = bt_cube_position_of(cube, anchor[0]);
    for (uint8_t slot = 0; slot < 12; slot++) {
        uint8_t stickers[2];
        bt_edge_stickers(slot, stickers);
        if (stickers[0] == position || stickers[1] == position) {
            return slot;
        }
    }
    BT_ABORT("edge slot always exists");
}

static uint8_t slot_of_corner(const bt_cube *cube, uint8_t piece) {
    uint8_t anchor[3];
    bt_corner_stickers(piece, anchor);
    uint8_t position = bt_cube_position_of(cube, anchor[0]);
    for (uint8_t slot = 0; slot < 8; slot++) {
        uint8_t stickers[3];
        bt_corner_stickers(slot, stickers);
        for (int i = 0; i < 3; i++) {
            if (stickers[i] == position) {
                return slot;
            }
        }
    }
    BT_ABORT("corner slot always exists");
}

static const char *edge_piece_name(uint8_t piece) {
    uint8_t stickers[2];
    bt_edge_stickers(piece, stickers);
    return bt_scheme_name_of(stickers[0]);
}

static const char *corner_piece_name(uint8_t piece) {
    uint8_t stickers[3];
    bt_corner_stickers(piece, stickers);
    return bt_scheme_name_of(stickers[0]);
}

static char *dup_text(const char *text) {
    size_t len = strlen(text) + 1;
    char *copy = malloc(len);
    if (copy == NULL) {
        BT_ABORT("copy allocation failed");
    }
    memcpy(copy, text, len);
    return copy;
}

BT_TEST(fuzzkill_data, display_pins_for_solver_tsv_and_verdict) {
    BT_CHECK_EQ(bt_solver_dir(BT_SOLVER_CHERRY), "cherry");
    BT_CHECK_EQ(bt_solver_dir(BT_SOLVER_EGGINS), "eggins");
    BT_CHECK_EQ(bt_solver_dir(BT_SOLVER_USER), "user");
    const char *sheet_files[BT_SHEET_CLASS_COUNT] = {
        "edge", "corner", "flips", "twists", "parity", "ltct",
    };
    for (int i = 0; i < BT_SHEET_CLASS_COUNT; i++) {
        BT_CHECK_EQ(bt_sheet_class_file((bt_sheet_class)i), sheet_files[i]);
        bt_sheet_class parsed;
        BT_CHECK(bt_sheet_class_parse(sheet_files[i], &parsed));
        BT_CHECK((int)parsed == i);
    }
    char text[96];
    bt_tsv_error tsv;
    tsv.kind = BT_TSV_ERR_BAD_HEADER;
    tsv.line = 2;
    bt_tsv_error_string(&tsv, text, sizeof text);
    BT_CHECK_EQ(text,
                "line 2: header must be Position/Alg/Commutator/Reconstruction Link/Youtube Link");
    tsv.kind = BT_TSV_ERR_COLUMN_COUNT;
    tsv.line = 3;
    tsv.found = 4;
    bt_tsv_error_string(&tsv, text, sizeof text);
    BT_CHECK_EQ(text, "line 3: expected 5 columns, found 4");
    tsv.kind = BT_TSV_ERR_EMPTY_FIELD;
    tsv.line = 4;
    snprintf(tsv.column, sizeof tsv.column, "%s", "Alg");
    bt_tsv_error_string(&tsv, text, sizeof text);
    BT_CHECK_EQ(text, "line 4: Alg column is empty");
    bt_verdict ok_verdict;
    bt_verdict_init(&ok_verdict);
    bt_verdict_string(&ok_verdict, text, sizeof text);
    BT_CHECK_EQ(text, "ok");
    bt_verdict_free(&ok_verdict);
    bt_verdict two_problems;
    bt_verdict_init(&two_problems);
    two_problems.problems = malloc(2 * sizeof *two_problems.problems);
    if (two_problems.problems == NULL) {
        BT_ABORT("problems allocation failed");
    }
    memset(two_problems.problems, 0, 2 * sizeof *two_problems.problems);
    two_problems.problems[0].kind = BT_PROBLEM_BAD_POSITION;
    snprintf(two_problems.problems[0].detail, sizeof two_problems.problems[0].detail, "%s",
             "first");
    two_problems.problems[1].kind = BT_PROBLEM_CENTERS_MOVED;
    two_problems.problem_count = 2;
    bt_verdict_string(&two_problems, text, sizeof text);
    BT_CHECK_EQ(text, "bad position: first\ncenters moved");
    bt_verdict_free(&two_problems);
}

BT_TEST(fuzzkill_data, corpus_is_empty_tracks_rows) {
    bt_corpus empty;
    BT_CHECK(bt_corpus_from_rows(NULL, 0, &empty) == 0);
    BT_CHECK(bt_corpus_is_empty(&empty) == 1);
    BT_CHECK(bt_corpus_total_rows(&empty) == 0);
    bt_corpus_free(&empty);
    bt_corpus_row *rows = malloc(sizeof *rows);
    if (rows == NULL) {
        BT_ABORT("row allocation failed");
    }
    memset(rows, 0, sizeof *rows);
    rows->solver = BT_SOLVER_CHERRY;
    rows->sheet = BT_SHEET_EDGE;
    rows->rowid = 1;
    rows->position = dup_text("UF-FR");
    rows->alg = alg_of("R U R'");
    rows->alg_text = dup_text("R U R'");
    rows->commutator = dup_text("");
    rows->recon_link = dup_text("");
    rows->youtube_link = dup_text("");
    bt_corpus corpus;
    BT_CHECK(bt_corpus_from_rows(rows, 1, &corpus) == 0);
    BT_CHECK(bt_corpus_is_empty(&corpus) == 0);
    BT_CHECK(bt_corpus_total_rows(&corpus) == 1);
    bt_corpus_free(&corpus);
}

BT_TEST(fuzzkill_data, tsv_header_and_row_widths_are_pinned) {
    const char *header = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";
    const char *row = "UF-FR\tR U R'\t\t\t";
    char sheet[256];
    bt_tsv_rows parsed;
    bt_tsv_error err;
    snprintf(sheet, sizeof sheet, "%s\n%s", header, row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) == 0);
    bt_tsv_rows_free(&parsed);
    snprintf(sheet, sizeof sheet, "%s\t\t\n%s", header, row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) == 0);
    bt_tsv_rows_free(&parsed);
    snprintf(sheet, sizeof sheet, "%s\n%s\textra", header, row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT && err.line == 2 && err.found == 6);
    snprintf(sheet, sizeof sheet, "Position\tAlg\tCommutator\tReconstruction Link\t\n%s", row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_BAD_HEADER && err.line == 1);
    snprintf(sheet, sizeof sheet, "Position\tAlg\tCommutator\t\n%s", row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT && err.line == 1 && err.found == 4);
    snprintf(sheet, sizeof sheet, "Position\tAlg\tCommutator\tx\n%s", row);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT && err.line == 1 && err.found == 4);
    snprintf(sheet, sizeof sheet, "%s\nUF-FR\tR U R'\t\t\tkept\tkept2", header);
    BT_CHECK(bt_tsv_parse_sheet(sheet, &parsed, &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_COLUMN_COUNT && err.line == 2 && err.found == 6);
    BT_CHECK(bt_tsv_parse_sheet("Nope\tAlg\tCommutator\tReconstruction Link\tYoutube Link", &parsed,
                                &err) != 0);
    BT_CHECK(err.kind == BT_TSV_ERR_BAD_HEADER && err.line == 1);
}

BT_TEST(fuzzkill_data, verify_reports_displaced_and_center_problems) {
    bt_cube m2 = cube_of_alg("M2");
    uint8_t swapped[12];
    size_t swapped_count = 0;
    for (uint8_t piece = 0; piece < 12; piece++) {
        if (slot_of_edge(&m2, piece) != piece) {
            swapped[swapped_count++] = piece;
        }
    }
    BT_CHECK(swapped_count == 4);
    int have_mutual = 0;
    uint8_t ea = 0;
    uint8_t eb = 0;
    for (size_t i = 0; i < swapped_count && have_mutual == 0; i++) {
        for (size_t j = i + 1; j < swapped_count; j++) {
            if (slot_of_edge(&m2, swapped[i]) == swapped[j] &&
                slot_of_edge(&m2, swapped[j]) == swapped[i]) {
                have_mutual = 1;
                ea = swapped[i];
                eb = swapped[j];
                break;
            }
        }
    }
    BT_CHECK(have_mutual == 1);
    char flips_position[48];
    snprintf(flips_position, sizeof flips_position, "%s & %s", edge_piece_name(ea),
             edge_piece_name(eb));
    bt_alg m2_alg = alg_of("M2");
    bt_verdict flips;
    bt_verify(&m2_alg, flips_position, BT_SHEET_FLIPS, &flips);
    BT_CHECK(bt_verdict_ok(&flips) == 0);
    BT_CHECK(bt_verdict_contains_problem(&flips, BT_PROBLEM_DISPLACED, NULL, NULL, NULL));
    bt_verdict_free(&flips);
    uint8_t unmoved[12];
    size_t unmoved_count = 0;
    for (uint8_t piece = 0; piece < 12; piece++) {
        if (slot_of_edge(&m2, piece) == piece) {
            unmoved[unmoved_count++] = piece;
        }
    }
    char three_style_position[48];
    snprintf(three_style_position, sizeof three_style_position, "%s-%s-%s",
             edge_piece_name(unmoved[0]), edge_piece_name(unmoved[1]), edge_piece_name(unmoved[2]));
    bt_verdict style;
    bt_verify(&m2_alg, three_style_position, BT_SHEET_EDGE, &style);
    BT_CHECK(bt_verdict_ok(&style) == 0);
    BT_CHECK(bt_verdict_contains_problem(&style, BT_PROBLEM_NON_FIXED_RESIDUE, NULL, NULL, NULL));
    bt_verdict_free(&style);
    bt_alg x_alg = alg_of("x");
    bt_verdict rotated;
    bt_verify(&x_alg, three_style_position, BT_SHEET_EDGE, &rotated);
    BT_CHECK(bt_verdict_contains_problem(&rotated, BT_PROBLEM_CENTERS_MOVED, NULL, NULL, NULL));
    bt_verdict_free(&rotated);
    bt_alg_free(&x_alg);
    const char *combo_text = "M2 R U R' D' R U' R' D";
    bt_cube combo = cube_of_alg(combo_text);
    int have_one_way = 0;
    uint8_t ca = 0;
    uint8_t cb = 0;
    for (uint8_t a = 0; a < 8 && have_one_way == 0; a++) {
        uint8_t b = slot_of_corner(&combo, a);
        if (b != a && slot_of_corner(&combo, b) != a) {
            have_one_way = 1;
            ca = a;
            cb = b;
        }
    }
    BT_CHECK(have_one_way == 1);
    char parity_position[96];
    snprintf(parity_position, sizeof parity_position, "%s-%s, %s-%s", edge_piece_name(ea),
             edge_piece_name(eb), corner_piece_name(ca), corner_piece_name(cb));
    bt_alg combo_alg = alg_of(combo_text);
    bt_verdict parity;
    bt_verify(&combo_alg, parity_position, BT_SHEET_PARITY, &parity);
    BT_CHECK(bt_verdict_ok(&parity) == 0);
    BT_CHECK(bt_verdict_contains_problem(&parity, BT_PROBLEM_DISPLACED, NULL, NULL, NULL));
    bt_verdict_free(&parity);
    bt_alg_free(&combo_alg);
    bt_alg_free(&m2_alg);
}

BT_TEST(fuzzkill_data, verify_agrees_with_classifier_on_corner_cycle) {
    bt_alg alg = alg_of("R U R' D' R U' R' D");
    bt_report report;
    bt_scheme_classify_alg(&alg, &report);
    char position[BT_CLASS_POSITION_MAX];
    BT_CHECK(bt_scheme_report_position(&report, position));
    bt_verdict verdict;
    bt_verify(&alg, position, BT_SHEET_CORNER, &verdict);
    BT_CHECK(bt_verdict_ok(&verdict) == 1);
    bt_verdict_free(&verdict);
    bt_alg wrong = alg_of("R U R' D' R U' R' D R");
    bt_verdict wrong_verdict;
    bt_verify(&wrong, position, BT_SHEET_CORNER, &wrong_verdict);
    BT_CHECK(bt_verdict_ok(&wrong_verdict) == 0);
    bt_verdict_free(&wrong_verdict);
    bt_alg_free(&wrong);
    bt_alg_free(&alg);
}

BT_TEST(fuzzkill_data, empty_alg_and_piece_index_helpers_stay_sane) {
    bt_cube solved = bt_cube_solved();
    for (uint8_t piece = 0; piece < 8; piece++) {
        BT_CHECK(slot_of_corner(&solved, piece) == piece);
    }
    for (uint8_t piece = 0; piece < 12; piece++) {
        BT_CHECK(slot_of_edge(&solved, piece) == piece);
    }
    uint8_t corner[3];
    bt_corner_stickers(0, corner);
    bt_piece corner_piece = bt_piece_of(corner[1]);
    BT_CHECK(corner_piece.kind == BT_PIECE_CORNER && corner_piece.index == 0);
    uint8_t edge[2];
    bt_edge_stickers(0, edge);
    bt_piece edge_piece = bt_piece_of(edge[1]);
    BT_CHECK(edge_piece.kind == BT_PIECE_EDGE && edge_piece.index == 0);
}

BT_TEST(fuzzkill_data, verify_accepts_exactly_the_true_corner_cycle) {
    const char *texts[2] = {"R U R' D' R U' R' D", "R U' L' U R' U' L U"};
    for (int t = 0; t < 2; t++) {
        bt_alg alg = alg_of(texts[t]);
        bt_report report;
        bt_scheme_classify_alg(&alg, &report);
        char position[BT_CLASS_POSITION_MAX];
        BT_CHECK(bt_scheme_report_position(&report, position));
        bt_verdict verdict;
        bt_verify(&alg, position, BT_SHEET_CORNER, &verdict);
        BT_CHECK(bt_verdict_ok(&verdict) == 1);
        bt_verdict_free(&verdict);
        bt_cube cube = cube_of_alg(texts[t]);
        uint8_t start = 24;
        for (uint8_t s = 0; s < 24; s++) {
            if (bt_cube_position_of(&cube, s) != s) {
                start = s;
                break;
            }
        }
        uint8_t second = bt_cube_position_of(&cube, start);
        uint8_t third = bt_cube_position_of(&cube, second);
        BT_CHECK(bt_cube_position_of(&cube, third) == start);
        uint8_t cycle_pieces[3];
        uint8_t seeds[3] = {start, second, third};
        for (int i = 0; i < 3; i++) {
            bt_piece piece = bt_piece_of(seeds[i]);
            BT_CHECK(piece.kind == BT_PIECE_CORNER);
            cycle_pieces[i] = piece.index;
        }
        uint8_t pool[9];
        size_t pool_count = 0;
        for (int i = 0; i < 3; i++) {
            uint8_t stickers[3];
            bt_corner_stickers(cycle_pieces[i], stickers);
            for (int j = 0; j < 3; j++) {
                pool[pool_count++] = stickers[j];
            }
        }
        BT_CHECK(pool_count == 9);
        char ok_set[9][48];
        size_t ok_count = 0;
        bool seen[9] = {false};
        for (size_t i = 0; i < pool_count; i++) {
            if (seen[i]) {
                continue;
            }
            uint8_t b = bt_cube_position_of(&cube, pool[i]);
            uint8_t c = bt_cube_position_of(&cube, b);
            BT_CHECK(bt_cube_position_of(&cube, c) == pool[i]);
            seen[i] = true;
            for (size_t j = 0; j < pool_count; j++) {
                if (pool[j] == b || pool[j] == c) {
                    seen[j] = true;
                }
            }
            snprintf(ok_set[ok_count++], sizeof ok_set[0], "%s-%s-%s", bt_scheme_name_of(pool[i]),
                     bt_scheme_name_of(b), bt_scheme_name_of(c));
            snprintf(ok_set[ok_count++], sizeof ok_set[0], "%s-%s-%s", bt_scheme_name_of(b),
                     bt_scheme_name_of(c), bt_scheme_name_of(pool[i]));
            snprintf(ok_set[ok_count++], sizeof ok_set[0], "%s-%s-%s", bt_scheme_name_of(c),
                     bt_scheme_name_of(pool[i]), bt_scheme_name_of(b));
        }
        BT_CHECK(ok_count == 9);
        for (size_t a = 0; a < pool_count; a++) {
            for (size_t b = 0; b < pool_count; b++) {
                for (size_t c = 0; c < pool_count; c++) {
                    if (a == b || b == c || a == c) {
                        continue;
                    }
                    char candidate[48];
                    snprintf(candidate, sizeof candidate, "%s-%s-%s", bt_scheme_name_of(pool[a]),
                             bt_scheme_name_of(pool[b]), bt_scheme_name_of(pool[c]));
                    bool expected_ok = false;
                    for (size_t k = 0; k < ok_count; k++) {
                        if (strcmp(ok_set[k], candidate) == 0) {
                            expected_ok = true;
                        }
                    }
                    bt_verdict verdict;
                    bt_verify(&alg, candidate, BT_SHEET_CORNER, &verdict);
                    BT_CHECK(bt_verdict_ok(&verdict) == (expected_ok ? 1 : 0));
                    bt_verdict_free(&verdict);
                }
            }
        }
        bt_alg_free(&alg);
    }
}
