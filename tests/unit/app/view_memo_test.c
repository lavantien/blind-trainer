#include "app/theme.h"
#include "app/ui.h"
#include "app/views/frame.h"
#include "app/views/memo.h"
#include "app/window.h"

#include "app/views/images.h"
#include "common/alloc.h"
#include "data/fsrs.h"
#include "scheme/scheme.h"

#include "common.h"

#include "raylib.h"

#include "data/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bt_stores empty_stores(void) {
    bt_stores stores;
    memset(&stores, 0, sizeof stores);
    stores.settings.memo_cadence_ms = 800;
    stores.settings.memo_mode = BT_MEMO_PER_ORBIT;
    stores.settings.drill_reps = 6;
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

static void sample_memo(bt_memo *out) {
    bt_cube scramble;
    bt_scheme_random_state(12345, &scramble);
    bt_scheme_memo(&scramble, out);
}

static void memo_with_edge_pair(char first, char second, bt_memo *out) {
    for (uint64_t seed = 0; seed < 5000; seed++) {
        bt_cube scramble;
        bt_scheme_random_state(seed, &scramble);
        bt_scheme_memo(&scramble, out);
        for (size_t i = 0; i < out->edge_pair_count; i++) {
            char letters[2];
            bt_scheme_case_letters(&out->edge_pairs[i], letters);
            if (letters[0] == first && letters[1] == second) {
                return;
            }
        }
    }
    BT_ABORT("no seed produced edge pair %c%c", first, second);
}

