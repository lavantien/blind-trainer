from pathlib import Path

p = Path("tests/unit/app/view_case_drill_test.c")
text = p.read_text(encoding="utf-8")

old = """    bt_overrides store;
    bt_overrides_init(&store);
    bt_drill_case cases[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_collect_cases(&browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases[0].case_ref.event, "bigcube") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.method, "yau") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.stage, "parity") == 0);
    BT_CHECK(strcmp(cases[0].case_ref.case_key, "par.1") == 0);
    BT_CHECK(cases[0].has_size);
    BT_CHECK_EQ(cases[0].size, (long long)4);
    BT_CHECK(strcmp(cases[0].alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases[0].seed_alg, "alg par.1") == 0);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    count = bt_case_drill_collect_cases(&event_browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(cases[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(cases[1].case_ref.case_key, "cll.2") == 0);
    BT_CHECK(!cases[0].has_size && !cases[1].has_size);
    bt_overrides_free(&store);"""
new = """    bt_overrides store;
    bt_overrides_init(&store);
    bt_drill_case_queue cases;
    bt_drill_case_queue_init(&cases);
    size_t count = bt_case_drill_collect_cases(&browser, &store, &cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases.items[0].case_ref.event, "bigcube") == 0);
    BT_CHECK(strcmp(cases.items[0].case_ref.method, "yau") == 0);
    BT_CHECK(strcmp(cases.items[0].case_ref.stage, "parity") == 0);
    BT_CHECK(strcmp(cases.items[0].case_ref.case_key, "par.1") == 0);
    BT_CHECK(cases.items[0].has_size);
    BT_CHECK_EQ(cases.items[0].size, (long long)4);
    BT_CHECK(strcmp(cases.items[0].alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases.items[0].seed_alg, "alg par.1") == 0);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    count = bt_case_drill_collect_cases(&event_browser, &store, &cases);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(cases.items[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(cases.items[1].case_ref.case_key, "cll.2") == 0);
    BT_CHECK(!cases.items[0].has_size && !cases.items[1].has_size);
    bt_drill_case_queue_free(&cases);
    bt_overrides_free(&store);"""
assert old in text, "walk test anchor"
text = text.replace(old, new, 1)

old = """    bt_drill_case cases[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_collect_cases(&browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases[0].seed_alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases[0].alg, "R U R'") == 0);
    bt_overrides_free(&store);"""
new = """    bt_drill_case_queue cases;
    bt_drill_case_queue_init(&cases);
    size_t count = bt_case_drill_collect_cases(&browser, &store, &cases);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(cases.items[0].seed_alg, "alg par.1") == 0);
    BT_CHECK(strcmp(cases.items[0].alg, "R U R'") == 0);
    bt_drill_case_queue_free(&cases);
    bt_overrides_free(&store);"""
assert old in text, "override test anchor"
text = text.replace(old, new, 1)

old = """    bt_overrides store;
    bt_overrides_init(&store);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&browser, &store, NULL), (size_t)0);"""
new = """    bt_overrides store;
    bt_overrides_init(&store);
    bt_drill_case_queue collected;
    bt_drill_case_queue_init(&collected);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&browser, &store, &collected), (size_t)0);"""
assert old in text, "letters null anchor"
text = text.replace(old, new, 1)

old = """    bt_repo_deeper(&bld);
    bt_repo_move(&bld, 1);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&bld, &store, NULL), (size_t)0);
    bt_str_list_free(&hidden);"""
new = """    bt_repo_deeper(&bld);
    bt_repo_move(&bld, 1);
    BT_CHECK_EQ(bt_case_drill_collect_cases(&bld, &store, &collected), (size_t)0);
    bt_drill_case_queue_free(&collected);
    bt_str_list_free(&hidden);"""
assert old in text, "bld null anchor"
text = text.replace(old, new, 1)

old = """    bt_drill_case queue_in[3] = {fresh, future, overdue};
    bt_case_cards_put(&cards, &future_ref, &scheduled);
    bt_card fresh_card;
    bt_card_init(&fresh_card, 0);
    bt_case_cards_put(&cards, &overdue_ref, &fresh_card);
    bt_drill_case queue_out[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = bt_case_drill_due_session(queue_in, 3, &cards, 1, queue_out);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(queue_out[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(queue_out[1].case_ref.case_key, "cll.3") == 0);
    bt_drill_case only_future[1] = {future};
    count = bt_case_drill_due_session(only_future, 1, &cards, 1, queue_out);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(queue_out[0].case_ref.case_key, "cll.2") == 0);
    bt_case_cards_free(&cards);"""
