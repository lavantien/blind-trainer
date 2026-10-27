#include "app/views/drill_card.h"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_case edge_case(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_EDGE;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_EDGE, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_EDGE, second);
    return case_id;
}

static bt_case corner_case(char first, char second) {
    bt_case case_id;
    case_id.orbit = BT_ORBIT_CORNER;
    case_id.t1 = bt_scheme_sticker_of(BT_ORBIT_CORNER, first);
    case_id.t2 = bt_scheme_sticker_of(BT_ORBIT_CORNER, second);
    return case_id;
}

static bt_corpus_row heap_row(bt_solver solver, bt_sheet_class sheet, const char *position,
                              const char *alg_text, const char *commutator) {
    bt_corpus_row row;
    memset(&row, 0, sizeof row);
    row.solver = solver;
    row.sheet = sheet;
    row.rowid = 1;
    row.position = _strdup(position);
    char err_token[64];
    if (!bt_alg_parse(alg_text, &row.alg, err_token, sizeof err_token)) {
        BT_ABORT("fixture alg %s: %s", alg_text, err_token);
    }
    row.alg_text = _strdup(alg_text);
    row.commutator = _strdup(commutator);
    row.recon_link = _strdup("");
    row.youtube_link = _strdup("");
    return row;
}

static bt_corpus_row edge_row(const char *position, const char *alg, const char *commutator) {
    return heap_row(BT_SOLVER_CHERRY, BT_SHEET_EDGE, position, alg, commutator);
}

static bt_corpus corpus_of_rows(bt_corpus_row *rows, size_t count) {
    bt_corpus_row *heap = NULL;
    if (count > 0) {
        heap = malloc(count * sizeof *heap);
        memcpy(heap, rows, count * sizeof *heap);
    }
    bt_corpus corpus;
    bt_corpus_from_rows(heap, count, &corpus);
    return corpus;
}

static bt_corpus single_pair_corpus(const char *alg, const char *commutator) {
    bt_corpus_row rows[1] = {edge_row("UF-UB-UR", alg, commutator)};
    return corpus_of_rows(rows, 1);
}

static bt_stores stores_with_reps(unsigned int reps) {
    bt_stores stores;
    memset(&stores, 0, sizeof stores);
    stores.settings.memo_cadence_ms = 800;
    stores.settings.memo_mode = BT_MEMO_SYLLABLES;
    stores.settings.drill_reps = reps;
    bt_srs_store_init(&stores.srs);
    bt_solve_log_init(&stores.solves);
    bt_user_algs_init(&stores.user_algs);
    bt_str_list_init(&stores.seeds.hidden);
    bt_str_list_init(&stores.seeds.warnings);
    return stores;
}

static bt_syllable_book book_with(const char *orbit, const char *letters, const char *syllable) {
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    if (orbit != NULL) {
        bt_syllable_book_insert(&book, orbit, letters, syllable);
    }
    return book;
}

static void inverse_text_of(const char *text, char out[BT_CARD_TEXT_MAX]) {
    bt_alg alg;
    memset(&alg, 0, sizeof alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse(text, &alg, err_token, sizeof err_token));
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_alg_format(&inverse, &sb);
    snprintf(out, BT_CARD_TEXT_MAX, "%s", sb.data);
    bt_strbuf_free(&sb);
    bt_alg_free(&inverse);
    bt_alg_free(&alg);
}