BT_TEST(view_memo, tokens_fall_back_to_letters_without_syllables) {
    bt_memo memo;
    sample_memo(&memo);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t count = 0;
    bt_memo_build_tokens(&memo, &book, tokens, &count);
    BT_CHECK(count == memo.edge_pair_count + memo.corner_pair_count);
    size_t at = 0;
    for (int orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? memo.edge_pairs : memo.corner_pairs;
        size_t pair_count = orbit == 0 ? memo.edge_pair_count : memo.corner_pair_count;
        for (size_t i = 0; i < pair_count; i++) {
            char letters[2];
            bt_scheme_case_letters(&pairs[i], letters);
            BT_CHECK(tokens[at].orbit == pairs[i].orbit);
            BT_CHECK(tokens[at].letters[0] == letters[0] && tokens[at].letters[1] == letters[1]);
            char pair[3] = {letters[0], letters[1], '\0'};
            char text[BT_MEMO_TEXT_MAX];
            bt_memo_token_text(&tokens[at], BT_MEMO_SYLLABLES, text);
            BT_CHECK(strcmp(text, pair) == 0);
            at++;
        }
    }
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, syllable_book_overrides_pair_text_when_present) {
    bt_syllable_book book = book_with(NULL, NULL, NULL);
    bt_syllable_book_insert(&book, "edge", "AB", "tả");
    bt_memo memo;
    memo_with_edge_pair('A', 'B', &memo);
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t count = 0;
    bt_memo_build_tokens(&memo, &book, tokens, &count);
    const bt_memo_token *ab = NULL;
    for (size_t i = 0; i < count; i++) {
        if (tokens[i].letters[0] == 'A' && tokens[i].letters[1] == 'B' &&
            tokens[i].orbit == BT_ORBIT_EDGE) {
            ab = &tokens[i];
        }
    }
    BT_CHECK(ab != NULL);
    char text[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(ab, BT_MEMO_SYLLABLES, text);
    BT_CHECK(strcmp(text, "tả") == 0);
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, image_mode_uses_words_when_available) {
    bt_view_images_install();
    bt_memo memo;
    memo_with_edge_pair('A', 'B', &memo);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t count = 0;
    bt_memo_build_tokens(&memo, &book, tokens, &count);
    const bt_memo_token *ab = NULL;
    for (size_t i = 0; i < count; i++) {
        if (tokens[i].letters[0] == 'A' && tokens[i].letters[1] == 'B' &&
            tokens[i].orbit == BT_ORBIT_EDGE) {
            ab = &tokens[i];
        }
    }
    BT_CHECK(ab != NULL);
    char text[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(ab, BT_MEMO_IMAGES, text);
    BT_CHECK(strcmp(text, "Anthony Brooks") == 0);
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, per_orbit_streams_edge_syllables_and_corner_images) {
    bt_memo_token edge;
    memset(&edge, 0, sizeof edge);
    edge.orbit = BT_ORBIT_EDGE;
    edge.letters[0] = 'A';
    edge.letters[1] = 'B';
    edge.syllable = _strdup("tả");
    edge.image = "Anthony Brooks";
    bt_memo_token corner;
    memset(&corner, 0, sizeof corner);
    corner.orbit = BT_ORBIT_CORNER;
    corner.letters[0] = 'A';
    corner.letters[1] = 'B';
    corner.syllable = _strdup("tả");
    corner.image = "Anthony Brooks";
    char text[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(&edge, BT_MEMO_PER_ORBIT, text);
    BT_CHECK(strcmp(text, "tả") == 0);
    bt_memo_token_text(&corner, BT_MEMO_PER_ORBIT, text);
    BT_CHECK(strcmp(text, "Anthony Brooks") == 0);
    free(edge.syllable);
    free(corner.syllable);
}

BT_TEST(view_memo, per_orbit_falls_back_to_letters_when_the_orbit_book_lacks_the_pair) {
    bt_memo_token edge;
    memset(&edge, 0, sizeof edge);
    edge.orbit = BT_ORBIT_EDGE;
    edge.letters[0] = 'C';
    edge.letters[1] = 'D';
    bt_memo_token corner;
    memset(&corner, 0, sizeof corner);
    corner.orbit = BT_ORBIT_CORNER;
    corner.letters[0] = 'E';
    corner.letters[1] = 'F';
    corner.syllable = _strdup("bo");
    char text[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(&edge, BT_MEMO_PER_ORBIT, text);
    BT_CHECK(strcmp(text, "CD") == 0);
    bt_memo_token_text(&corner, BT_MEMO_PER_ORBIT, text);
    BT_CHECK(strcmp(text, "EF") == 0);
    free(corner.syllable);
}

BT_TEST(view_memo, stream_rows_center_the_current_token_with_trailing_recents) {
    bt_memo memo;
    sample_memo(&memo);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_session session;
    bt_memo_session_init(&session);
    bt_memo_build_tokens(&memo, &book, session.tokens, &session.token_count);
    bt_stores stores = empty_stores();
    bt_memo_mode mode = stores.settings.memo_mode;
    size_t shown = 3;
    if (shown > session.token_count) {
        shown = session.token_count;
    }
    session.shown = shown;
    bt_memo_rows rows;
    bt_memo_view_rows(BT_MEMO_STREAM, &session, &stores, &rows);
    char focal[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(&session.tokens[shown - 1], mode, focal);
    BT_CHECK(strcmp(rows.focal, focal) == 0);
    char recents[BT_MEMO_RECENT_MAX];
    recents[0] = '\0';
    for (size_t i = 0; i + 1 < shown; i++) {
        char text[BT_MEMO_TEXT_MAX];
        bt_memo_token_text(&session.tokens[i], mode, text);
        if (i > 0) {
            strcat(recents, " ");
        }
        strcat(recents, text);
    }
    BT_CHECK(strcmp(rows.recent, recents) == 0);
    char marker[32];
    snprintf(marker, sizeof marker, "%zu/", shown);
    BT_CHECK(strstr(rows.status, marker) != NULL);
    bt_stores_free(&stores);
    bt_memo_session_free(&session);
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, review_rows_list_every_token_with_residue_status) {
    bt_memo memo;
    sample_memo(&memo);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_session session;
    bt_memo_session_init(&session);
    bt_memo_build_tokens(&memo, &book, session.tokens, &session.token_count);
    session.shown = session.token_count;
    session.has_memo = 1;
    session.memo = memo;
    bt_stores stores = empty_stores();
    bt_memo_mode mode = stores.settings.memo_mode;
    bt_memo_rows rows;
    bt_memo_view_rows(BT_MEMO_REVIEW, &session, &stores, &rows);
    char all[BT_MEMO_RECENT_MAX];
    all[0] = '\0';
    for (size_t i = 0; i < session.token_count; i++) {
        char text[BT_MEMO_TEXT_MAX];
        bt_memo_token_text(&session.tokens[i], mode, text);
        if (i > 0) {
            strcat(all, " ");
        }
        strcat(all, text);
    }
    BT_CHECK(strcmp(rows.recent, all) == 0);
    char lines[16][BT_MEMO_TEXT_MAX];
    size_t line_count = 0;
    bt_memo_residue_lines(&memo, lines, 16, &line_count);
    char residue[BT_MEMO_TEXT_MAX * 8];
    residue[0] = '\0';
    for (size_t i = 0; i < line_count; i++) {
        if (i > 0) {
            strcat(residue, "; ");
        }
        strcat(residue, lines[i]);
    }
    BT_CHECK(strstr(rows.status, residue) != NULL);
    BT_CHECK(strstr(rows.status, "rate") != NULL);
    bt_stores_free(&stores);
    bt_memo_session_free(&session);
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, stream_advances_only_at_cadence) {
    bt_memo_session session;
    bt_memo_session_init(&session);
    bt_stores stores = empty_stores();
    bt_memo memo;
    sample_memo(&memo);
    bt_syllable_book book;
    bt_syllable_book_init(&book);
    bt_memo_build_tokens(&memo, &book, session.tokens, &session.token_count);
    BT_CHECK(session.token_count > 0);
    session.clock = 0.0F;
    session.shown = 0;
    session.clock += 0.5F * bt_memo_cadence_secs(&session, &stores);
    BT_CHECK(session.clock < bt_memo_cadence_secs(&session, &stores));
    bt_syllable_book_free(&book);
    bt_memo_session_free(&session);
    bt_stores_free(&stores);
}

static const char STATUS_LONG[] =
    "parity UF-UB, UFR-UBR; flip UF & UL; flip UR & FL; flip UB & DF; flip DL & UB; flip FU & BR; "
    "flip LU & RF; twist UFR x2; twist UBL x1; twist DFR x2; twist DBR x1; twist ULB x2; "
    "twist URF x1; twist DLF x2; twist DBL x1; r replay  space next  1..4 rate pairs";
static const char STATUS_SHORT[] = "stream 3/20  cadence 800 ms";

enum {
    WRAP_WORD_MAX = 64,
};

static int with_memo_fonts(int (*run)(const bt_app_font_book *)) {
    if (!bt_app_window_open_driven(640, 480, "view memo test", NULL)) {
        return 0;
    }
    bt_app_font_book book;
    char error[BT_APP_ERROR_MAX];
    char path[BT_PATH_MAX];
    bt_data_config_path(path);
    if (bt_app_fonts_load(path, NULL, NULL, 0, &book, error, sizeof error) != 0) {
        bt_app_window_close();
        BT_ABORT("fonts: %s", error);
    }
    int ok = run(&book);
    bt_app_fonts_unload(&book);
    bt_app_window_close();
    return ok;
}

static float memo_measure(const bt_app_font_book *book, const char *text, float size) {
    Font regular = *(const Font *)book->regular;
    return MeasureTextEx(regular, text, size, 1.0F).x;
}

static size_t split_words(const char *text, char out[WRAP_WORD_MAX][BT_MEMO_TEXT_MAX]) {
    size_t count = 0;
    const char *at = text;
    while (*at != '\0' && count < WRAP_WORD_MAX) {
        while (*at == ' ') {
            at++;
        }
        if (*at == '\0') {
            break;
        }
        const char *start = at;
        while (*at != '\0' && *at != ' ') {
            at++;
        }
        size_t len = (size_t)(at - start);
        if (len >= BT_MEMO_TEXT_MAX) {
            len = BT_MEMO_TEXT_MAX - 1;
        }
        memcpy(out[count], start, len);
        out[count][len] = '\0';
        count++;
    }
    return count;
}

static int words_match(const char *left, const char *right) {
    char left_words[WRAP_WORD_MAX][BT_MEMO_TEXT_MAX];
    char right_words[WRAP_WORD_MAX][BT_MEMO_TEXT_MAX];
    size_t left_count = split_words(left, left_words);
    size_t right_count = split_words(right, right_words);
    if (left_count != right_count) {
        return 0;
    }
    for (size_t i = 0; i < left_count; i++) {
        if (strcmp(left_words[i], right_words[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

static int run_status_wrap(const bt_app_font_book *book) {
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    float body = theme->font_body * theme->root_rem;
    float full = memo_measure(book, STATUS_LONG, body);
    if (!(full > metrics.content_w * 2.0F)) {
        return 0;
    }
    char lines[BT_MEMO_WRAP_MAX][BT_MEMO_TEXT_MAX];
    size_t count = bt_memo_wrap_lines(book->regular, STATUS_LONG, body, metrics.content_w, lines,
                                      BT_MEMO_WRAP_MAX, BT_MEMO_WRAP_MAX);
    if (count < 3) {
        return 0;
    }
    char joined[BT_MEMO_STATUS_MAX];
    joined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (lines[i][0] == '\0') {
            return 0;
        }
        if (memo_measure(book, lines[i], body) > metrics.content_w + 0.5F) {
            return 0;
        }
        if (i > 0) {
            strcat(joined, " ");
        }
        strcat(joined, lines[i]);
    }
    if (!words_match(STATUS_LONG, joined)) {
        return 0;
    }
    size_t short_count = bt_memo_wrap_lines(book->regular, STATUS_SHORT, body, metrics.content_w,
                                            lines, BT_MEMO_WRAP_MAX, BT_MEMO_WRAP_MAX);
    return short_count == 1 && strcmp(lines[0], STATUS_SHORT) == 0;
}

BT_TEST(view_memo, status_wrap_spreads_a_long_review_status_across_the_content_width) {
    BT_CHECK_EQ(strlen(STATUS_LONG), (size_t)256);
    BT_CHECK(with_memo_fonts(run_status_wrap));
}

static int run_status_clamp(const bt_app_font_book *book) {
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    float body = theme->font_body * theme->root_rem;
    char lines[BT_MEMO_WRAP_MAX][BT_MEMO_TEXT_MAX];
    size_t count = bt_memo_wrap_lines(book->regular, STATUS_LONG, body, metrics.content_w, lines,
                                      BT_MEMO_WRAP_MAX, 2);
    if (count != 2) {
        return 0;
    }
    size_t last = strlen(lines[1]);
    if (last < 3 || strcmp(lines[1] + last - 3, "...") != 0) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (memo_measure(book, lines[i], body) > metrics.content_w + 0.5F) {
            return 0;
        }
    }
    return 1;
}

BT_TEST(view_memo, status_wrap_clamps_to_the_line_budget_with_a_trailing_ellipsis) {
    BT_CHECK(with_memo_fonts(run_status_clamp));
}

static int run_wrap_edges(const bt_app_font_book *book) {
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(1280.0F, 800.0F);
    float body = theme->font_body * theme->root_rem;
    char lines[BT_MEMO_WRAP_MAX][BT_MEMO_TEXT_MAX];
    size_t empty = bt_memo_wrap_lines(book->regular, "", body, metrics.content_w, lines,
                                      BT_MEMO_WRAP_MAX, BT_MEMO_WRAP_MAX);
    size_t single = bt_memo_wrap_lines(book->regular, "ready", body, metrics.content_w, lines,
                                       BT_MEMO_WRAP_MAX, 4);
    int single_ok = single == 1 && strcmp(lines[0], "ready") == 0;
    size_t starved = bt_memo_wrap_lines(book->regular, "ready", body, metrics.content_w, lines,
                                        BT_MEMO_WRAP_MAX, 0);
    return empty == 0 && single_ok && starved == 0;
}

BT_TEST(view_memo, wrap_handles_empty_short_and_starved_line_budgets) {
    BT_CHECK(with_memo_fonts(run_wrap_edges));
}

enum {
    WRAP_WORD_CHARS = 60,
    WRAP_WORD_BUDGET = 300,
    MEMO_DRAW_W = 640,
    MEMO_DRAW_H = 480,
    MEMO_DWELL_FRAMES = 6,
};

static void oversized_word(char out[WRAP_WORD_CHARS + 1]) {
    memset(out, 'a', WRAP_WORD_CHARS);
    out[WRAP_WORD_CHARS] = '\0';
}

static int run_wrap_oversized_word_alone(const bt_app_font_book *book) {
    const bt_theme *theme = bt_theme_table();
    float body = theme->font_body * theme->root_rem;
    char word[WRAP_WORD_CHARS + 1];
    oversized_word(word);
    char text[BT_MEMO_TEXT_MAX];
    snprintf(text, sizeof text, "tail %s head", word);
    if (!(memo_measure(book, word, body) > (float)WRAP_WORD_BUDGET)) {
        return 0;
    }
    char lines[BT_MEMO_WRAP_MAX][BT_MEMO_TEXT_MAX];
    size_t count = bt_memo_wrap_lines(book->regular, text, body, (float)WRAP_WORD_BUDGET, lines,
                                      BT_MEMO_WRAP_MAX, BT_MEMO_WRAP_MAX);
    if (count != 3 || strcmp(lines[0], "tail") != 0 || strcmp(lines[2], "head") != 0 ||
        strcmp(lines[1], word) != 0) {
        return 0;
    }
    char joined[BT_MEMO_TEXT_MAX];
    snprintf(joined, sizeof joined, "%s %s %s", lines[0], lines[1], lines[2]);
    if (!words_match(text, joined)) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        float fitted = bt_ui_fitted_width(book->regular, lines[i], body, (float)WRAP_WORD_BUDGET);
        if (fitted > (float)WRAP_WORD_BUDGET + 0.01F) {
            return 0;
        }
    }
    return 1;
}

BT_TEST(view_memo, wrap_emits_an_oversized_word_as_its_own_line) {
    BT_CHECK(with_memo_fonts(run_wrap_oversized_word_alone));
}

static int memo_status_ink_extent(const char *path, int y0, int y1, int *rightmost) {
    Image image = LoadImage(path);
    if (image.width <= 0 || image.height <= 0) {
        UnloadImage(image);
        return 0;
    }
    Color *pixels = LoadImageColors(image);
    Color surface = bt_app_color(bt_theme_background());
    int found = 0;
    int right = -1;
    for (int y = y0; y <= y1 && y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Color pixel = pixels[y * image.width + x];
            if (pixel.r != surface.r || pixel.g != surface.g || pixel.b != surface.b) {
                found = 1;
                if (x > right) {
                    right = x;
                }
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    *rightmost = right;
    return found;
}

static const bt_app_font_book *g_memo_book;

static void draw_memo_oversized_status(void) {
    bt_memo_rows rows;
    memset(&rows, 0, sizeof rows);
    char word[WRAP_WORD_CHARS + 1];
    oversized_word(word);
    snprintf(rows.focal, sizeof rows.focal, "%s", "...");
    snprintf(rows.status, sizeof rows.status, "%s", word);
    bt_view_memo_draw(&rows, g_memo_book->regular, g_memo_book->bold, (float)MEMO_DRAW_W,
                      (float)MEMO_DRAW_H);
}

static int run_status_oversized_word_clamped(const bt_app_font_book *book) {
    if (!bt_app_window_open_driven(MEMO_DRAW_W, MEMO_DRAW_H, "memo status clamp", NULL)) {
        return 0;
    }
    g_memo_book = book;
    for (int frame = 0; frame < MEMO_DWELL_FRAMES; frame++) {
        bt_app_window_frame_begin();
        ClearBackground(bt_app_color(bt_theme_background()));
        draw_memo_oversized_status();
        bt_app_window_frame_end();
    }
    char shot[BT_PATH_MAX];
    snprintf(shot, sizeof shot, "%s/app-memo-status-%d.png", getenv("TEMP"), _getpid());
    remove(shot);
    int ok = bt_app_window_capture(shot);
    int right = -1;
    const bt_theme *theme = bt_theme_table();
    float body = theme->font_body * theme->root_rem;
    int band_top = (int)(MEMO_DRAW_H - theme->panel_padding_rem * theme->root_rem - body) - 4;
    if (ok) {
        ok = memo_status_ink_extent(shot, band_top, MEMO_DRAW_H - 1, &right);
    }
    remove(shot);
    bt_app_window_close();
    float limit = (float)MEMO_DRAW_W - theme->panel_padding_rem * theme->root_rem;
    return ok && right >= 0 && (float)right <= limit + 2.0F;
}

BT_TEST(view_memo, status_draw_clamps_an_oversized_line_to_the_content_width) {
    BT_CHECK(with_memo_fonts(run_status_oversized_word_clamped));
}

static void residue_with_every_kind(bt_memo *out) {
    memset(out, 0, sizeof *out);
    out->residue.has_parity = true;
    out->residue.parity.edge[0] = 0;
    out->residue.parity.edge[1] = 1;
    out->residue.parity.corner[0] = 2;
    out->residue.parity.corner[1] = 3;
    out->residue.flip_count = 2;
    out->residue.flips[0] = 4;
    out->residue.flips[1] = 5;
    out->residue.twist_count = 1;
    out->residue.twists[0].corner = 6;
    out->residue.twists[0].quarter = 2;
}

BT_TEST(view_memo, residue_lines_report_parity_flips_and_twists) {
    bt_memo memo;
    residue_with_every_kind(&memo);
    char lines[BT_MEMO_RESIDUE_MAX][BT_MEMO_TEXT_MAX];
    size_t count = 0;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, BT_MEMO_RESIDUE_MAX, &count) == 1);
    BT_CHECK_EQ(count, (size_t)3);
    char parity[BT_MEMO_TEXT_MAX];
    snprintf(parity, sizeof parity, "parity %s-%s, %s-%s", bt_scheme_name_of(0),
             bt_scheme_name_of(1), bt_scheme_name_of(2), bt_scheme_name_of(3));
    BT_CHECK(strcmp(lines[0], parity) == 0);
    char flip[BT_MEMO_TEXT_MAX];
    snprintf(flip, sizeof flip, "flip %s & %s", bt_scheme_name_of(4), bt_scheme_name_of(5));
    BT_CHECK(strcmp(lines[1], flip) == 0);
    char twist[BT_MEMO_TEXT_MAX];
    snprintf(twist, sizeof twist, "twist %s x2", bt_scheme_name_of(6));
    BT_CHECK(strcmp(lines[2], twist) == 0);
}

BT_TEST(view_memo, residue_lines_fail_closed_when_the_capacity_runs_out) {
    bt_memo memo;
    residue_with_every_kind(&memo);
    char lines[BT_MEMO_RESIDUE_MAX][BT_MEMO_TEXT_MAX];
    size_t count = 99;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, 0, &count) == 0);
    BT_CHECK_EQ(count, (size_t)0);
    count = 99;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, 2, &count) == 0);
    BT_CHECK_EQ(count, (size_t)2);
    memo.residue.has_parity = false;
    count = 99;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, 0, &count) == 0);
    BT_CHECK_EQ(count, (size_t)0);
    memo.residue.flip_count = 0;
    memo.residue.twist_count = 0;
    count = 99;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, 0, &count) == 0);
    BT_CHECK_EQ(count, (size_t)0);
}

