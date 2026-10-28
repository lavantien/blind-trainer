#include "app/views/algs/mod.h"

#include "app/boot.h"
#include "app/views/algs/browse.h"
#include "app/views/algs/detail.h"
#include "app/views/case_drill.h"
#include "app/views/drill.h"
#include "app/views/frame.h"
#include "app/window.h"
#include "common/space.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float BT_ALGS_LINE_FACTOR = 1.5F;

bt_view_list_span bt_view_list_window(size_t total, size_t selected, size_t capacity) {
    BT_TRACE_ENTER("app.algs_list_window");
    bt_view_list_span window = {0, 0};
    size_t clamped = capacity;
    if (clamped > total) {
        clamped = total;
    }
    if (clamped < 1) {
        clamped = 1;
    }
    size_t start = selected >= clamped / 2 ? selected - clamped / 2 : 0;
    size_t last_start = total >= clamped ? total - clamped : 0;
    if (start > last_start) {
        start = last_start;
    }
    window.first = start;
    window.count = clamped;
    BT_TRACE_EXIT("app.algs_list_window");
    return window;
}

size_t bt_view_algs_all_cases(bt_case out[BT_ALGS_CASES_MAX]) {
    BT_TRACE_ENTER("app.algs_all_cases");
    size_t count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, out);
    count += bt_scheme_enumerate_cases(BT_ORBIT_CORNER, out + count);
    BT_TRACE_EXIT("app.algs_all_cases");
    return count;
}

void bt_view_algs_letters_of(const bt_case *case_id, char out[3]) {
    BT_TRACE_ENTER("app.algs_letters_of");
    char letters[2];
    bt_scheme_case_letters(case_id, letters);
    out[0] = letters[0];
    out[1] = letters[1];
    out[2] = '\0';
    BT_TRACE_EXIT("app.algs_letters_of");
}

const char *bt_view_algs_orbit_word(bt_orbit orbit) {
    BT_TRACE_ENTER("app.algs_orbit_word");
    const char *word = orbit == BT_ORBIT_EDGE ? "edge" : "corner";
    BT_TRACE_EXIT("app.algs_orbit_word");
    return word;
}

static size_t trimmed_start(const char *text, size_t len) {
    size_t at = 0;
    while (at < len) {
        size_t run = bt_space_len(text, at, len);
        if (run == 0) {
            break;
        }
        at += run;
    }
    return at;
}

static size_t trimmed_end(const char *text, size_t len) {
    size_t at = len;
    while (at > 0) {
        size_t run = bt_space_len(text, at - 1, len);
        if (run == 0) {
            break;
        }
        at -= run;
    }
    return at;
}