static int colors_equal(bt_theme_color a, bt_theme_color b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

static const bt_card_row *find_row(const bt_card_content *content, const char *prefix) {
    for (size_t i = 0; i < content->row_count; i++) {
        if (strncmp(content->rows[i].text, prefix, strlen(prefix)) == 0) {
            return &content->rows[i];
        }
    }
    return NULL;
}

static const char *stub_images_for(const char letters[2]) {
    if (letters[0] == 'A' && letters[1] == 'B') {
        return "Anthony Brooks";
    }
    return NULL;
}

BT_TEST(view_drill_card, side_b_uses_the_vendor_flip_when_it_alternates) {
    const char *alg = "R2 U' S R2 S' R2 U R2";
    char inverse[BT_CARD_TEXT_MAX];
    inverse_text_of(alg, inverse);
    bt_corpus_row rows[2] = {
        edge_row("UF-UB-UR", alg, ""),
        heap_row(BT_SOLVER_EGGINS, BT_SHEET_EDGE, "UF-UR-UB", inverse, ""),
    };
    bt_corpus corpus = corpus_of_rows(rows, 2);
    bt_case case_id = edge_case('A', 'B');
    bt_drill_side a;
    bt_drill_side b;
    bt_drill_side_init(&a);
    bt_drill_side_init(&b);
    BT_CHECK(bt_view_build_sides(&corpus, &case_id, &a, &b));
    BT_CHECK(strcmp(a.solver_label, "cherry") == 0);
    BT_CHECK(a.vendor && b.vendor);
    BT_CHECK(strcmp(b.solver_label, "eggins") == 0);
    char b_letters[3];
    bt_drill_side_letters(&b, b_letters);
    BT_CHECK(strcmp(b_letters, "BA") == 0);
    BT_CHECK(strcmp(b.position, "UF-UR-UB") == 0);
    bt_drill_side_free(&a);
    bt_drill_side_free(&b);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, side_b_falls_back_to_the_inverse_without_flip_coverage) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_case case_id = edge_case('A', 'B');
    bt_drill_side a;
    bt_drill_side b;
    bt_drill_side_init(&a);
    bt_drill_side_init(&b);
    BT_CHECK(bt_view_build_sides(&corpus, &case_id, &a, &b));
    BT_CHECK(!b.vendor);
    BT_CHECK(strcmp(b.solver_label, "inverse") == 0);
    bt_alg expected = bt_alg_inverse(&a.alg);
    BT_CHECK(bt_alg_eq(&b.alg, &expected));
    char expected_text[BT_CARD_TEXT_MAX];
    char a_inverse[BT_CARD_TEXT_MAX];
    inverse_text_of(a.alg_text, a_inverse);
    snprintf(expected_text, sizeof expected_text, "%s", a_inverse);
    BT_CHECK(strcmp(b.alg_text, expected_text) == 0);
    BT_CHECK(b.verified);
    bt_alg_free(&expected);
    bt_drill_side_free(&a);
    bt_drill_side_free(&b);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, install_guard_replaces_a_flip_that_breaks_alternation) {
    bt_corpus_row rows[2] = {
        edge_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", ""),
        edge_row("UF-UR-UB", "R U R' U'", ""),
    };
    bt_corpus corpus = corpus_of_rows(rows, 2);
    bt_case case_id = edge_case('A', 'B');
    bt_drill_side a;
    bt_drill_side b;
    bt_drill_side_init(&a);
    bt_drill_side_init(&b);
    BT_CHECK(bt_view_build_sides(&corpus, &case_id, &a, &b));
    BT_CHECK(!b.vendor);
    BT_CHECK(strcmp(b.solver_label, "inverse") == 0);
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &a.alg);
    bt_cube_apply_alg(&cube, &b.alg);
    BT_CHECK(bt_cube_is_solved(&cube));
    bt_drill_side_free(&a);
    bt_drill_side_free(&b);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_content_carries_the_full_pair_card) {
    const char *alg = "R2 U' S R2 S' R2 U R2";
    bt_corpus_row rows[2] = {
        edge_row("UF-UB-UR", alg, "R2 U':[S,R2]"),
        heap_row(BT_SOLVER_EGGINS, BT_SHEET_EDGE, "UF-UB-UR", "M2 U M2 U2 M U M2", ""),
    };
    bt_corpus corpus = corpus_of_rows(rows, 2);
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_syllable_book book = book_with("edge", "AB", "tả");
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
    char inverse[BT_CARD_TEXT_MAX];
    inverse_text_of(alg, inverse);
    bt_alg parsed;
    memset(&parsed, 0, sizeof parsed);
    char err_token[64];
    BT_CHECK(bt_alg_parse(alg, &parsed, err_token, sizeof err_token));
    char metrics[BT_CARD_TEXT_MAX];
    snprintf(metrics, sizeof metrics, "%u qtm %u htm cherry verified", bt_alg_qtm(&parsed),
             bt_alg_htm(&parsed));
    const char *expected[8] = {
        "AB",
        "edge UF-UB-UR",
        "memo tả",
        alg,
    };
    char prepare[BT_CARD_TEXT_MAX];
    snprintf(prepare, sizeof prepare, "prepare BA inverse: %s", inverse);
    char alt[BT_CARD_TEXT_MAX];
    snprintf(alt, sizeof alt, "alt eggins M2 U M2 U2 M U M2");
    BT_CHECK(content.row_count == 8);
    BT_CHECK(strcmp(content.rows[0].text, expected[0]) == 0);
    BT_CHECK(strcmp(content.rows[1].text, expected[1]) == 0);
    BT_CHECK(strcmp(content.rows[2].text, expected[2]) == 0);
    BT_CHECK(strcmp(content.rows[3].text, expected[3]) == 0);
    BT_CHECK(strcmp(content.rows[4].text, prepare) == 0);
    BT_CHECK(strcmp(content.rows[5].text, metrics) == 0);
    BT_CHECK(strcmp(content.rows[6].text, "srs AB new, BA new") == 0);
    BT_CHECK(strcmp(content.rows[7].text, alt) == 0);
    BT_CHECK(content.has_mechanics && content.span_count > 4);
    BT_CHECK(strcmp(content.spans[0].text, "R2 U': ") == 0);
    BT_CHECK(content.spans[0].size_rem == bt_theme_table()->font_alg);
    BT_CHECK(colors_equal(content.spans[0].color, bt_theme_setup()));
    BT_CHECK(strcmp(content.spans[2].text, "S") == 0);
    BT_CHECK(colors_equal(content.spans[2].color, bt_theme_teal()));
    BT_CHECK(strcmp(content.spans[4].text, "R2") == 0);
    BT_CHECK(colors_equal(content.spans[4].color, bt_theme_interchange()));
    bt_alg_free(&parsed);
    bt_syllable_book_free(&book);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_skips_the_raw_commutator_row_when_undecomposable) {
    const char *commutators[2] = {"Not found.", ""};
    for (int i = 0; i < 2; i++) {
        bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", commutators[i]);
        bt_stores stores = stores_with_reps(4);
        bt_drill_session session;
        bt_drill_session_init(&session);
        bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
        bt_syllable_book book;
        bt_syllable_book_init(&book);
        bt_card_content content;
        bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
        BT_CHECK(!content.has_mechanics);
        BT_CHECK(find_row(&content, "commutator") == NULL);
        int alg_row = 0;
        for (size_t r = 0; r < content.row_count; r++) {
            if (content.rows[r].style == BT_ROW_ALG && content.rows[r].text[0] != '\0') {
                alg_row = 1;
            }
        }
        BT_CHECK(alg_row);
        bt_syllable_book_free(&book);
        bt_drill_session_free(&session);
        bt_stores_free(&stores);
        bt_corpus_free(&corpus);
    }
}