BT_TEST(view_memo, residue_lines_without_any_residue_say_so) {
    bt_memo memo;
    memset(&memo, 0, sizeof memo);
    char lines[BT_MEMO_RESIDUE_MAX][BT_MEMO_TEXT_MAX];
    size_t count = 0;
    BT_CHECK(bt_memo_residue_lines(&memo, lines, BT_MEMO_RESIDUE_MAX, &count) == 1);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(lines[0], "residue none") == 0);
}

BT_TEST(view_memo, a_failed_syllable_malloc_leaves_the_token_on_letters) {
    bt_syllable_book book = book_with("edge", "AB", "ta");
    bt_memo memo;
    memo_with_edge_pair('A', 'B', &memo);
    bt_memo_token tokens[BT_MEMO_TOKENS_MAX];
    size_t count = 0;
    bt_alloc_fail_arm(0);
    bt_memo_build_tokens(&memo, &book, tokens, &count);
    bt_alloc_fail_disarm();
    const bt_memo_token *ab = NULL;
    for (size_t i = 0; i < count; i++) {
        if (tokens[i].letters[0] == 'A' && tokens[i].letters[1] == 'B' &&
            tokens[i].orbit == BT_ORBIT_EDGE) {
            ab = &tokens[i];
        }
    }
    BT_CHECK(ab != NULL);
    BT_CHECK(ab->syllable == NULL);
    char text[BT_MEMO_TEXT_MAX];
    bt_memo_token_text(ab, BT_MEMO_SYLLABLES, text);
    BT_CHECK(strcmp(text, "AB") == 0);
    bt_syllable_book_free(&book);
}

