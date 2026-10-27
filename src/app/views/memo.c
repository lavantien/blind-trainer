#include "app/views/memo.h"

#include "app/views/drill.h"

#include "common/bt_trace.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_memo_session_init(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_session_init");
    memset(session, 0, sizeof *session);
    session->scramble = bt_cube_solved();
    BT_TRACE_EXIT("app.memo_session_init");
}

void bt_memo_session_free(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_session_free");
    for (size_t i = 0; i < session->token_count; i++) {
        free(session->tokens[i].syllable);
        session->tokens[i].syllable = NULL;
    }
    session->token_count = 0;
    BT_TRACE_EXIT("app.memo_session_free");
}

float bt_memo_cadence_secs(const bt_memo_session *session, const bt_stores *stores) {
    BT_TRACE_ENTER("app.memo_cadence_secs");
    (void)session;
    float secs = (float)stores->settings.memo_cadence_ms / 1000.0F;
    BT_TRACE_EXIT("app.memo_cadence_secs");
    return secs;
}

void bt_memo_reset_stream(bt_memo_session *session) {
    BT_TRACE_ENTER("app.memo_reset_stream");
    session->shown = 0;
    session->clock = 0.0F;
    BT_TRACE_EXIT("app.memo_reset_stream");
}

void bt_memo_build_tokens(const bt_memo *memo, const bt_syllable_book *book, bt_memo_token *out,
                          size_t *count) {
    BT_TRACE_ENTER("app.memo_build_tokens");
    *count = 0;
    for (int orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? memo->edge_pairs : memo->corner_pairs;
        size_t pair_count = orbit == 0 ? memo->edge_pair_count : memo->corner_pair_count;
        for (size_t i = 0; i < pair_count; i++) {
            bt_memo_token *token = &out[(*count)++];
            token->orbit = pairs[i].orbit;
            bt_scheme_case_letters(&pairs[i], token->letters);
            token->syllable = NULL;
            const char *syllable = bt_syllable_book_for(
                book, token->orbit == BT_ORBIT_EDGE ? "edge" : "corner", token->letters);
            if (syllable != NULL) {
                token->syllable = _strdup(syllable);
                if (token->syllable == NULL) {
                    BT_TRACE_FAIL("app.memo_build_tokens", "out of memory");
                }
            }
            token->image = bt_views_image_for(token->letters);
        }
    }
    BT_TRACE_EXIT("app.memo_build_tokens");
}

void bt_memo_token_text(const bt_memo_token *token, bt_memo_mode mode, char out[BT_MEMO_TEXT_MAX]) {
    BT_TRACE_ENTER("app.memo_token_text");
    char pair[3] = {token->letters[0], token->letters[1], '\0'};
    const char *text = pair;
    int use_syllable = mode == BT_MEMO_SYLLABLES;
    int use_image = mode == BT_MEMO_IMAGES;
    if (mode == BT_MEMO_PER_ORBIT) {
        use_syllable = token->orbit == BT_ORBIT_EDGE;
        use_image = token->orbit == BT_ORBIT_CORNER;
    }
    if (use_syllable && token->syllable != NULL) {
        text = token->syllable;
    }
    if (use_image && token->image != NULL) {
        text = token->image;
    }
    snprintf(out, BT_MEMO_TEXT_MAX, "%s", text);
    BT_TRACE_EXIT("app.memo_token_text");
}

void bt_memo_rebuild(bt_memo_session *session, uint64_t seed, const bt_syllable_book *book) {
    BT_TRACE_ENTER("app.memo_rebuild");
    bt_memo_session_free(session);
    bt_cube scramble;
    bt_scheme_random_state(seed, &scramble);
    bt_memo memo;
    bt_scheme_memo(&scramble, &memo);
    bt_memo_build_tokens(&memo, book, session->tokens, &session->token_count);
    session->memo = memo;
    session->has_memo = 1;
    session->scramble = scramble;
    session->seed = seed;
    bt_memo_reset_stream(session);
    BT_TRACE_EXIT("app.memo_rebuild");
}

void bt_memo_rate_pairs(bt_stores *stores, const bt_fsrs_config *fsrs,
                        const bt_memo_session *session, bt_grade rating, long long now_day) {
    BT_TRACE_ENTER("app.memo_rate_pairs");
    if (!session->has_memo) {
        BT_TRACE_EXIT("app.memo_rate_pairs");
        return;
    }
    for (int orbit = 0; orbit < 2; orbit++) {
        const bt_case *pairs = orbit == 0 ? session->memo.edge_pairs : session->memo.corner_pairs;
        size_t pair_count =
            orbit == 0 ? session->memo.edge_pair_count : session->memo.corner_pair_count;
        for (size_t i = 0; i < pair_count; i++) {
            char position[BT_MEMO_TEXT_MAX];
            bt_view_case_position(&pairs[i], position);
            bt_view_write_review(stores, fsrs, bt_view_sheet_of(pairs[i].orbit), position, rating,
                                 now_day);
        }
    }
    BT_TRACE_EXIT("app.memo_rate_pairs");
}