BT_TEST(view_drill_card,
        card_falls_back_to_the_raw_commutator_row_when_present_but_undecomposable) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "junk text");
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
    BT_CHECK(!content.has_mechanics);
    const bt_card_row *row = find_row(&content, "commutator ");
    BT_CHECK(row != NULL);
    BT_CHECK(strcmp(row->text, "commutator junk text") == 0);
    BT_CHECK(row->style == BT_ROW_SMALL);
    bt_syllable_book_free(&book);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_adds_the_expand_row_only_when_cancellation_shortens_the_alg) {
    const char *alg_text = "U' R U R' U' U S U' U R U' R' U S'";
    bt_corpus corpus = single_pair_corpus(alg_text, "[U':[R,U],S]");
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
    int expand_found = 0;
    for (size_t r = 0; r < content.row_count; r++) {
        if (content.rows[r].style == BT_ROW_SMALL &&
            strcmp(content.rows[r].text, "expand U' R U R' S R U' R' U S'") == 0) {
            expand_found = 1;
        }
    }
    BT_CHECK(expand_found);
    bt_corpus plain = single_pair_corpus("R2 U' S R2 S' R2 U R2", "R2 U':[S,R2]");
    bt_drill_session plain_session;
    bt_drill_session_init(&plain_session);
    bt_view_install_pair(&plain_session, &plain, &stores, edge_case('A', 'B'));
    bt_card_content plain_content;
    bt_view_card_content(&plain_session, &plain, &book, &stores, 0, &plain_content);
    BT_CHECK(find_row(&plain_content, "expand ") == NULL);
    bt_syllable_book_free(&book);
    bt_drill_session_free(&plain_session);
    bt_corpus_free(&plain);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_srs_line_reads_each_side_card_state) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_stores stores = stores_with_reps(4);
    bt_card seen;
    memset(&seen, 0, sizeof seen);
    seen.interval_days = 6;
    seen.due_day = 0;
    seen.reps = 2;
    bt_srs_store_set(&stores.srs, "edge/UF-UB-UR", &seen);
    bt_card later = seen;
    later.due_day = 10;
    bt_srs_store_set(&stores.srs, "edge/UF-UR-UB", &later);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
    const bt_card_row *row = find_row(&content, "srs ");
    BT_CHECK(row != NULL);
    BT_CHECK(strcmp(row->text, "srs AB due, BA ok") == 0);
    bt_syllable_book_free(&book);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_prepare_line_prefers_the_vendor_flip_alg) {
    const char *alg = "R2 U' S R2 S' R2 U R2";
    char inverse[BT_CARD_TEXT_MAX];
    inverse_text_of(alg, inverse);
    bt_corpus_row rows[2] = {
        edge_row("UF-UB-UR", alg, ""),
        heap_row(BT_SOLVER_EGGINS, BT_SHEET_EDGE, "UF-UR-UB", inverse, ""),
    };
    bt_corpus corpus = corpus_of_rows(rows, 2);
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_card_content content;
    bt_view_card_content(&session, &corpus, &book, &stores, 0, &content);
    const bt_card_row *row = find_row(&content, "prepare ");
    BT_CHECK(row != NULL);
    char expected[BT_CARD_TEXT_MAX];
    snprintf(expected, sizeof expected, "prepare BA eggins: %s", inverse);
    BT_CHECK(strcmp(row->text, expected) == 0);
    BT_CHECK(row->style == BT_ROW_BODY);
    bt_syllable_book_free(&book);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, card_lines_pick_tokens_per_orbit_with_letter_fallback) {
    bt_views_set_image_lookup(stub_images_for);
    bt_stores stores = stores_with_reps(4);
    bt_corpus_row corner_rows[1] = {
        heap_row(BT_SOLVER_CHERRY, BT_SHEET_CORNER, "UFR-UBL-UBR", "R U' L' U", ""),
    };
    bt_corpus corner_corpus = corpus_of_rows(corner_rows, 1);
    bt_drill_session corner_session;
    bt_drill_session_init(&corner_session);
    bt_view_install_pair(&corner_session, &corner_corpus, &stores, corner_case('A', 'B'));
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_card_content corner_content;
    bt_view_card_content(&corner_session, &corner_corpus, &book, &stores, 0, &corner_content);
    BT_CHECK(strcmp(corner_content.rows[0].text, "AB") == 0);
    BT_CHECK(strcmp(corner_content.rows[1].text, "corner UFR-UBL-UBR") == 0);
    char corner_expected[BT_CARD_TEXT_MAX];
    snprintf(corner_expected, sizeof corner_expected, "memo %s", stub_images_for("AB"));
    BT_CHECK(strcmp(corner_content.rows[2].text, corner_expected) == 0);
    bt_corpus edge_corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_drill_session edge_session;
    bt_drill_session_init(&edge_session);
    bt_view_install_pair(&edge_session, &edge_corpus, &stores, edge_case('A', 'B'));
    bt_card_content edge_content;
    bt_view_card_content(&edge_session, &edge_corpus, &book, &stores, 0, &edge_content);
    BT_CHECK(strcmp(edge_content.rows[2].text, "memo AB") == 0);
    bt_views_set_image_lookup(NULL);
    bt_drill_session_free(&edge_session);
    bt_corpus_free(&edge_corpus);
    bt_drill_session_free(&corner_session);
    bt_corpus_free(&corner_corpus);
    bt_syllable_book_free(&book);
    bt_stores_free(&stores);
}