BT_TEST(view_memo, rating_without_a_memo_writes_no_reviews) {
    bt_memo_session session;
    bt_memo_session_init(&session);
    bt_stores stores = empty_stores();
    bt_fsrs_config fsrs;
    bt_fsrs_config_defaults(&fsrs);
    bt_memo_rate_pairs(&stores, &fsrs, &session, BT_GRADE_GOOD, 100);
    BT_CHECK_EQ(bt_srs_store_len(&stores.srs), (size_t)0);
    bt_stores_free(&stores);
}

BT_TEST(view_memo, the_recent_trail_starts_after_the_recent_window) {
    bt_memo_session session;
    bt_memo_session_init(&session);
    enum { TRAIL_TOKENS = 20 };
    session.token_count = TRAIL_TOKENS;
    for (size_t i = 0; i < TRAIL_TOKENS; i++) {
        session.tokens[i].orbit = BT_ORBIT_EDGE;
        session.tokens[i].letters[0] = (char)('A' + i / 10);
        session.tokens[i].letters[1] = (char)('A' + i % 10);
        session.tokens[i].syllable = NULL;
        session.tokens[i].image = NULL;
    }
    session.shown = TRAIL_TOKENS;
    bt_stores stores = empty_stores();
    bt_memo_rows rows;
    bt_memo_view_rows(BT_MEMO_STREAM, &session, &stores, &rows);
    char expected[BT_MEMO_RECENT_MAX];
    expected[0] = '\0';
    for (size_t i = TRAIL_TOKENS - 1 - BT_MEMO_RECENT_TOKENS; i + 1 < TRAIL_TOKENS; i++) {
        char text[BT_MEMO_TEXT_MAX];
        bt_memo_token_text(&session.tokens[i], BT_MEMO_PER_ORBIT, text);
        if (expected[0] != '\0') {
            strcat(expected, " ");
        }
        strcat(expected, text);
    }
    BT_CHECK(strcmp(rows.recent, expected) == 0);
    bt_stores_free(&stores);
}