size_t bt_view_algs_filter_cases(const bt_case *all, size_t count, const char *query,
                                 bt_case out[BT_ALGS_CASES_MAX]) {
    BT_TRACE_ENTER("app.algs_filter_cases");
    size_t len = strlen(query);
    size_t start = trimmed_start(query, len);
    size_t end = trimmed_end(query, len);
    char trimmed[BT_ALGS_QUERY_MAX];
    size_t trimmed_len = 0;
    for (size_t at = start; at < end && trimmed_len + 1 < sizeof trimmed; at++) {
        char c = query[at];
        trimmed[trimmed_len++] = (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    trimmed[trimmed_len] = '\0';
    size_t kept = 0;
    if (trimmed_len == 0) {
        kept = count < BT_ALGS_CASES_MAX ? count : BT_ALGS_CASES_MAX;
        memcpy(out, all, kept * sizeof *out);
        BT_TRACE_EXIT("app.algs_filter_cases");
        return kept;
    }
    for (size_t i = 0; i < count && kept < BT_ALGS_CASES_MAX; i++) {
        char letters[3];
        bt_view_algs_letters_of(&all[i], letters);
        if (strncmp(letters, trimmed, trimmed_len) == 0) {
            out[kept++] = all[i];
        }
    }
    BT_TRACE_EXIT("app.algs_filter_cases");
    return kept;
}

size_t bt_view_algs_case_rows(const bt_corpus *corpus, const bt_case *case_id,
                              const bt_corpus_row *out[BT_ALGS_ROWS_MAX]) {
    BT_TRACE_ENTER("app.algs_case_rows");
    char position[BT_ALGS_TEXT_MAX];
    bt_view_case_position(case_id, position);
    bt_sheet_class sheet = bt_view_sheet_of(case_id->orbit);
    size_t count = 0;
    for (size_t i = 0; i < corpus->count && count < BT_ALGS_ROWS_MAX; i++) {
        if (corpus->rows[i].sheet == sheet && strcmp(corpus->rows[i].position, position) == 0) {
            out[count++] = &corpus->rows[i];
        }
    }
    BT_TRACE_EXIT("app.algs_case_rows");
    return count;
}

size_t bt_view_algs_case_rows_dyn(const bt_corpus *corpus, const bt_case *case_id,
                                  const bt_corpus_row ***out) {
    BT_TRACE_ENTER("app.algs_case_rows_dyn");
    char position[BT_ALGS_TEXT_MAX];
    bt_view_case_position(case_id, position);
    bt_sheet_class sheet = bt_view_sheet_of(case_id->orbit);
    const bt_corpus_row **rows = NULL;
    size_t count = 0;
    size_t cap = 0;
    for (size_t i = 0; i < corpus->count; i++) {
        if (corpus->rows[i].sheet == sheet && strcmp(corpus->rows[i].position, position) == 0) {
            if (count == cap) {
                size_t grown_cap = cap == 0 ? 16 : cap * 2;
                const bt_corpus_row **grown = realloc(rows, grown_cap * sizeof *grown);
                if (grown == NULL) {
                    free(rows);
                    BT_TRACE_FAIL("app.algs_case_rows_dyn", "out of memory");
                    BT_TRACE_EXIT("app.algs_case_rows_dyn");
                    *out = NULL;
                    return 0;
                }
                rows = grown;
                cap = grown_cap;
            }
            rows[count++] = &corpus->rows[i];
        }
    }
    BT_TRACE_EXIT("app.algs_case_rows_dyn");
    *out = rows;
    return count;
}

int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id) {
    BT_TRACE_ENTER("app.algs_covered");
    const bt_corpus_row *rows[BT_ALGS_ROWS_MAX];
    int covered = bt_view_algs_case_rows(corpus, case_id, rows) > 0;
    BT_TRACE_EXIT("app.algs_covered");
    return covered;
}

size_t bt_view_algs_list_capacity(const bt_app_config *config) {
    BT_TRACE_ENTER("app.algs_list_capacity");
    const bt_theme *theme = bt_theme_table();
    float chrome = theme->root_rem * (theme->header_height_rem + 2.0F * theme->panel_padding_rem);
    float query_box = theme->root_rem * 3.0F;
    float line = theme->root_rem * theme->font_body * BT_ALGS_LINE_FACTOR;
    float available = (float)config->window.height - chrome - query_box;
    long long rows = (long long)(available / line);
    if (rows < 1) {
        rows = 1;
    }
    BT_TRACE_EXIT("app.algs_list_capacity");
    return (size_t)rows;
}

size_t bt_view_algs_list_rows(const bt_view_algs_browser *browser, size_t capacity,
                              bt_view_case_row out[BT_ALGS_ROWS_MAX]) {
    BT_TRACE_ENTER("app.algs_list_rows");
    if (browser->filtered_count == 0 || capacity == 0) {
        if (capacity > 0) {
            snprintf(out[0].label, BT_ALGS_TEXT_MAX, "%s", "no case matches the query");
            out[0].selected = 0;
            BT_TRACE_EXIT("app.algs_list_rows");
            return 1;
        }
        BT_TRACE_EXIT("app.algs_list_rows");
        return 0;
    }
    bt_view_list_span window =
        bt_view_list_window(browser->filtered_count, browser->selected, capacity);
    for (size_t i = 0; i < window.count && i < BT_ALGS_ROWS_MAX; i++) {
        size_t index = window.first + i;
        char letters[3];
        bt_view_algs_letters_of(&browser->filtered[index], letters);
        snprintf(out[i].label, BT_ALGS_TEXT_MAX, "%c %s %s", index == browser->selected ? '>' : ' ',
                 letters, bt_view_algs_orbit_word(browser->filtered[index].orbit));
        out[i].selected = index == browser->selected;
    }
    BT_TRACE_EXIT("app.algs_list_rows");
    return window.count < BT_ALGS_ROWS_MAX ? window.count : BT_ALGS_ROWS_MAX;
}

void bt_view_algs_meta_line(const bt_corpus_row *row, char out[BT_ALGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.algs_meta_line");
    bt_verdict verdict;
    bt_verdict_init(&verdict);
    bt_verify(&row->alg, row->position, row->sheet, &verdict);
    char badge[32];
    if (bt_verdict_ok(&verdict)) {
        snprintf(badge, sizeof badge, "%s", "verified");
    } else {
        snprintf(badge, sizeof badge, "%zu problem(s)", verdict.problem_count);
    }
    snprintf(out, BT_ALGS_TEXT_MAX, "%s qtm %u htm %u [%s]", bt_solver_dir(row->solver),
             bt_alg_qtm(&row->alg), bt_alg_htm(&row->alg), badge);
    bt_verdict_free(&verdict);
    BT_TRACE_EXIT("app.algs_meta_line");
}

size_t bt_view_algs_pair_expand_rows(const bt_corpus_row *row, char out[BT_ALGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.algs_pair_expand_rows");
    bt_expanded_list units;
    bt_expanded_list_init(&units);
    const char *displayed =
        bt_decompose_verified(row->commutator, &row->alg, &units) ? "" : row->alg_text;
    bt_expanded_list_free(&units);
    int written =
        bt_view_expand_alg_line(row->commutator, &row->alg, row->alg_text, displayed, out);
    BT_TRACE_EXIT("app.algs_pair_expand_rows");
    return written ? 1 : 0;
}

size_t bt_view_algs_mechanic_spans(const bt_expanded_list *units,
                                   bt_view_algs_span out[BT_ALGS_SPANS_MAX]) {
    BT_TRACE_ENTER("app.algs_mechanic_spans");
    size_t count = 0;
    for (size_t index = 0; index < units->count; index++) {
        const bt_expanded *unit = &units->items[index];
        if (index > 0 && count < BT_ALGS_SPANS_MAX) {
            snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", " + ");
            out[count++].color = bt_theme_text_muted();
        }
        if (unit->has_setup && count < BT_ALGS_SPANS_MAX) {
            bt_strbuf text;
            bt_strbuf_init(&text, 128);
            bt_alg_format(&unit->setup, &text);
            snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s: ", text.data);
            free(text.data);
            out[count++].color = bt_theme_setup();
        }
        if (count < BT_ALGS_SPANS_MAX) {
            snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", "[");
            out[count++].color = bt_theme_text_muted();
        }
        bt_strbuf first;
        bt_strbuf_init(&first, 128);
        bt_strbuf second;
        bt_strbuf_init(&second, 128);
        if (unit->b_first) {
            bt_alg_format(&unit->b, &first);
            bt_alg_format(&unit->a, &second);
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", first.data);
                out[count++].color = bt_theme_interchange();
            }
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", ",");
                out[count++].color = bt_theme_text_muted();
            }
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", second.data);
                out[count++].color = bt_theme_teal();
            }
        } else {
            bt_alg_format(&unit->a, &first);
            bt_alg_format(&unit->b, &second);
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", first.data);
                out[count++].color = bt_theme_teal();
            }
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", ",");
                out[count++].color = bt_theme_text_muted();
            }
            if (count < BT_ALGS_SPANS_MAX) {
                snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", second.data);
                out[count++].color = bt_theme_interchange();
            }
        }
        free(first.data);
        free(second.data);
        if (count < BT_ALGS_SPANS_MAX) {
            snprintf(out[count].text, BT_ALGS_TEXT_MAX, "%s", "]");
            out[count++].color = bt_theme_text_muted();
        }
    }
    BT_TRACE_EXIT("app.algs_mechanic_spans");
    return count;
}