BT_TEST(view_drill_card, mechanic_spans_join_multiple_units_with_a_separator) {
    bt_alg alg;
    memset(&alg, 0, sizeof alg);
    char err_token[64];
    BT_CHECK(bt_alg_parse("S R2 S' R2 U R2 U' R2", &alg, err_token, sizeof err_token));
    bt_card_content content;
    memset(&content, 0, sizeof content);
    BT_CHECK(bt_view_mechanic_spans("[S,R2] + [U,R2]", &alg, &content));
    char joined[BT_CARD_TEXT_MAX * 4];
    joined[0] = '\0';
    for (size_t i = 0; i < content.span_count; i++) {
        strcat(joined, content.spans[i].text);
    }
    BT_CHECK(strcmp(joined, "[S, R2] + [U, R2]") == 0);
    BT_CHECK(strcmp(content.spans[5].text, " + ") == 0);
    BT_CHECK(content.spans[5].size_rem == bt_theme_table()->font_alg);
    BT_CHECK(colors_equal(content.spans[5].color, bt_theme_text_primary()));
    BT_CHECK(strcmp(content.spans[7].text, "U") == 0);
    BT_CHECK(colors_equal(content.spans[7].color, bt_theme_teal()));
    BT_CHECK(strcmp(content.spans[9].text, "R2") == 0);
    BT_CHECK(colors_equal(content.spans[9].color, bt_theme_interchange()));
    bt_alg_free(&alg);
}

