#include "app/views/algs/mod.h"

#include "app/views/drill.h"
#include "app/window.h"
#include "common/space.h"

#include "raylib.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    float line = theme->root_rem * theme->font_body * 1.5F;
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
    bt_view_algs_refresh(app->query, &app->browser);
    app->browser.selected = 0;
    if (app->browser.filtered_count > 0) {
        bt_view_algs_highlight(&ctx->net, &app->browser.filtered[0]);
    }
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
    if (app->query_focused) {
        ctx->input_focus = 1;
    }
    int step = 0;
    int drill = 0;
    int focus_query = 0;
    if (IsKeyPressed(KEY_SLASH)) {
        focus_query = 1;
    }
    if (IsKeyPressed(KEY_DOWN)) {
        step += 1;
    }
    if (IsKeyPressed(KEY_UP)) {
        step -= 1;
    }
    if (!app->query_focused && IsKeyPressed(KEY_N)) {
        step += 1;
    }
    if (!app->query_focused && IsKeyPressed(KEY_P)) {
        step -= 1;
    }
    if (!app->query_focused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_D))) {
        drill = 1;
    }
    if (focus_query) {
        app->query_focused = 1;
        ctx->input_focus = 1;
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
    if (drill && app->browser.selected < app->browser.filtered_count) {
        bt_view_drill_set_pending(app->browser.filtered[app->browser.selected]);
        bt_app_request_state(ctx, BT_APP_DRILL);
        bt_app_apply_pending(ctx);
    }
    if (app->query_focused) {
        int code = GetCharPressed();
        while (code > 0) {
            size_t len = strlen(app->query);
            if (len + 1 < sizeof app->query) {
                app->query[len] = (char)code;
                app->query[len + 1] = '\0';
            }
            code = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(app->query);
            if (len > 0) {
                app->query[len - 1] = '\0';
            }
        }
        bt_view_algs_refresh(app->query, &app->browser);
    }
    BT_TRACE_EXIT("app.algs_update");
}
