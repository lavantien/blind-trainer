from pathlib import Path

header = Path("src/app/views/algs/mod.h")
text = header.read_text(encoding="utf-8")

old = """size_t bt_view_algs_case_rows(const bt_corpus *corpus, const bt_case *case_id,
                              const bt_corpus_row *out[BT_ALGS_ROWS_MAX]);
int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id);"""
new = """size_t bt_view_algs_case_rows(const bt_corpus *corpus, const bt_case *case_id,
                              const bt_corpus_row *out[BT_ALGS_ROWS_MAX]);
size_t bt_view_algs_case_rows_dyn(const bt_corpus *corpus, const bt_case *case_id,
                                  const bt_corpus_row **out);
int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id);"""
assert old in text, "header case rows"
text = text.replace(old, new, 1)

old = """size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line out[BT_ALGS_ROWS_MAX * 2]);"""
new = """size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line **out);
void bt_view_algs_style_metrics(bt_algs_style style, float *size, bt_theme_color *color);"""
assert old in text, "header detail lines"
text = text.replace(old, new, 1)

header.write_text(text, encoding="utf-8")

p = Path("src/app/views/algs/mod.c")
text = p.read_text(encoding="utf-8")

old = """int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id) {"""
new = """size_t bt_view_algs_case_rows_dyn(const bt_corpus *corpus, const bt_case *case_id,
                                  const bt_corpus_row **out) {
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
                const bt_corpus_row **grown =
                    realloc(rows, grown_cap * sizeof *grown);
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

int bt_view_algs_covered(const bt_corpus *corpus, const bt_case *case_id) {"""
assert old in text, "case rows dyn anchor"
text = text.replace(old, new, 1)

old = """void bt_view_algs_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_enter");
    bt_view_algs_app_state *app = bt_view_algs_app();
    bt_view_algs_refresh(app->query, &app->browser);"""
new = """void bt_view_algs_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.algs_enter");
    bt_view_algs_app_state *app = bt_view_algs_app();
    app->query_focused = 0;
    bt_view_algs_refresh(app->query, &app->browser);"""
assert old in text, "enter reset anchor"
text = text.replace(old, new, 1)

old = """void bt_view_case_drill_open_request(bt_app_ctx *ctx) {
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
    bt_app_transition_frame(ctx);
    BT_TRACE_EXIT("app.case_drill_open_request");
}"""
new = """void bt_view_case_drill_open_request(bt_app_ctx *ctx) {
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
}"""
assert old in text, "open request anchor"
text = text.replace(old, new, 1)

old = """static void push_detail(bt_view_algs_detail_line *out, size_t cap, size_t *count,
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
    size_t count = 0;"""
new = """static int push_detail(bt_view_algs_detail_line **out, size_t *cap, size_t *count,
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
    *color = paint;
    BT_TRACE_EXIT("app.algs_style_metrics");
}

size_t bt_view_algs_detail_lines(const bt_corpus *corpus, const bt_view_algs_browser *browser,
                                 bt_view_algs_detail_line **out) {
    BT_TRACE_ENTER("app.algs_detail_lines");
    bt_view_algs_detail_line *lines = NULL;
    size_t cap = 0;
    size_t count = 0;
    *out = NULL;"""
assert old in text, "push detail + signature anchor"
text = text.replace(old, new, 1)

old = """    snprintf(text, sizeof text, "%zu of %zu cases match", browser->filtered_count,
             browser->all_count);
    push_detail(out, cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    if (browser->filtered_count == 0 || browser->selected >= browser->filtered_count) {
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, "no case matches the query",
                    bt_theme_text_muted());
        BT_TRACE_EXIT("app.algs_detail_lines");
        return count;
    }"""
new = """    snprintf(text, sizeof text, "%zu of %zu cases match", browser->filtered_count,
             browser->all_count);
    if (push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted()) !=
        0) {
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
    }"""
assert old in text, "detail head anchor"
text = text.replace(old, new, 1)