static bt_view_algs_app_state *shared_algs_app;

void bt_view_algs_app_init(bt_view_algs_app_state *app) {
    BT_TRACE_ENTER("app.algs_app_init");
    memset(app, 0, sizeof *app);
    app->browser.all_count = bt_view_algs_all_cases(app->browser.all);
    app->initialized = 1;
    BT_TRACE_EXIT("app.algs_app_init");
}

bt_view_algs_app_state *bt_view_algs_app(void) {
    BT_TRACE_ENTER("app.algs_app");
    if (shared_algs_app == NULL) {
        shared_algs_app = malloc(sizeof *shared_algs_app);
        if (shared_algs_app == NULL) {
            BT_TRACE_FAIL("app.algs_app", "out of memory");
            BT_TRACE_EXIT("app.algs_app");
            return NULL;
        }
        bt_view_algs_app_init(shared_algs_app);
    }
    BT_TRACE_EXIT("app.algs_app");
    return shared_algs_app;
}

void bt_view_algs_highlight(bt_app_net_state *net, const bt_case *case_id) {
    BT_TRACE_ENTER("app.algs_highlight");
    net->cube = bt_cube_solved();
    net->has_buffer = 1;
    net->buffer_sticker = bt_view_buffer_of(case_id->orbit);
    net->has_target[0] = 1;
    net->has_target[1] = 1;
    net->targets[0] = case_id->t1;
    net->targets[1] = case_id->t2;
    BT_TRACE_EXIT("app.algs_highlight");
}

void bt_view_algs_refresh(const char *query, bt_view_algs_browser *browser) {
    BT_TRACE_ENTER("app.algs_refresh");
    browser->filtered_count =
        bt_view_algs_filter_cases(browser->all, browser->all_count, query, browser->filtered);
    if (browser->selected >= browser->filtered_count) {
        browser->selected = browser->filtered_count > 0 ? browser->filtered_count - 1 : 0;
    }
    BT_TRACE_EXIT("app.algs_refresh");
}

