from pathlib import Path

p = Path("src/app/views/case_drill.c")
text = p.read_text(encoding="utf-8")

old = """static long long size_label_value(const char *label) {
    char digits[BT_REPO_LABEL_MAX];
    size_t at = 0;
    while (label[at] != '\\0' && label[at] != 'x' && at + 1 < sizeof digits) {
        digits[at] = label[at];
        at++;
    }
    digits[at] = '\\0';
    return strtoll(digits, NULL, 10);
}

static size_t collect_node(const bt_repo_node *node, const char *event, const char *method,
                           const char *stage, int has_size, long long size,
                           const bt_overrides *store, bt_drill_case *out, size_t cap) {
    size_t count = 0;
    if (node->kind == BT_REPO_LETTERS) {
        return 0;
    }
    if (node->kind == BT_REPO_EVENT || node->kind == BT_REPO_METHOD ||
        node->kind == BT_REPO_STAGE) {
        const char *node_method = node->kind == BT_REPO_METHOD ? node->label : method;
        const char *node_stage = node->kind == BT_REPO_STAGE ? node->label : stage;
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, node_method, node_stage, has_size,
                                  size, store, out + count, cap - count);
        }
        return count;
    }
    if (node->kind == BT_REPO_SIZE) {
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, method, stage, 1,
                                  size_label_value(node->label), store, out + count, cap - count);
        }
        return count;
    }
    if (method != NULL && stage != NULL && node->has_case) {
        if (cap == 0) {
            return 0;
        }
        bt_case_ref_of(&out->case_ref, event, method, stage, node->label);
        out->has_size = has_size;
        out->size = size;
        snprintf(out->seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", node->case_alg);
        bt_effective_alg effective = bt_view_effective_alg(&out->case_ref, node->case_alg, store);
        snprintf(out->alg, BT_CASE_DRILL_TEXT_MAX, "%s", effective.alg);
        return 1;
    }
    return 0;
}"""
new = """static size_t collect_node(const bt_repo_node *node, const char *event, const char *method,
                           const char *stage, int has_size, long long size,
                           const bt_overrides *store, bt_drill_case_queue *out) {
    size_t count = 0;
    if (node->kind == BT_REPO_LETTERS) {
        return 0;
    }
    if (node->kind == BT_REPO_EVENT || node->kind == BT_REPO_METHOD ||
        node->kind == BT_REPO_STAGE) {
        const char *node_method = node->kind == BT_REPO_METHOD ? node->label : method;
        const char *node_stage = node->kind == BT_REPO_STAGE ? node->label : stage;
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, node_method, node_stage, has_size,
                                  size, store, out);
        }
        return count;
    }
    if (node->kind == BT_REPO_SIZE) {
        long long parsed = 0;
        int parsed_ok = bt_repo_size_of_label(node->label, &parsed);
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, method, stage, parsed_ok, parsed,
                                  store, out);
        }
        return count;
    }
    if (method != NULL && stage != NULL && node->has_case) {
        bt_drill_case drill;
        memset(&drill, 0, sizeof drill);
        bt_case_ref_of(&drill.case_ref, event, method, stage, node->label);
        drill.has_size = has_size;
        drill.size = size;
        snprintf(drill.seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", node->case_alg);
        bt_effective_alg effective = bt_view_effective_alg(&drill.case_ref, node->case_alg, store);
        snprintf(drill.alg, BT_CASE_DRILL_TEXT_MAX, "%s", effective.alg);
        if (bt_drill_case_queue_push(out, &drill) != 0) {
            return 0;
        }
        return 1;
    }
    return 0;
}"""
assert old in text, "collect block anchor"
text = text.replace(old, new, 1)

old = """size_t bt_case_drill_collect_cases(const bt_repo_browser *browser, const bt_overrides *store,
                                   bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX]) {"""
new = """size_t bt_case_drill_collect_cases(const bt_repo_browser *browser, const bt_overrides *store,
                                   bt_drill_case_queue *out) {"""
assert old in text, "collect_cases signature"
text = text.replace(old, new, 1)

old = """        if (node->kind == BT_REPO_SIZE) {
            has_size = 1;
            size = size_label_value(node->label);
        } else if (node->kind == BT_REPO_METHOD) {"""
new = """        if (node->kind == BT_REPO_SIZE) {
            has_size = bt_repo_size_of_label(node->label, &size);
        } else if (node->kind == BT_REPO_METHOD) {"""
assert old in text, "collect_cases size walk"
text = text.replace(old, new, 1)

