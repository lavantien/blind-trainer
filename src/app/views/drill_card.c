#include "app/views/drill_card.h"

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "data/commutator.h"
#include "data/verify.h"
#include "scheme/scheme.h"

#include <stdio.h>
#include <string.h>

void bt_row_style_metrics(bt_row_style style, float *size_rem, bt_theme_color *color) {
    BT_TRACE_ENTER("app.drill_row_style_metrics");
    const bt_theme *theme = bt_theme_table();
    switch (style) {
    case BT_ROW_FOCAL:
        *size_rem = theme->font_focal;
        *color = bt_theme_text_primary();
        break;
    case BT_ROW_MUTED:
        *size_rem = theme->font_body;
        *color = bt_theme_text_muted();
        break;
    case BT_ROW_HEADING:
        *size_rem = theme->font_heading;
        *color = bt_theme_text_primary();
        break;
    case BT_ROW_ALG:
        *size_rem = theme->font_alg;
        *color = bt_theme_text_primary();
        break;
    case BT_ROW_BODY:
        *size_rem = theme->font_body;
        *color = bt_theme_text_primary();
        break;
    case BT_ROW_SMALL:
        *size_rem = theme->font_small;
        *color = bt_theme_text_muted();
        break;
    case BT_ROW_MICRO:
        *size_rem = theme->font_micro;
        *color = bt_theme_text_muted();
        break;
    default:
        *size_rem = theme->font_body;
        *color = bt_theme_text_primary();
        break;
    }
    BT_TRACE_EXIT("app.drill_row_style_metrics");
}

static int alg_copy(const bt_alg *src, bt_alg *dst) {
    bt_alg_init(dst);
    for (size_t i = 0; i < bt_alg_len(src); i++) {
        if (!bt_alg_push(dst, src->items[i])) {
            bt_alg_free(dst);
            return 0;
        }
    }
    return 1;
}

static void alg_text_into(const bt_alg *alg, char out[BT_CARD_TEXT_MAX]) {
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    bt_alg_format(alg, &sb);
    snprintf(out, BT_CARD_TEXT_MAX, "%s", sb.data != NULL ? sb.data : "");
    bt_strbuf_free(&sb);
}

static void side_from_row(const bt_case *case_id, const bt_corpus_row *row, bt_drill_side *side) {
    bt_drill_side_init(side);
    side->case_id = *case_id;
    bt_view_case_position(case_id, side->position);
    alg_copy(&row->alg, &side->alg);
    snprintf(side->alg_text, sizeof side->alg_text, "%s", row->alg_text);
    snprintf(side->commutator, sizeof side->commutator, "%s", row->commutator);
    snprintf(side->solver_label, sizeof side->solver_label, "%s", bt_solver_dir(row->solver));
    bt_verdict verdict;
    bt_verdict_init(&verdict);
    bt_verify(&row->alg, row->position, row->sheet, &verdict);
    side->verified = bt_verdict_ok(&verdict) ? 1 : 0;
    bt_verdict_free(&verdict);
    side->vendor = row->solver == BT_SOLVER_USER ? 0 : 1;
}

static void inverse_side(const bt_drill_side *a, bt_drill_side *side) {
    bt_drill_side_init(side);
    bt_case flipped = bt_view_flip_case(&a->case_id);
    side->case_id = flipped;
    bt_view_case_position(&flipped, side->position);
    bt_alg inverse = bt_alg_inverse(&a->alg);
    alg_copy(&inverse, &side->alg);
    bt_alg_free(&inverse);
    alg_text_into(&side->alg, side->alg_text);
    side->commutator[0] = '\0';
    snprintf(side->solver_label, sizeof side->solver_label, "%s", "inverse");
    side->verified = 1;
    side->vendor = 0;
}