void bt_view_algs_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_enter");
    bt_view_algs_app_state *app = bt_view_algs_app();
    app->query_focused = 0;
    bt_view_algs_refresh(app->query, &app->browser);
    app->browser.selected = 0;
    if (app->browser.filtered_count > 0) {
        bt_view_algs_highlight(&ctx->net, &app->browser.filtered[0]);
    }
    bt_repo_enter(&ctx->stores, bt_view_algs_repo());
    bt_view_algs_override_enter(ctx);
    bt_view_case_drill_cards_enter(ctx);
    BT_TRACE_EXIT("app.algs_enter");
}

void bt_view_algs_read_query(bt_app_ctx *ctx, const char *text) {
    BT_TRACE_ENTER("app.algs_read_query");
    (void)ctx;
    bt_view_algs_app_state *app = bt_view_algs_app();
    if (strcmp(app->query, text) != 0) {
        snprintf(app->query, sizeof app->query, "%s", text);
    }
    BT_TRACE_EXIT("app.algs_read_query");
}

void bt_view_algs_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_update");
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_view_override_draft *draft = bt_view_algs_draft();
    if (app->query_focused) {
        ctx->input_focus = 1;
    }
    if (app->query_focused && !ctx->input_focus) {
        app->query_focused = 0;
    }
    if (!app->query_focused && draft->has_case) {
        const bt_ui_input *frame = bt_app_frame_input();
        if (frame != NULL) {
            for (int i = 0; i < frame->char_count; i++) {
                size_t len = strlen(draft->text);
                if (len + 1 < sizeof draft->text) {
                    draft->text[len] = (char)frame->chars[i];
                    draft->text[len + 1] = 0;
                }
            }
        }
    }
    static const int polled[] = {
        KEY_SLASH, KEY_RIGHT, KEY_LEFT, KEY_DOWN,  KEY_UP,  KEY_N,         KEY_P,
        KEY_D,     KEY_O,     KEY_X,    KEY_ENTER, KEY_TAB, KEY_BACKSPACE,
    };
    int keys[sizeof polled / sizeof polled[0]];
    int count = 0;
    for (size_t i = 0; i < sizeof polled / sizeof polled[0]; i++) {
        if (IsKeyPressed(polled[i])) {
            keys[count++] = polled[i];
        }
    }
    if (app->query_focused) {
        const bt_ui_input *frame = bt_app_frame_input();
        if (frame != NULL) {
            for (int i = 0; i < frame->char_count; i++) {
                if (count < (int)(sizeof keys / sizeof keys[0])) {
                    keys[count++] = frame->chars[i];
                }
            }
        }
        bt_view_algs_frame(ctx, keys, count);
        bt_view_algs_refresh(app->query, &app->browser);
        BT_TRACE_EXIT("app.algs_update");
        return;
    }
    bt_view_algs_frame(ctx, keys, count);
    BT_TRACE_EXIT("app.algs_update");
}

void bt_view_case_drill_open_request(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.case_drill_open_request");
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_view_case_drill_app_state *drill = bt_view_case_drill_app();
    if (!repo->repo_focused) {
        BT_TRACE_EXIT("app.case_drill_open_request");
        return;
    }
    bt_drill_case_queue queue;
    bt_drill_case_queue_init(&queue);
    if (!bt_case_drill_request(repo, bt_view_algs_overrides(), &drill->cards, bt_views_now_day(),
                               &queue)) {
        bt_drill_case_queue_free(&queue);
        BT_TRACE_EXIT("app.case_drill_open_request");
        return;
    }
    bt_view_case_drill_set_pending(&queue);
    bt_drill_case_queue_free(&queue);
    bt_app_request_state(ctx, BT_APP_CASE_DRILL);
    bt_app_transition_frame(ctx);
    BT_TRACE_EXIT("app.case_drill_open_request");
}