old = """    size_t count = collect_node(node, arm->label, method, stage, has_size, size, store, out,
                                BT_CASE_DRILL_QUEUE_MAX);"""
new = """    size_t count = collect_node(node, arm->label, method, stage, has_size, size, store, out);"""
assert old in text, "collect cases tail"
text = text.replace(old, new, 1)

old = """size_t bt_case_drill_due_session(const bt_drill_case *cases, size_t count,
                                 const bt_case_cards *cards, long long now_day,
                                 bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX]) {
    BT_TRACE_ENTER("app.case_drill_due_session");
    bt_drill_case due[BT_CASE_DRILL_QUEUE_MAX];
    bt_drill_case seen[BT_CASE_DRILL_QUEUE_MAX];
    size_t due_count = 0;
    size_t seen_count = 0;
    for (size_t i = 0; i < count; i++) {
        const bt_card *card = bt_case_cards_get(cards, &cases[i].case_ref);
        if (card != NULL && !bt_card_is_due(card, now_day)) {
            seen[seen_count++] = cases[i];
        } else {
            due[due_count++] = cases[i];
        }
    }
    const bt_drill_case *source = due;
    size_t source_count = due_count;
    if (due_count == 0) {
        source = seen;
        source_count = seen_count;
    }
    memcpy(out, source, source_count * sizeof *source);
    BT_TRACE_EXIT("app.case_drill_due_session");
    return source_count;
}"""
new = """size_t bt_case_drill_due_session(const bt_drill_case_queue *cases,
                                 const bt_case_cards *cards, long long now_day,
                                 bt_drill_case_queue *out) {
    BT_TRACE_ENTER("app.case_drill_due_session");
    bt_drill_case_queue due;
    bt_drill_case_queue_init(&due);
    bt_drill_case_queue seen;
    bt_drill_case_queue_init(&seen);
    for (size_t i = 0; i < cases->count; i++) {
        const bt_card *card = bt_case_cards_get(cards, &cases->items[i].case_ref);
        if (card != NULL && !bt_card_is_due(card, now_day)) {
            bt_drill_case_queue_push(&seen, &cases->items[i]);
        } else {
            bt_drill_case_queue_push(&due, &cases->items[i]);
        }
    }
    bt_drill_case_queue *source = &due;
    if (due.count == 0) {
        source = &seen;
    }
    size_t count = source->count;
    bt_drill_case_queue_set(out, source->items, source->count);
    bt_drill_case_queue_free(&seen);
    bt_drill_case_queue_free(&due);
    BT_TRACE_EXIT("app.case_drill_due_session");
    return count;
}"""
assert old in text, "due_session block"
text = text.replace(old, new, 1)

old = """int bt_case_drill_request(const bt_repo_browser *browser, const bt_overrides *store,
                          const bt_case_card_store *cards, long long now_day,
                          bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX], size_t *out_count) {
    BT_TRACE_ENTER("app.case_drill_request");
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node == NULL) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    size_t all = bt_case_drill_collect_cases(browser, store, out);
    if (all == 0) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    if (node->kind == BT_REPO_CASE) {
        *out_count = all;
        BT_TRACE_EXIT("app.case_drill_request");
        return 1;
    }
    *out_count = bt_case_drill_due_session(out, all, &cards->cards, now_day, out);
    BT_TRACE_EXIT("app.case_drill_request");
    return 1;
}"""
new = """int bt_case_drill_request(const bt_repo_browser *browser, const bt_overrides *store,
                          const bt_case_card_store *cards, long long now_day,
                          bt_drill_case_queue *out) {
    BT_TRACE_ENTER("app.case_drill_request");
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node == NULL) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    size_t all = bt_case_drill_collect_cases(browser, store, out);
    if (all == 0) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    if (node->kind != BT_REPO_CASE) {
        bt_case_drill_due_session(out, &cards->cards, now_day, out);
    }
    BT_TRACE_EXIT("app.case_drill_request");
    return 1;
}"""
assert old in text, "request block"
text = text.replace(old, new, 1)