BT_TEST(view_memo, a_zero_cadence_completes_the_stream_on_the_next_tick) {
    bt_app_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.stores.settings.memo_cadence_ms = 0;
    ctx.memo_phase = BT_MEMO_STREAM;
    bt_memo_app_state app;
    bt_view_memo_app_init(&app);
    app.session.token_count = 3;
    BT_CHECK_EQ(app.session.shown, (size_t)0);
    bt_view_memo_tick(&ctx, &app, 0.016F);
    BT_CHECK_EQ(app.session.shown, (size_t)3);
    BT_CHECK(ctx.memo_phase == BT_MEMO_RECALL);
    bt_view_memo_app_free(&app);
}

BT_TEST(view_memo, row_draw_without_loaded_fonts_returns_before_touching_stores) {
    BT_CHECK(bt_app_fonts_current() == NULL);
    bt_app_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    bt_view_memo_row_draw(&ctx);
}

static int run_row_draw_without_stores(const bt_app_font_book *book) {
    (void)book;
    bt_app_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    bt_view_memo_row_draw(&ctx);
    return 1;
}

BT_TEST(view_memo, row_draw_without_stores_returns_before_building_rows) {
    BT_CHECK(with_memo_fonts(run_row_draw_without_stores));
}

enum {
    WRAP_OVERLONG_CHARS = BT_MEMO_TEXT_MAX + 2,
    WRAP_OVERLONG_WIDTH = 2000,
};