replacements = [
    ("""    push_detail(out, cap, &count, BT_ALGS_STYLE_FOCAL, letters, bt_theme_text_primary());""",
     """    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_FOCAL, letters, bt_theme_text_primary());"""),
    ("""    push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text,
                covered ? bt_theme_ok() : bt_theme_danger());""",
     """    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text,
                covered ? bt_theme_ok() : bt_theme_danger());"""),
    ("""    const bt_corpus_row *rows[BT_ALGS_ROWS_MAX];
    size_t row_count = bt_view_algs_case_rows(corpus, case_id, rows);
    if (row_count == 0) {
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, "no vendor algs", bt_theme_text_muted());
    }""",
     """    const bt_corpus_row **rows = NULL;
    size_t row_count = bt_view_algs_case_rows_dyn(corpus, case_id, &rows);
    if (row_count == 0) {
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, "no vendor algs",
                    bt_theme_text_muted());
    }"""),
    ("""                for (size_t span = 0; span < span_count; span++) {
                    push_detail(out, cap, &count, BT_ALGS_STYLE_ALG, spans[span].text,
                                spans[span].color);
                }""",
     """                for (size_t span = 0; span < span_count; span++) {
                    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_ALG, spans[span].text,
                                spans[span].color);
                }"""),
    ("""                push_detail(out, cap, &count, BT_ALGS_STYLE_ALG, row->alg_text,
                            bt_theme_text_primary());""",
     """                push_detail(&lines, &cap, &count, BT_ALGS_STYLE_ALG, row->alg_text,
                            bt_theme_text_primary());"""),
    ("""                    snprintf(text, sizeof text, "commutator %s", row->commutator);
                    push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());""",
     """                    snprintf(text, sizeof text, "commutator %s", row->commutator);
                    push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text,
                                bt_theme_text_muted());"""),
    ("""            if (bt_view_algs_pair_expand_rows(row, expand)) {
                push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, expand, bt_theme_text_muted());
            }""",
     """            if (bt_view_algs_pair_expand_rows(row, expand)) {
                push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, expand,
                            bt_theme_text_muted());
            }"""),
    ("""            push_detail(out, cap, &count, BT_ALGS_STYLE_MUTED, row->alg_text,
                        bt_theme_text_muted());""",
     """            push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MUTED, row->alg_text,
                        bt_theme_text_muted());"""),
    ("""        bt_view_algs_meta_line(row, text);
        push_detail(out, cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());
        snprintf(text, sizeof text, "recon %s video %s", row->recon_link, row->youtube_link);
        push_detail(out, cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    }
    BT_TRACE_EXIT("app.algs_detail_lines");
    return count;
}""",
     """        bt_view_algs_meta_line(row, text);
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_SMALL, text, bt_theme_text_muted());
        snprintf(text, sizeof text, "recon %s video %s", row->recon_link, row->youtube_link);
        push_detail(&lines, &cap, &count, BT_ALGS_STYLE_MICRO, text, bt_theme_text_muted());
    }
    free(rows);
    BT_TRACE_EXIT("app.algs_detail_lines");
    *out = lines;
    return count;
}"""),
]
for old, new in replacements:
    assert old in text, f"detail body anchor: {old[:60]}"
    text = text.replace(old, new, 1)

old = """    bt_view_frame_draw(
        font_bold, font_regular, window_w, window_h, "algs",
        "type letters to search, arrows select, left right browse packs, o edits a "
        "case, enter saves, tab cancels, x deletes the override, d drills, esc menu");
    bt_view_frame_draw(
        font_bold, font_regular, window_w, window_h, "algs",
        "type letters to search, arrows select, left right browse packs, o edits a "
        "case, enter saves, tab cancels, x deletes the override, d drills, esc menu");"""
new = """    bt_view_frame_draw(
        font_bold, font_regular, window_w, window_h, "algs",
        "type letters to search, arrows select, left right browse packs, o edits a "
        "case, enter saves, tab cancels, x deletes the override, d drills, esc menu");"""
assert old in text, "duplicate frame draw anchor"
text = text.replace(old, new, 1)

old = """        bt_view_algs_detail_line lines[BT_ALGS_ROWS_MAX * 2];
        size_t detail_count = bt_view_algs_detail_lines(&ctx->vendor_corpus, &app->browser, lines);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            bt_app_draw_text(font_regular, lines[i].text, detail_x, detail_y, theme->font_small,
                             lines[i].color);
            detail_y += theme->font_small * theme->root_rem * 1.5F;
        }"""
new = """        bt_view_algs_detail_line *detail_lines = NULL;
        size_t detail_count =
            bt_view_algs_detail_lines(&ctx->vendor_corpus, &app->browser, &detail_lines);
        float detail_x = metrics.content_x + metrics.content_w * 0.4F;
        float detail_y = metrics.content_y;
        for (size_t i = 0; i < detail_count; i++) {
            float font_size = theme->font_small;
            bt_theme_color font_color = detail_lines[i].color;
            bt_view_algs_style_metrics(detail_lines[i].style, &font_size, &font_color);
            bt_app_draw_text(font_regular, detail_lines[i].text, detail_x, detail_y, font_size,
                             detail_lines[i].color);
            detail_y += font_size * theme->root_rem * 1.5F;
        }
        free(detail_lines);"""
assert old in text, "letters draw anchor"
text = text.replace(old, new, 1)

p.write_text(text, encoding="utf-8")
print("mod.c reworked")