int bt_view_build_sides(const bt_corpus *corpus, const bt_case *case_id, bt_drill_side *side_a,
                        bt_drill_side *side_b) {
    BT_TRACE_ENTER("app.drill_build_sides");
    bt_sheet_class sheet = bt_view_sheet_of(case_id->orbit);
    char position[BT_CARD_TEXT_MAX];
    bt_view_case_position(case_id, position);
    size_t rows[BT_CORPUS_INDEX_MAX];
    size_t count = bt_corpus_rows_for_position(corpus, sheet, position, rows, BT_CORPUS_INDEX_MAX);
    if (count == 0) {
        BT_TRACE_EXIT("app.drill_build_sides");
        return 0;
    }
    side_from_row(case_id, &corpus->rows[rows[0]], side_a);
    bt_case flipped = bt_view_flip_case(case_id);
    char flip_position[BT_CARD_TEXT_MAX];
    bt_view_case_position(&flipped, flip_position);
    size_t flip_rows[BT_CORPUS_INDEX_MAX];
    size_t flip_count =
        bt_corpus_rows_for_position(corpus, sheet, flip_position, flip_rows, BT_CORPUS_INDEX_MAX);
    if (flip_count > 0) {
        side_from_row(&flipped, &corpus->rows[flip_rows[0]], side_b);
    } else {
        inverse_side(side_a, side_b);
    }
    bt_cube check = bt_cube_solved();
    bt_cube_apply_alg(&check, &side_a->alg);
    bt_cube_apply_alg(&check, &side_b->alg);
    if (!bt_cube_is_solved(&check)) {
        bt_drill_side_free(side_b);
        inverse_side(side_a, side_b);
    }
    BT_TRACE_EXIT("app.drill_build_sides");
    return 1;
}

void bt_view_memo_token(const bt_syllable_book *book, const bt_case *case_id,
                        char out[BT_CARD_TEXT_MAX]) {
    BT_TRACE_ENTER("app.drill_memo_token");
    char letters[2];
    bt_scheme_case_letters(case_id, letters);
    char pair[3] = {letters[0], letters[1], '\0'};
    const char *token = NULL;
    if (case_id->orbit == BT_ORBIT_EDGE) {
        token = bt_syllable_book_for(book, "edge", letters);
    } else {
        token = bt_views_image_for(letters);
    }
    if (token == NULL) {
        token = pair;
    }
    snprintf(out, BT_CARD_TEXT_MAX, "%s", token);
    BT_TRACE_EXIT("app.drill_memo_token");
}

static int push_span(bt_card_content *content, const char *text, bt_theme_color color) {
    if (content->span_count >= BT_CARD_SPANS_MAX) {
        BT_TRACE_FAIL("app.drill_mechanic_spans", "span overflow at %zu", content->span_count);
        return 0;
    }
    bt_card_span *span = &content->spans[content->span_count++];
    snprintf(span->text, sizeof span->text, "%s", text);
    span->size_rem = bt_theme_table()->font_alg;
    span->color = color;
    return 1;
}

int bt_view_mechanic_spans(const char *commutator, const bt_alg *alg, bt_card_content *out) {
    BT_TRACE_ENTER("app.drill_mechanic_spans");
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    if (!bt_decompose_verified(commutator, alg, &units)) {
        bt_expanded_list_free(&units);
        BT_TRACE_EXIT("app.drill_mechanic_spans");
        return 0;
    }
    for (size_t i = 0; i < units.count; i++) {
        const bt_expanded *unit = &units.items[i];
        if (i > 0 && !push_span(out, " + ", bt_theme_text_primary())) {
            bt_expanded_list_free(&units);
            BT_TRACE_FAIL("app.drill_mechanic_spans", "out of memory");
            BT_TRACE_EXIT("app.drill_mechanic_spans");
            return 0;
        }
        if (unit->has_setup) {
            char setup[BT_CARD_TEXT_MAX];
            alg_text_into(&unit->setup, setup);
            char label[BT_CARD_TEXT_MAX];
            snprintf(label, sizeof label, "%s: ", setup);
            if (!push_span(out, label, bt_theme_setup())) {
                bt_expanded_list_free(&units);
                BT_TRACE_EXIT("app.drill_mechanic_spans");
                return 0;
            }
        }
        if (!push_span(out, "[", bt_theme_text_primary())) {
            bt_expanded_list_free(&units);
            BT_TRACE_EXIT("app.drill_mechanic_spans");
            return 0;
        }
        char a_text[BT_CARD_TEXT_MAX];
        char b_text[BT_CARD_TEXT_MAX];
        alg_text_into(&unit->a, a_text);
        alg_text_into(&unit->b, b_text);
        int ok = 1;
        if (unit->b_first) {
            ok = ok && push_span(out, b_text, bt_theme_interchange());
            ok = ok && push_span(out, ", ", bt_theme_text_primary());
            ok = ok && push_span(out, a_text, bt_theme_teal());
        } else {
            ok = ok && push_span(out, a_text, bt_theme_teal());
            ok = ok && push_span(out, ", ", bt_theme_text_primary());
            ok = ok && push_span(out, b_text, bt_theme_interchange());
        }
        ok = ok && push_span(out, "]", bt_theme_text_primary());
        if (!ok) {
            bt_expanded_list_free(&units);
            BT_TRACE_EXIT("app.drill_mechanic_spans");
            return 0;
        }
    }
    bt_expanded_list_free(&units);
    out->has_mechanics = 1;
    BT_TRACE_EXIT("app.drill_mechanic_spans");
    return 1;
}