static int run_wrap_overlong_word(const bt_app_font_book *book) {
    const bt_theme *theme = bt_theme_table();
    float body = theme->font_body * theme->root_rem;
    char word[WRAP_OVERLONG_CHARS + 1];
    memset(word, 'a', WRAP_OVERLONG_CHARS);
    word[WRAP_OVERLONG_CHARS] = '\0';
    char text[BT_MEMO_TEXT_MAX * 2];
    snprintf(text, sizeof text, "%s tail", word);
    char lines[BT_MEMO_WRAP_MAX][BT_MEMO_TEXT_MAX];
    size_t count =
        bt_memo_wrap_lines(book->regular, text, body, (float)WRAP_OVERLONG_WIDTH, lines, 2, 1);
    if (count != 1) {
        return 0;
    }
    size_t len = strlen(lines[0]);
    if (len < 4 || len > BT_MEMO_TEXT_MAX - 1 || strcmp(lines[0] + len - 3, "...") != 0) {
        return 0;
    }
    if (memo_measure(book, lines[0], body) > (float)WRAP_OVERLONG_WIDTH + 0.5F) {
        return 0;
    }
    size_t tight =
        bt_memo_wrap_lines(book->regular, "ready", body, (float)WRAP_OVERLONG_WIDTH, lines, 1, 4);
    return tight == 1 && strcmp(lines[0], "ready") == 0;
}

BT_TEST(view_memo, wrap_clamps_a_word_longer_than_the_line_buffer) {
    BT_CHECK(with_memo_fonts(run_wrap_overlong_word));
}