int bt_memo_residue_lines(const bt_memo *memo, char out[][BT_MEMO_TEXT_MAX], size_t cap,
                          size_t *count) {
    BT_TRACE_ENTER("app.memo_residue_lines");
    *count = 0;
    if (memo->residue.has_parity) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "parity %s-%s, %s-%s",
                 bt_scheme_name_of(memo->residue.parity.edge[0]),
                 bt_scheme_name_of(memo->residue.parity.edge[1]),
                 bt_scheme_name_of(memo->residue.parity.corner[0]),
                 bt_scheme_name_of(memo->residue.parity.corner[1]));
    }
    for (size_t i = 0; i + 1 < memo->residue.flip_count; i += 2) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "flip %s & %s",
                 bt_scheme_name_of(memo->residue.flips[i]),
                 bt_scheme_name_of(memo->residue.flips[i + 1]));
    }
    for (size_t i = 0; i < memo->residue.twist_count; i++) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "twist %s x%u",
                 bt_scheme_name_of(memo->residue.twists[i].corner),
                 (unsigned)memo->residue.twists[i].quarter);
    }
    if (*count == 0) {
        if (*count >= cap) {
            BT_TRACE_FAIL("app.memo_residue_lines", "cap %zu too small", cap);
            BT_TRACE_EXIT("app.memo_residue_lines");
            return 0;
        }
        snprintf(out[(*count)++], BT_MEMO_TEXT_MAX, "%s", "residue none");
    }
    BT_TRACE_EXIT("app.memo_residue_lines");
    return 1;
}

static void append_joined(char *out, size_t cap, const bt_memo_token *tokens, size_t count,
                          bt_memo_mode mode) {
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        char text[BT_MEMO_TEXT_MAX];
        bt_memo_token_text(&tokens[i], mode, text);
        if (i > 0) {
            strncat(out, " ", cap - strlen(out) - 1);
        }
        strncat(out, text, cap - strlen(out) - 1);
    }
}

static void trailing_before_current(char *out, size_t cap, const bt_memo_session *session,
                                    bt_memo_mode mode) {
    size_t end = session->shown > 0 ? session->shown - 1 : 0;
    size_t start = end > BT_MEMO_RECENT_TOKENS ? end - BT_MEMO_RECENT_TOKENS : 0;
    append_joined(out, cap, session->tokens + start, end - start, mode);
}

static void review_status(char *out, size_t cap, const bt_memo_session *session) {
    out[0] = '\0';
    if (session->has_memo) {
        char lines[BT_MEMO_RESIDUE_MAX][BT_MEMO_TEXT_MAX];
        size_t count = 0;
        bt_memo_residue_lines(&session->memo, lines, BT_MEMO_RESIDUE_MAX, &count);
        for (size_t i = 0; i < count; i++) {
            if (out[0] != '\0') {
                strncat(out, "; ", cap - strlen(out) - 1);
            }
            strncat(out, lines[i], cap - strlen(out) - 1);
        }
    }
    if (out[0] != '\0') {
        strncat(out, "; ", cap - strlen(out) - 1);
    }
    strncat(out, "r replay  space next  1..4 rate pairs", cap - strlen(out) - 1);
}

void bt_memo_view_rows(bt_memo_phase phase, const bt_memo_session *session, const bt_stores *stores,
                       bt_memo_rows *out) {
    BT_TRACE_ENTER("app.memo_view_rows");
    bt_memo_mode mode = stores->settings.memo_mode;
    unsigned cadence = stores->settings.memo_cadence_ms;
    if (phase == BT_MEMO_INTRO) {
        snprintf(out->focal, sizeof out->focal, "%s", "space to stream");
        out->recent[0] = '\0';
        snprintf(out->status, sizeof out->status, "seed %llu  %zu pairs  mode %s  cadence %u ms",
                 (unsigned long long)session->seed, session->token_count, bt_memo_mode_label(mode),
                 cadence);
    } else if (phase == BT_MEMO_STREAM) {
        if (session->shown > 0) {
            bt_memo_token_text(&session->tokens[session->shown - 1], mode, out->focal);
        } else {
            snprintf(out->focal, sizeof out->focal, "%s", "...");
        }
        trailing_before_current(out->recent, sizeof out->recent, session, mode);
        snprintf(out->status, sizeof out->status, "stream %zu/%zu  cadence %u ms", session->shown,
                 session->token_count, cadence);
    } else if (phase == BT_MEMO_RECALL) {
        snprintf(out->focal, sizeof out->focal, "%s", "recall");
        out->recent[0] = '\0';
        snprintf(out->status, sizeof out->status, "%zu/%zu streamed  space to reveal",
                 session->shown, session->token_count);
    } else {
        snprintf(out->focal, sizeof out->focal, "%s", "rate 1..4");
        append_joined(out->recent, sizeof out->recent, session->tokens, session->token_count, mode);
        review_status(out->status, sizeof out->status, session);
    }
    BT_TRACE_EXIT("app.memo_view_rows");
}