static const char *due_label(const bt_stores *stores, bt_sheet_class sheet, const char *position,
                             long long now_day) {
    char key[BT_CARD_TEXT_MAX];
    bt_view_card_key(sheet, position, key);
    const bt_card *card = bt_srs_store_get(&stores->srs, key);
    if (card == NULL) {
        return "new";
    }
    return bt_card_is_due(card, now_day) ? "due" : "ok";
}

static int push_row(bt_card_content *content, const char *text, bt_row_style style) {
    if (content->row_count >= BT_CARD_ROWS_MAX) {
        BT_TRACE_FAIL("app.drill_card_content", "row overflow at %zu", content->row_count);
        return 0;
    }
    bt_card_row *row = &content->rows[content->row_count++];
    snprintf(row->text, sizeof row->text, "%s", text);
    row->style = style;
    return 1;
}

void bt_view_card_content(const bt_drill_session *session, const bt_corpus *corpus,
                          const bt_syllable_book *book, const bt_stores *stores, long long now_day,
                          bt_card_content *out) {
    BT_TRACE_ENTER("app.drill_card_content");
    memset(out, 0, sizeof *out);
    const bt_drill_side *side_b = NULL;
    const bt_drill_side *a = bt_drill_session_sides(session, &side_b);
    if (a == NULL) {
        char hint[BT_CARD_TEXT_MAX];
        if (session->has_pin) {
            char pin_letters[3];
            char letters[2];
            bt_scheme_case_letters(&session->pin, letters);
            pin_letters[0] = letters[0];
            pin_letters[1] = letters[1];
            pin_letters[2] = '\0';
            snprintf(hint, sizeof hint, "pair %s uncovered, esc menu", pin_letters);
        } else {
            snprintf(hint, sizeof hint, "%s", "no covered pairs, esc menu");
        }
        push_row(out, hint, BT_ROW_MUTED);
        BT_TRACE_EXIT("app.drill_card_content");
        return;
    }
    bt_sheet_class sheet = bt_view_sheet_of(a->case_id.orbit);
    int has_mechanics = bt_view_mechanic_spans(a->commutator, &a->alg, out);
    char letters[3];
    bt_drill_side_letters(a, letters);
    char line[BT_CARD_TEXT_MAX];
    push_row(out, letters, BT_ROW_FOCAL);
    snprintf(line, sizeof line, "%s %s", a->case_id.orbit == BT_ORBIT_EDGE ? "edge" : "corner",
             a->position);
    push_row(out, line, BT_ROW_MUTED);
    char token[BT_CARD_TEXT_MAX];
    bt_view_memo_token(book, &a->case_id, token);
    snprintf(line, sizeof line, "memo %s", token);
    push_row(out, line, BT_ROW_HEADING);
    push_row(out, a->alg_text, BT_ROW_ALG);
    if (!has_mechanics && bt_view_has_commutator(a->commutator)) {
        snprintf(line, sizeof line, "commutator %s", a->commutator);
        push_row(out, line, BT_ROW_SMALL);
    }
    char expand[BT_CARD_TEXT_MAX];
    if (bt_view_expand_alg_line(a->commutator, &a->alg, a->alg_text, a->alg_text, expand)) {
        push_row(out, expand, BT_ROW_SMALL);
    }
    char b_letters[3];
    bt_drill_side_letters(side_b, b_letters);
    snprintf(line, sizeof line, "prepare %s %s: %s", b_letters, side_b->solver_label,
             side_b->alg_text);
    push_row(out, line, BT_ROW_BODY);
    snprintf(line, sizeof line, "%u qtm %u htm %s %s", bt_alg_qtm(&a->alg), bt_alg_htm(&a->alg),
             a->solver_label, a->verified ? "verified" : "unverified");
    push_row(out, line, BT_ROW_SMALL);
    snprintf(line, sizeof line, "srs %s %s, %s %s", letters,
             due_label(stores, sheet, a->position, now_day), b_letters,
             due_label(stores, sheet, side_b->position, now_day));
    push_row(out, line, BT_ROW_SMALL);
    size_t rows[BT_CORPUS_INDEX_MAX];
    size_t count =
        bt_corpus_rows_for_position(corpus, sheet, a->position, rows, BT_CORPUS_INDEX_MAX);
    for (size_t i = 1; i < count; i++) {
        const bt_corpus_row *row = &corpus->rows[rows[i]];
        snprintf(line, sizeof line, "alt %s %s", bt_solver_dir(row->solver), row->alg_text);
        if (!push_row(out, line, BT_ROW_MICRO)) {
            break;
        }
    }
    BT_TRACE_EXIT("app.drill_card_content");
}