BT_TEST(view_drill_card, reps_lines_show_the_executing_side_in_whole_seconds) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_stores stores = stores_with_reps(4);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    session.reps_done = 2;
    session.reps_side_a = 0;
    session.reps_clock = 3.7F;
    char lines[3][BT_CARD_TEXT_MAX];
    size_t count = 0;
    bt_view_reps_lines(&session, lines, 3, &count);
    BT_CHECK(count == 3);
    BT_CHECK(strcmp(lines[0], "rep 2 of 4") == 0);
    char expected[BT_CARD_TEXT_MAX];
    snprintf(expected, sizeof expected, "BA %s", session.side_b.alg_text);
    BT_CHECK(strcmp(lines[1], expected) == 0);
    BT_CHECK(strcmp(lines[2], "3s") == 0);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, rate_lines_prompt_each_side_in_order) {
    bt_corpus corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
    bt_stores stores = stores_with_reps(2);
    bt_drill_session session;
    bt_drill_session_init(&session);
    bt_view_install_pair(&session, &corpus, &stores, edge_case('A', 'B'));
    session.splits[0].is_a = 1;
    session.splits[0].secs = 1.0F;
    session.splits[1].is_a = 0;
    session.splits[1].secs = 2.0F;
    session.split_count = 2;
    char lines[3][BT_CARD_TEXT_MAX];
    size_t count = 0;
    bt_view_rate_lines(&session, lines, 3, &count);
    BT_CHECK(count == 3);
    BT_CHECK(strcmp(lines[0], "rate AB") == 0);
    BT_CHECK(strcmp(lines[1], "2 reps avg 1.5s") == 0);
    session.rate_step = 1;
    bt_view_rate_lines(&session, lines, 3, &count);
    BT_CHECK(strcmp(lines[0], "rate BA") == 0);
    bt_drill_session_free(&session);
    bt_stores_free(&stores);
    bt_corpus_free(&corpus);
}

BT_TEST(view_drill_card, reps_and_rate_lines_report_no_pair_without_sides) {
    bt_drill_session session;
    bt_drill_session_init(&session);
    char lines[3][BT_CARD_TEXT_MAX];
    size_t count = 0;
    bt_view_reps_lines(&session, lines, 3, &count);
    BT_CHECK(count == 1);
    BT_CHECK(strcmp(lines[0], "no pair") == 0);
    bt_view_rate_lines(&session, lines, 3, &count);
    BT_CHECK(count == 1);
    BT_CHECK(strcmp(lines[0], "no pair") == 0);
    bt_drill_session_free(&session);
}