void bt_view_algs_key(bt_app_ctx *ctx, int key) {
    BT_TRACE_ENTER("app.algs_key");
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_repo_browser *repo = bt_view_algs_repo();
    if (app->query_focused && !ctx->input_focus) {
        app->query_focused = 0;
    }
    int typing = app->query_focused;
    int step = 0;
    int drill = 0;
    int deeper = 0;
    int shallower = 0;
    if (key == KEY_SLASH && !typing) {
        app->query_focused = 1;
        ctx->input_focus = 1;
        BT_TRACE_EXIT("app.algs_key");
        return;
    }
    if (key == KEY_DOWN || (key == KEY_N && !typing)) {
        step = 1;
    } else if (key == KEY_UP || (key == KEY_P && !typing)) {
        step = -1;
    } else if (key == KEY_RIGHT) {
        deeper = 1;
    } else if (key == KEY_LEFT) {
        shallower = 1;
    } else if ((key == KEY_ENTER || key == KEY_D) && !typing) {
        drill = 1;
    }
    if (deeper) {
        bt_repo_deeper(repo);
    }
    if (shallower) {
        bt_repo_shallower(repo);
    }
    if (bt_repo_browser_letters_focused(repo)) {
        if (step != 0 && app->browser.filtered_count > 0) {
            long long len = (long long)app->browser.filtered_count;
            long long current = (long long)app->browser.selected;
            long long next = ((current + step) % len + len) % len;
            app->browser.selected = (size_t)next;
        }
        if (app->browser.filtered_count > 0) {
            bt_view_algs_highlight(&ctx->net, &app->browser.filtered[app->browser.selected]);
        }
        if (drill && app->browser.selected < app->browser.filtered_count) {
            bt_view_drill_set_pending(app->browser.filtered[app->browser.selected]);
            bt_app_request_state(ctx, BT_APP_DRILL);
            bt_app_transition_frame(ctx);
        }
        BT_TRACE_EXIT("app.algs_key");
        return;
    }
    if (step != 0) {
        bt_repo_move(repo, step);
    }
    if (drill) {
        bt_view_case_drill_open_request(ctx);
    }
    BT_TRACE_EXIT("app.algs_key");
}

static int push_detail(bt_view_algs_detail_line **out, size_t *cap, size_t *count,
                       bt_algs_style style, const char *text, bt_theme_color color) {
    if (*count == *cap) {
        size_t grown_cap = *cap == 0 ? 16 : *cap * 2;
        bt_view_algs_detail_line *grown = realloc(*out, grown_cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.algs_detail_lines", "out of memory");
            return -1;
        }
        *out = grown;
        *cap = grown_cap;
    }
    (*out)[*count].style = style;
    snprintf((*out)[*count].text, BT_ALGS_TEXT_MAX, "%s", text);
    (*out)[*count].color = color;
    (*count)++;
    return 0;
}

void bt_view_algs_style_metrics(bt_algs_style style, float *size, bt_theme_color *color) {
    BT_TRACE_ENTER("app.algs_style_metrics");
    const bt_theme *theme = bt_theme_table();
    float font = theme->font_small;
    bt_theme_color paint = bt_theme_text_muted();
    switch (style) {
    case BT_ALGS_STYLE_FOCAL:
        font = theme->font_title;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_MUTED:
    case BT_ALGS_STYLE_BODY:
        font = theme->font_body;
        paint = bt_theme_text_muted();
        break;
    case BT_ALGS_STYLE_HEADING:
        font = theme->font_heading;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_ALG:
        font = theme->font_alg;
        paint = bt_theme_text_primary();
        break;
    case BT_ALGS_STYLE_SMALL:
        font = theme->font_small;
        paint = bt_theme_text_muted();
        break;
    case BT_ALGS_STYLE_MICRO:
        font = theme->font_micro;
        paint = bt_theme_text_muted();
        break;
    }
    *size = font;
    if (color != NULL) {
        *color = paint;
    }
    BT_TRACE_EXIT("app.algs_style_metrics");
}