new = """    bt_drill_case_queue queue_in;
    bt_drill_case_queue_init(&queue_in);
    bt_drill_case_queue_push(&queue_in, &fresh);
    bt_drill_case_queue_push(&queue_in, &future);
    bt_drill_case_queue_push(&queue_in, &overdue);
    bt_case_cards_put(&cards, &future_ref, &scheduled);
    bt_card fresh_card;
    bt_card_init(&fresh_card, 0);
    bt_case_cards_put(&cards, &overdue_ref, &fresh_card);
    bt_drill_case_queue queue_out;
    bt_drill_case_queue_init(&queue_out);
    size_t count = bt_case_drill_due_session(&queue_in, &cards, 1, &queue_out);
    BT_CHECK_EQ(count, (size_t)2);
    BT_CHECK(strcmp(queue_out.items[0].case_ref.case_key, "cll.1") == 0);
    BT_CHECK(strcmp(queue_out.items[1].case_ref.case_key, "cll.3") == 0);
    bt_drill_case_queue only_future;
    bt_drill_case_queue_init(&only_future);
    bt_drill_case_queue_push(&only_future, &future);
    count = bt_case_drill_due_session(&only_future, &cards, 1, &queue_out);
    BT_CHECK_EQ(count, (size_t)1);
    BT_CHECK(strcmp(queue_out.items[0].case_ref.case_key, "cll.2") == 0);
    bt_drill_case_queue_free(&queue_out);
    bt_drill_case_queue_free(&only_future);
    bt_drill_case_queue_free(&queue_in);
    bt_case_cards_free(&cards);"""
assert old in text, "due session anchor"
text = text.replace(old, new, 1)

old = """    bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX];
    size_t count = 0;

    bt_seed_row rows[4];
    bt_repo_browser case_browser = armed(rows);
    at_bigcube_case(&case_browser);
    BT_CHECK(bt_case_drill_request(&case_browser, &store, &cards, 0, out, &count));
    BT_CHECK_EQ(count, (size_t)1);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    BT_CHECK(bt_case_drill_request(&event_browser, &store, &cards, 0, out, &count));
    BT_CHECK_EQ(count, (size_t)2);

    bt_repo_browser letters = armed(rows);
    bt_repo_deeper(&letters);
    BT_CHECK(!bt_case_drill_request(&letters, &store, &cards, 0, out, &count));

    bt_overrides_free(&store);"""
new = """    bt_drill_case_queue out;
    bt_drill_case_queue_init(&out);

    bt_seed_row rows[4];
    bt_repo_browser case_browser = armed(rows);
    at_bigcube_case(&case_browser);
    BT_CHECK(bt_case_drill_request(&case_browser, &store, &cards, 0, &out));
    BT_CHECK_EQ(out.count, (size_t)1);

    bt_repo_browser event_browser = armed(rows);
    at_2x2_event(&event_browser);
    BT_CHECK(bt_case_drill_request(&event_browser, &store, &cards, 0, &out));
    BT_CHECK_EQ(out.count, (size_t)2);

    bt_repo_browser letters = armed(rows);
    bt_repo_deeper(&letters);
    BT_CHECK(!bt_case_drill_request(&letters, &store, &cards, 0, &out));

    bt_drill_case_queue_free(&out);
    bt_overrides_free(&store);"""
assert old in text, "request anchor"
text = text.replace(old, new, 1)

old = """    queue[1] = build_case("pll.2");
    bt_case_ref_of(&queue[1].case_ref, "3x3", "cfop", "pll", "pll.2");
    snprintf(queue[1].seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    snprintf(queue[1].alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    bt_case_drill_install(&session, queue, 2);"""
new = """    queue[1] = build_case("pll.2");
    bt_case_ref_of(&queue[1].case_ref, "3x3", "cfop", "pll", "pll.2");
    snprintf(queue[1].seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    snprintf(queue[1].alg, BT_CASE_DRILL_TEXT_MAX, "%s", "QQ");
    bt_drill_case_queue pending;
    bt_drill_case_queue_init(&pending);
    bt_drill_case_queue_push(&pending, &queue[0]);
    bt_drill_case_queue_push(&pending, &queue[1]);
    bt_case_drill_install(&session, &pending);"""
assert old in text, "install anchor"
text = text.replace(old, new, 1)

old = """    bt_case_drill_advance(&session);
    BT_CHECK(!session.has_current);
    BT_CHECK(!session.has_grid);
    BT_CHECK(!session.has_grid_error);
}"""
new = """    bt_case_drill_advance(&session);
    BT_CHECK(!session.has_current);
    BT_CHECK(!session.has_grid);
    BT_CHECK(!session.has_grid_error);
    bt_drill_case_queue_free(&pending);
    bt_drill_case_queue_free(&session.queue);
}"""
assert old in text, "install tail anchor"
text = text.replace(old, new, 1)

old = """    bt_drill_case *cases = malloc(ROWS * sizeof *cases);
    size_t count = bt_case_drill_collect_cases(&browser, &store, cases);
    BT_CHECK_EQ(count, (size_t)ROWS);
    bt_case_drill_session session;
    memset(&session, 0, sizeof session);
    bt_case_drill_install(&session, cases, count);
    BT_CHECK_EQ(session.total, (size_t)ROWS);
    BT_CHECK_EQ(session.queue.count, (size_t)ROWS);
    free(cases);"""
new = """    bt_drill_case_queue cases;
    bt_drill_case_queue_init(&cases);
    size_t count = bt_case_drill_collect_cases(&browser, &store, &cases);
    BT_CHECK_EQ(count, (size_t)ROWS);
    bt_case_drill_session session;
    memset(&session, 0, sizeof session);
    bt_case_drill_install(&session, &cases);
    BT_CHECK_EQ(session.total, (size_t)ROWS);
    BT_CHECK_EQ(session.queue.count, (size_t)ROWS);
    bt_drill_case_queue_free(&cases);
    bt_drill_case_queue_free(&session.queue);"""
assert old in text, "cap test anchor"
text = text.replace(old, new, 1)

p.write_text(text, encoding="utf-8")
print("view_case_drill_test updated")