void bt_view_reps_lines(const bt_drill_session *session, char out[][BT_CARD_TEXT_MAX], size_t cap,
                        size_t *count) {
    BT_TRACE_ENTER("app.drill_reps_lines");
    *count = 0;
    const bt_drill_side *side = bt_drill_session_current_side(session);
    if (side == NULL) {
        if (cap > 0) {
            snprintf(out[0], BT_CARD_TEXT_MAX, "%s", "no pair");
            *count = 1;
        }
        BT_TRACE_EXIT("app.drill_reps_lines");
        return;
    }
    if (cap < 3) {
        BT_TRACE_FAIL("app.drill_reps_lines", "cap %zu too small", cap);
        BT_TRACE_EXIT("app.drill_reps_lines");
        return;
    }
    char letters[3];
    bt_drill_side_letters(side, letters);
    snprintf(out[0], BT_CARD_TEXT_MAX, "rep %u of %u", session->reps_done, session->reps_target);
    snprintf(out[1], BT_CARD_TEXT_MAX, "%s %s", letters, side->alg_text);
    snprintf(out[2], BT_CARD_TEXT_MAX, "%us", (unsigned int)session->reps_clock);
    *count = 3;
    BT_TRACE_EXIT("app.drill_reps_lines");
}

void bt_view_rate_lines(const bt_drill_session *session, char out[][BT_CARD_TEXT_MAX], size_t cap,
                        size_t *count) {
    BT_TRACE_ENTER("app.drill_rate_lines");
    *count = 0;
    const bt_drill_side *side_b = NULL;
    const bt_drill_side *a = bt_drill_session_sides(session, &side_b);
    if (a == NULL) {
        if (cap > 0) {
            snprintf(out[0], BT_CARD_TEXT_MAX, "%s", "no pair");
            *count = 1;
        }
        BT_TRACE_EXIT("app.drill_rate_lines");
        return;
    }
    if (cap < 3) {
        BT_TRACE_FAIL("app.drill_rate_lines", "cap %zu too small", cap);
        BT_TRACE_EXIT("app.drill_rate_lines");
        return;
    }
    const bt_drill_side *side = session->rate_step == 0 ? a : side_b;
    float total = 0.0F;
    for (size_t i = 0; i < session->split_count; i++) {
        total += session->splits[i].secs;
    }
    float avg = session->split_count == 0 ? 0.0F : total / (float)session->split_count;
    char letters[3];
    bt_drill_side_letters(side, letters);
    snprintf(out[0], BT_CARD_TEXT_MAX, "rate %s", letters);
    snprintf(out[1], BT_CARD_TEXT_MAX, "%zu reps avg %.1fs", session->split_count, avg);
    snprintf(out[2], BT_CARD_TEXT_MAX, "%s", "1 again 2 hard 3 good 4 easy");
    *count = 3;
    BT_TRACE_EXIT("app.drill_rate_lines");
}