size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line **out) {
    BT_TRACE_ENTER("app.algs_detail_lines");
    bt_view_algs_detail_line *lines = NULL;
    size_t cap = 0;
    size_t count = 0;
    *out = NULL;
    char text[BT_ALGS_TEXT_MAX];
    snprintf(text, sizeof text, "%zu of %zu cases match", browser->filtered_count,
             browser->all_count);
    if (push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted()) != 0) {
        free(lines);
        BT_TRACE_EXIT("app.algs_detail_lines");
        return 0;
    }
    if (browser->filtered_count == 0 || browser->selected >= browser->filtered_count) {
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, "no case matches the query",
                    bt_theme_text_muted());
        BT_TRACE_EXIT("app.algs_detail_lines");
        *out = lines;
        return count;
    }
    const bt_case *case_id = &browser->filtered[browser->selected];
    char letters[3];
    bt_view_algs_letters_of(case_id, letters);
    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_FOCAL, letters, bt_theme_text_primary());
    char position[BT_ALGS_TEXT_MAX];
    bt_view_case_position(case_id, position);
    int covered = bt_view_algs_covered(corpus, case_id);
    snprintf(text, sizeof text, "%s %s covered %s", bt_view_algs_orbit_word(case_id->orbit),
             position, covered ? "yes" : "no");
    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text,
                covered ? bt_theme_ok() : bt_theme_danger());
    const bt_corpus_row **rows = NULL;
    size_t row_count = bt_view_algs_case_rows_dyn(corpus, case_id, &rows);
    if (row_count == 0) {
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, "no vendor algs",
                    bt_theme_text_muted());
    }
    for (size_t i = 0; i < row_count; i++) {
        const bt_corpus_row *row = rows[i];
        if (i == 0) {
            bt_expanded_list units;
            bt_expanded_list_init(&units);
            if (bt_decompose_verified(row->commutator, &row->alg, &units)) {
                bt_view_algs_span spans[BT_ALGS_SPANS_MAX];
                size_t span_count = bt_view_algs_mechanic_spans(&units, spans);
                for (size_t span = 0; span < span_count; span++) {
                    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_ALG, spans[span].text,
                                spans[span].color);
                }
                bt_expanded_list_free(&units);
            } else {
                bt_expanded_list_free(&units);
                push_detail(&lines, &cap, &count, BT_ALGS_STYLE_ALG, row->alg_text,
                            bt_theme_text_primary());
                if (row->commutator[0] != '\0') {
                    snprintf(text, sizeof text, "commutator %s", row->commutator);
                    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text,
                                bt_theme_text_muted());
                }
            }
            char expand[BT_ALGS_TEXT_MAX];
            if (bt_view_algs_pair_expand_rows(row, expand)) {
                push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, expand,
                            bt_theme_text_muted());
            }
        } else {
            push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MUTED, row->alg_text,
                        bt_theme_text_muted());
        }
        bt_view_algs_meta_line(row, text);
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());
        snprintf(text, sizeof text, "recon %s video %s", row->recon_link, row->youtube_link);
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    }
    free(rows);
    BT_TRACE_EXIT("app.algs_detail_lines");
    *out = lines;
    return count;
}

void bt_view_algs_draw(const bt_app_ctx *ctx, const void *font_regular, const void *font_bold,
                       float window_w, float window_h) {
    BT_TRACE_ENTER("app.view_algs_draw");
    const bt_theme *theme = bt_theme_table();
    bt_view_frame_metrics metrics = bt_view_frame_metrics_for(window_w, window_h);
    bt_view_frame_draw(
        font_bold, font_regular, window_w, window_h, "algs",
        "type letters to search, arrows select, left right browse packs, o edits a "
        "case, enter saves, tab cancels, x deletes the override, d drills, esc menu");
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_app_config config;
    bt_app_config_defaults(&config);
    config.window.width = (unsigned)window_w;
    config.window.height = (unsigned)window_h;
    size_t capacity = bt_view_algs_list_capacity(&config);
    float y = metrics.content_y;
    if (bt_repo_browser_letters_focused(repo)) {
        bt_view_case_row rows[BT_ALGS_ROWS_MAX];
        size_t count = bt_view_algs_list_rows(&app->browser, capacity, rows);
        for (size_t i = 0; i < count; i++) {
            bt_app_draw_text(font_regular, rows[i].label, metrics.content_x, y,
                             theme->font_body * theme->root_rem,
                             rows[i].selected ? bt_theme_text_primary() : bt_theme_text_muted());
            y += theme->font_body * theme->root_rem * BT_ALGS_LINE_FACTOR;
        }
        bt_view_algs_detail_line *detail_lines = NULL;
        size_t detail_count =
            bt_view_algs_detail_lines(&ctx->vendor_corpus, &app->browser, &detail_lines);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            float font_size = theme->font_small;
            bt_view_algs_style_metrics(detail_lines[i].style, &font_size, NULL);
            bt_app_draw_text(font_regular, detail_lines[i].text, detail_x, detail_y,
                             font_size * theme->root_rem, detail_lines[i].color);
            detail_y += font_size * theme->root_rem * BT_ALGS_LINE_FACTOR;
        }
        free(detail_lines);
    } else {
        bt_repo_list_row rows[BT_REPO_LIST_MAX];
        size_t count = bt_repo_list_rows(repo, BT_REPO_LIST_MAX, rows);
        for (size_t i = 0; i < count; i++) {
            bt_app_draw_text(font_regular, rows[i].label, metrics.content_x, y,
                             theme->font_body * theme->root_rem,
                             rows[i].selected ? bt_theme_text_primary() : bt_theme_text_muted());
            y += theme->font_body * theme->root_rem * BT_ALGS_LINE_FACTOR;
        }
        bt_detail_line lines[BT_DETAIL_LINES_MAX];
        size_t detail_count = 0;
        bt_view_repo_detail_lines(repo, bt_view_algs_overrides(), bt_view_algs_draft(), lines,
                                  BT_DETAIL_LINES_MAX, &detail_count);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            float size = lines[i].kind == BT_DETAIL_TITLE ? theme->font_title : theme->font_small;
            bt_app_draw_text(
                font_regular, lines[i].text, detail_x, detail_y, size * theme->root_rem,
                lines[i].kind == BT_DETAIL_WARN ? bt_theme_danger() : bt_theme_text_muted());
            detail_y += size * theme->root_rem * BT_ALGS_LINE_FACTOR;
        }
    }
    BT_TRACE_EXIT("app.view_algs_draw");
}