old = """void bt_case_drill_install(bt_case_drill_session *session, const bt_drill_case *queue,
                           size_t count) {
    BT_TRACE_ENTER("app.case_drill_install");
    session->total = count;
    session->done = 0;
    if (count > BT_CASE_DRILL_QUEUE_MAX) {
        count = BT_CASE_DRILL_QUEUE_MAX;
    }
    memcpy(session->queue.items, queue, count * sizeof *queue);
    session->queue.count = count;
    session->has_save_error = 0;
    session->save_error[0] = '\\0';
    bt_case_drill_advance(session);
    BT_TRACE_EXIT("app.case_drill_install");
}"""
new = """void bt_case_drill_install(bt_case_drill_session *session, const bt_drill_case_queue *queue) {
    BT_TRACE_ENTER("app.case_drill_install");
    session->total = queue->count;
    session->done = 0;
    bt_drill_case_queue_set(&session->queue, queue->items, queue->count);
    session->has_save_error = 0;
    session->save_error[0] = '\\0';
    bt_case_drill_advance(session);
    BT_TRACE_EXIT("app.case_drill_install");
}"""
assert old in text, "install block"
text = text.replace(old, new, 1)

old = """void bt_view_case_drill_app_init(bt_view_case_drill_app_state *app) {
    BT_TRACE_ENTER("app.case_drill_app_init");
    memset(app, 0, sizeof *app);
    bt_case_cards_init(&app->cards.cards);
    app->initialized = 1;
    BT_TRACE_EXIT("app.case_drill_app_init");
}"""
new = """void bt_drill_case_queue_init(bt_drill_case_queue *queue) {
    BT_TRACE_ENTER("app.case_drill_queue_init");
    queue->items = NULL;
    queue->count = 0;
    queue->cap = 0;
    BT_TRACE_EXIT("app.case_drill_queue_init");
}

void bt_drill_case_queue_free(bt_drill_case_queue *queue) {
    BT_TRACE_ENTER("app.case_drill_queue_free");
    free(queue->items);
    queue->items = NULL;
    queue->count = 0;
    queue->cap = 0;
    BT_TRACE_EXIT("app.case_drill_queue_free");
}

int bt_drill_case_queue_push(bt_drill_case_queue *queue, const bt_drill_case *case_drill) {
    if (queue->count == queue->cap) {
        size_t cap = queue->cap == 0 ? 16 : queue->cap * 2;
        bt_drill_case *grown = realloc(queue->items, cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.case_drill_queue_push", "out of memory");
            return -1;
        }
        queue->items = grown;
        queue->cap = cap;
    }
    queue->items[queue->count++] = *case_drill;
    return 0;
}

int bt_drill_case_queue_set(bt_drill_case_queue *queue, const bt_drill_case *cases,
                            size_t count) {
    if (count > queue->cap) {
        size_t cap = queue->cap == 0 ? 16 : queue->cap;
        while (cap < count) {
            cap *= 2;
        }
        bt_drill_case *grown = realloc(queue->items, cap * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.case_drill_queue_set", "out of memory");
            return -1;
        }
        queue->items = grown;
        queue->cap = cap;
    }
    memcpy(queue->items, cases, count * sizeof *cases);
    queue->count = count;
    return 0;
}

void bt_view_case_drill_app_init(bt_view_case_drill_app_state *app) {
    BT_TRACE_ENTER("app.case_drill_app_init");
    memset(app, 0, sizeof *app);
    bt_case_cards_init(&app->cards.cards);
    bt_drill_case_queue_init(&app->pending);
    bt_drill_case_queue_init(&app->session.queue);
    app->initialized = 1;
    BT_TRACE_EXIT("app.case_drill_app_init");
}"""
assert old in text, "app init block"
text = text.replace(old, new, 1)

old = """void bt_view_case_drill_set_pending(const bt_drill_case *queue, size_t count) {
    BT_TRACE_ENTER("app.case_drill_set_pending");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    if (app == NULL || count > BT_CASE_DRILL_QUEUE_MAX) {
        BT_TRACE_EXIT("app.case_drill_set_pending");
        return;
    }
    memcpy(app->pending.items, queue, count * sizeof *queue);
    app->pending.count = count;
    BT_TRACE_EXIT("app.case_drill_set_pending");
}"""
new = """void bt_view_case_drill_set_pending(const bt_drill_case_queue *queue) {
    BT_TRACE_ENTER("app.case_drill_set_pending");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    if (app == NULL) {
        BT_TRACE_EXIT("app.case_drill_set_pending");
        return;
    }
    bt_drill_case_queue_set(&app->pending, queue->items, queue->count);
    BT_TRACE_EXIT("app.case_drill_set_pending");
}"""
assert old in text, "set_pending block"
text = text.replace(old, new, 1)

old = """    bt_case_drill_install(&app->session, app->pending.items, app->pending.count);
    app->pending.count = 0;"""
new = """    bt_case_drill_install(&app->session, &app->pending);
    app->pending.count = 0;"""
assert old in text, "enter install call"
text = text.replace(old, new, 1)

p.write_text(text, encoding="utf-8")
print("case_drill.c reworked")
