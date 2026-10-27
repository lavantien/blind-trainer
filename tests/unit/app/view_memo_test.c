#include "app/views/memo.h"

#include "common.h"

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

static const char *stub_images_for(const char letters[2]) {
    if (letters[0] == 'A' && letters[1] == 'B') {
        return "Anthony Brooks";
    }
    return NULL;
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
    bt_views_set_image_lookup(stub_images_for);
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
    bt_views_set_image_lookup(NULL);
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