void bt_view_algs_row_draw(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.view_algs_row_draw");
    const bt_app_font_book *book = bt_app_fonts_current();
    if (book == NULL) {
        BT_TRACE_FAIL("app.view_algs_row_draw", "fonts not loaded");
        BT_TRACE_EXIT("app.view_algs_row_draw");
        return;
    }
    bt_view_algs_draw(ctx, book->regular, book->bold, (float)GetScreenWidth(),
                      (float)GetScreenHeight());
    BT_TRACE_EXIT("app.view_algs_row_draw");
}
void bt_view_override_open(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_override_open");
    (void)ctx;
    bt_case_site site;
    if (!bt_view_case_site_of(bt_view_algs_repo(), &site)) {
        BT_TRACE_EXIT("app.algs_override_open");
        return;
    }
    bt_view_override_draft *draft = bt_view_algs_draft();
    draft->has_case = 1;
    draft->site = site;
    const bt_repo_node *node = bt_repo_browser_node(bt_view_algs_repo());
    const char *seed =
        node != NULL && node->has_case && node->case_alg != NULL ? node->case_alg : "";
    const char *current = bt_overrides_get(bt_view_algs_overrides(), &site.case_ref);
    snprintf(draft->text, sizeof draft->text, "%s", current != NULL ? current : seed);
    draft->has_error = 0;
    draft->error[0] = '\0';
    BT_TRACE_EXIT("app.algs_override_open");
}

void bt_view_override_cancel(void) {
    BT_TRACE_ENTER("app.algs_override_cancel");
    bt_view_override_draft_default(bt_view_algs_draft());
    BT_TRACE_EXIT("app.algs_override_cancel");
}

void bt_view_override_submit(bt_app_ctx *ctx, const char *text) {
    BT_TRACE_ENTER("app.algs_override_submit");
    bt_view_override_draft *draft = bt_view_algs_draft();
    if (!draft->has_case) {
        BT_TRACE_EXIT("app.algs_override_submit");
        return;
    }
    size_t len = strlen(text);
    size_t start = 0;
    size_t end = len;
    while (start < end) {
        size_t run = bt_space_len(text, start, len);
        if (run == 0) {
            break;
        }
        start += run;
    }
    while (end > start) {
        size_t run = bt_space_len(text, end - 1, len);
        if (run == 0) {
            break;
        }
        end -= run;
    }
    char trimmed[BT_ALGS_TEXT_MAX];
    size_t trimmed_len = 0;
    for (size_t at = start; at < end && trimmed_len + 1 < sizeof trimmed; at++) {
        trimmed[trimmed_len++] = text[at];
    }
    trimmed[trimmed_len] = '\0';
    if (trimmed[0] == '\0') {
        bt_view_override_cancel();
        BT_TRACE_EXIT("app.algs_override_submit");
        return;
    }
    char error[BT_ALGS_TEXT_MAX];
    if (bt_view_geometry_check(draft->site.case_ref.event, draft->site.has_size, draft->site.size,
                               trimmed, error, sizeof error) != 0) {
        draft->has_error = 1;
        snprintf(draft->error, sizeof draft->error, "%s", error);
        BT_TRACE_FAIL("app.algs_override_submit", "%s", error);
        BT_TRACE_EXIT("app.algs_override_submit");
        return;
    }
    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx->paths.user_root, db_path);
    if (bt_db_open_at(db_path, &db, &err) != 0 ||
        bt_repo_user_upsert(&db, &draft->site.case_ref, trimmed, &err) != 0) {
        draft->has_error = 1;
        snprintf(draft->error, sizeof draft->error, "%s", err.detail);
        if (db.conn != NULL) {
            bt_db_close(&db);
        }
        BT_TRACE_FAIL("app.algs_override_submit", "%s", err.detail);
        BT_TRACE_EXIT("app.algs_override_submit");
        return;
    }
    bt_db_close(&db);
    bt_overrides_put(bt_view_algs_overrides(), &draft->site.case_ref, trimmed);
    bt_view_override_cancel();
    BT_TRACE_EXIT("app.algs_override_submit");
}

