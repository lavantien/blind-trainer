#include "app/views/algs/mod.h"

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

void bt_view_case_drill_open_request(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.case_drill_open_request");
    bt_repo_browser *repo = bt_view_algs_repo();
    bt_view_case_drill_app_state *drill = bt_view_case_drill_app();
    bt_drill_case queue[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = 0;
    if (!repo->repo_focused) {
        BT_TRACE_EXIT("app.case_drill_open_request");
        return;
    }
    if (!bt_case_drill_request(repo, bt_view_algs_overrides(), &drill->cards, bt_views_now_day(),
                               queue, &count)) {
        BT_TRACE_EXIT("app.case_drill_open_request");
        return;
    }
    bt_view_case_drill_set_pending(queue, count);
    bt_app_request_state(ctx, BT_APP_CASE_DRILL);
    bt_app_apply_pending(ctx);
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
            bt_app_apply_pending(ctx);
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

static void push_detail(bt_view_algs_detail_line *out, size_t cap, size_t *count,
                        bt_algs_style style, const char *text, bt_theme_color color) {
    if (*count >= cap) {
        return;
    }
    out[*count].style = style;
    snprintf(out[*count].text, BT_ALGS_TEXT_MAX, "%s", text);
    out[*count].color = color;
    (*count)++;
}

size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line out[BT_ALGS_ROWS_MAX * 2]) {
    BT_TRACE_ENTER("app.algs_detail_lines");
    size_t cap = (size_t)BT_ALGS_ROWS_MAX * 2U;
    size_t count = 0;
    char text[BT_ALGS_TEXT_MAX];
    snprintf(text, sizeof text, "%zu of %zu cases match", browser->filtered_count,
             browser->all_count);
    push_detail(out, cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    if (browser->filtered_count == 0 || browser->selected >= browser->filtered_count) {
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, "no case matches the query",
                    bt_theme_text_muted());
        BT_TRACE_EXIT("app.algs_detail_lines");
        return count;
    }
    const bt_case *case_id = &browser->filtered[browser->selected];
    char letters[3];
    bt_view_algs_letters_of(case_id, letters);
    push_detail(out, cap, &count, BT_ALGS_STYLE_FOCAL, letters, bt_theme_text_primary());
    char position[BT_ALGS_TEXT_MAX];
    bt_view_case_position(case_id, position);
    int covered = bt_view_algs_covered(corpus, case_id);
    snprintf(text, sizeof text, "%s %s covered %s", bt_view_algs_orbit_word(case_id->orbit),
             position, covered ? "yes" : "no");
    push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text,
                covered ? bt_theme_ok() : bt_theme_danger());
    const bt_corpus_row *rows[BT_ALGS_ROWS_MAX];
    size_t row_count = bt_view_algs_case_rows(corpus, case_id, rows);
    if (row_count == 0) {
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, "no vendor algs", bt_theme_text_muted());
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
                    push_detail(out, cap, &count, BT_ALGS_STYLE_ALG, spans[span].text,
                                spans[span].color);
                }
                bt_expanded_list_free(&units);
            } else {
                bt_expanded_list_free(&units);
                push_detail(out, cap, &count, BT_ALGS_STYLE_ALG, row->alg_text,
                            bt_theme_text_primary());
                if (row->commutator[0] != '\0') {
                    snprintf(text, sizeof text, "commutator %s", row->commutator);
                    push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());
                }
            }
            char expand[BT_ALGS_TEXT_MAX];
            if (bt_view_algs_pair_expand_rows(row, expand)) {
                push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, expand, bt_theme_text_muted());
            }
        } else {
            push_detail(out, cap, &count, BT_ALGS_STYLE_MUTED, row->alg_text,
                        bt_theme_text_muted());
        }
        bt_view_algs_meta_line(row, text);
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());
        snprintf(text, sizeof text, "recon %s video %s", row->recon_link, row->youtube_link);
        push_detail(out, cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    }
    BT_TRACE_EXIT("app.algs_detail_lines");
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
            bt_app_draw_text(font_regular, rows[i].label, metrics.content_x, y, theme->font_body,
                             rows[i].selected ? bt_theme_text_primary() : bt_theme_text_muted());
            y += theme->font_body * theme->root_rem * 1.5F;
        }
        bt_view_algs_detail_line lines[BT_ALGS_ROWS_MAX * 2];
        size_t detail_count = bt_view_algs_detail_lines(&ctx->vendor_corpus, &app->browser, lines);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            bt_app_draw_text(font_regular, lines[i].text, detail_x, detail_y, theme->font_small,
                             lines[i].color);
            detail_y += theme->font_small * theme->root_rem * 1.5F;
        }
    } else {
        bt_repo_list_row rows[BT_REPO_LIST_MAX];
        size_t count = bt_repo_list_rows(repo, BT_REPO_LIST_MAX, rows);
        for (size_t i = 0; i < count; i++) {
            bt_app_draw_text(font_regular, rows[i].label, metrics.content_x, y, theme->font_body,
                             rows[i].selected ? bt_theme_text_primary() : bt_theme_text_muted());
            y += theme->font_body * theme->root_rem * 1.5F;
        }
        bt_detail_line lines[BT_DETAIL_LINES_MAX];
        size_t detail_count = 0;
        bt_view_repo_detail_lines(repo, bt_view_algs_overrides(), bt_view_algs_draft(), lines,
                                  BT_DETAIL_LINES_MAX, &detail_count);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            float size = lines[i].kind == BT_DETAIL_TITLE ? theme->font_title : theme->font_small;
            bt_app_draw_text(font_regular, lines[i].text, detail_x, detail_y, size,
                             lines[i].kind == BT_DETAIL_WARN ? bt_theme_danger()
                                                             : bt_theme_text_muted());
            detail_y += size * theme->root_rem * 1.5F;
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