void bt_view_override_delete(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_override_delete");
    bt_case_site site;
    if (!bt_view_case_site_of(bt_view_algs_repo(), &site)) {
        BT_TRACE_EXIT("app.algs_override_delete");
        return;
    }
    if (!bt_overrides_contains(bt_view_algs_overrides(), &site.case_ref)) {
        BT_TRACE_EXIT("app.algs_override_delete");
        return;
    }
    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx->paths.user_root, db_path);
    int opened = bt_db_open_at(db_path, &db, &err);
    int removed = opened == 0 ? bt_repo_user_remove(&db, &site.case_ref, &err) : -1;
    if (opened != 0 || removed < 0) {
        bt_view_override_draft *draft = bt_view_algs_draft();
        draft->has_error = 1;
        snprintf(draft->error, sizeof draft->error, "%s", err.detail);
        if (db.conn != NULL) {
            bt_db_close(&db);
        }
        BT_TRACE_FAIL("app.algs_override_delete", "%s", err.detail);
        BT_TRACE_EXIT("app.algs_override_delete");
        return;
    }
    bt_db_close(&db);
    bt_overrides_remove(bt_view_algs_overrides(), &site.case_ref);
    BT_TRACE_EXIT("app.algs_override_delete");
}

int bt_view_algs_frame(bt_app_ctx *ctx, const int *keys, int count) {
    BT_TRACE_ENTER("app.algs_frame");
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_repo_browser *repo = bt_view_algs_repo();
    if (app->query_focused && !ctx->input_focus) {
        app->query_focused = 0;
    }
    int typing = app->query_focused;
    for (int i = 0; i < count; i++) {
        int key = keys[i];
        if (key == KEY_SLASH && !typing) {
            app->query_focused = 1;
            typing = 1;
            ctx->input_focus = 1;
            continue;
        }
        bt_view_override_draft *draft = bt_view_algs_draft();
        if (draft->has_case) {
            if (key == KEY_ENTER) {
                bt_view_override_submit(ctx, draft->text);
                continue;
            }
            if (key == KEY_TAB) {
                bt_view_override_cancel();
                continue;
            }
            if (key == KEY_BACKSPACE) {
                size_t len = strlen(draft->text);
                if (len > 0) {
                    draft->text[len - 1] = '\0';
                }
                continue;
            }
            if (key >= 32 && key < 127) {
                size_t len = strlen(draft->text);
                if (len + 1 < sizeof draft->text) {
                    draft->text[len] = (char)key;
                    draft->text[len + 1] = '\0';
                }
                continue;
            }
        }
        if (typing && !draft->has_case) {
            if (key == KEY_BACKSPACE) {
                size_t len = strlen(app->query);
                if (len > 0) {
                    app->query[len - 1] = '\0';
                }
                continue;
            }
            if (key >= 32 && key < 127 && key != KEY_SLASH) {
                size_t len = strlen(app->query);
                if (len + 1 < sizeof app->query) {
                    app->query[len] = (char)key;
                    app->query[len + 1] = '\0';
                }
                continue;
            }
        }
        if (key == KEY_RIGHT) {
            bt_repo_deeper(repo);
        }
        if (key == KEY_LEFT) {
            bt_repo_shallower(repo);
        }
        if (bt_repo_browser_letters_focused(repo)) {
            int step = 0;
            if (key == KEY_DOWN || (key == KEY_N && !typing)) {
                step = 1;
            } else if (key == KEY_UP || (key == KEY_P && !typing)) {
                step = -1;
            }
            if (step != 0 && app->browser.filtered_count > 0) {
                long long len = (long long)app->browser.filtered_count;
                long long current = (long long)app->browser.selected;
                long long next = ((current + step) % len + len) % len;
                app->browser.selected = (size_t)next;
            }
            if (app->browser.filtered_count > 0) {
                bt_view_algs_highlight(&ctx->net, &app->browser.filtered[app->browser.selected]);
            }
            if (!typing && (key == KEY_ENTER || key == KEY_D) &&
                app->browser.selected < app->browser.filtered_count) {
                bt_view_drill_set_pending(app->browser.filtered[app->browser.selected]);
                bt_app_request_state(ctx, BT_APP_DRILL);
                bt_app_transition_frame(ctx);
            }
        } else {
            int step = 0;
            if (key == KEY_DOWN || (key == KEY_N && !typing)) {
                step = 1;
            } else if (key == KEY_UP || (key == KEY_P && !typing)) {
                step = -1;
            }
            if (step != 0) {
                bt_repo_move(repo, step);
            }
            if (!typing && key == KEY_D) {
                bt_view_case_drill_open_request(ctx);
            }
        }
        if (!draft->has_case && !typing) {
            if (key == KEY_O) {
                bt_view_override_open(ctx);
            } else if (key == KEY_X) {
                bt_view_override_delete(ctx);
            }
        }
    }
    BT_TRACE_EXIT("app.algs_frame");
    return 0;
}
