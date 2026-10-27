#include "app/views/algs/browse.h"

#include "common/bt_trace.h"

#include <stdlib.h>
#include <string.h>

static char *dup_or_empty(const char *text) {
    const char *source = text != NULL ? text : "";
    char *out = malloc(strlen(source) + 1);
    if (out == NULL) {
        return NULL;
    }
    strcpy(out, source);
    return out;
}

static void node_set_label(bt_repo_node *node, const char *label) {
    snprintf(node->label, sizeof node->label, "%s", label);
}

static void node_push_child(bt_repo_node *node, const bt_repo_node *child) {
    bt_repo_node *grown = realloc(node->children, (node->children_count + 1) * sizeof *grown);
    if (grown == NULL) {
        BT_TRACE_FAIL("app.repo_node_push", "out of memory");
        return;
    }
    node->children = grown;
    node->children[node->children_count] = *child;
    node->children_count++;
}

void bt_repo_node_free(bt_repo_node *node) {
    BT_TRACE_ENTER("app.repo_node_free");
    for (size_t i = 0; i < node->children_count; i++) {
        bt_repo_node_free(&node->children[i]);
    }
    free(node->children);
    free(node->case_alg);
    free(node->case_source_url);
    free(node->case_source_at);
    node->children = NULL;
    node->children_count = 0;
    node->case_alg = NULL;
    node->case_source_url = NULL;
    node->case_source_at = NULL;
    BT_TRACE_EXIT("app.repo_node_free");
}

void bt_repo_browser_default(bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_browser_default");
    memset(browser, 0, sizeof *browser);
    browser->arms = malloc(sizeof *browser->arms);
    if (browser->arms == NULL) {
        BT_TRACE_FAIL("app.repo_browser_default", "out of memory");
        BT_TRACE_EXIT("app.repo_browser_default");
        return;
    }
    memset(browser->arms, 0, sizeof *browser->arms);
    node_set_label(&browser->arms[0], BT_REPO_LETTERS_LABEL);
    browser->arms[0].kind = BT_REPO_LETTERS;
    browser->arm_count = 1;
    BT_TRACE_EXIT("app.repo_browser_default");
}

void bt_repo_browser_free(bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_browser_free");
    for (size_t i = 0; i < browser->arm_count; i++) {
        bt_repo_node_free(&browser->arms[i]);
    }
    free(browser->arms);
    browser->arms = NULL;
    browser->arm_count = 0;
    BT_TRACE_EXIT("app.repo_browser_free");
}

static int hidden_contains(const bt_str_list *hidden, const char *event) {
    for (size_t i = 0; hidden != NULL && i < hidden->count; i++) {
        if (strcmp(hidden->items[i], event) == 0) {
            return 1;
        }
    }
    return 0;
}

static int size_covers(const bt_nspec *n, long long size) {
    if (n->kind == BT_NSPEC_FIXED) {
        return n->fixed == size;
    }
    if (n->kind == BT_NSPEC_SIZES) {
        for (size_t i = 0; i < n->size_count; i++) {
            if (n->sizes[i] == size) {
                return 1;
            }
        }
    }
    return 0;
}

static size_t event_sizes(const bt_pack_entry **packs, size_t count, long long *out, size_t cap) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        const bt_nspec *n = &packs[i]->n;
        if (n->kind == BT_NSPEC_FIXED) {
            long long size = n->fixed;
            int present = 0;
            for (size_t at = 0; at < total; at++) {
                present = present || out[at] == size;
            }
            if (!present && total < cap) {
                out[total++] = size;
            }
        } else if (n->kind == BT_NSPEC_SIZES) {
            for (size_t s = 0; s < n->size_count; s++) {
                long long size = n->sizes[s];
                int present = 0;
                for (size_t at = 0; at < total; at++) {
                    present = present || out[at] == size;
                }
                if (!present && total < cap) {
                    out[total++] = size;
                }
            }
        }
    }
    for (size_t i = 1; i < total; i++) {
        long long key = out[i];
        size_t at = i;
        while (at > 0 && out[at - 1] > key) {
            out[at] = out[at - 1];
            at--;
        }
        out[at] = key;
    }
    return total;
}

static bt_repo_node case_node_of(const bt_seed_row *row) {
    bt_repo_node node;
    memset(&node, 0, sizeof node);
    node_set_label(&node, row->case_key);
    node.kind = BT_REPO_CASE;
    node.has_case = 1;
    node.case_alg = dup_or_empty(row->alg);
    node.case_source_url = dup_or_empty(row->source_url);
    node.case_source_at = dup_or_empty(row->source_at);
    return node;
}

static bt_repo_node stage_node_of(const bt_pack_entry *pack, const bt_seed_row *rows,
                                  size_t row_count) {
    bt_repo_node node;
    memset(&node, 0, sizeof node);
    node_set_label(&node, pack->stage);
    node.kind = BT_REPO_STAGE;
    for (size_t i = 0; i < row_count; i++) {
        const bt_seed_row *row = &rows[i];
        if (strcmp(row->event, pack->event) == 0 && strcmp(row->method, pack->method) == 0 &&
            strcmp(row->stage, pack->stage) == 0) {
            bt_repo_node case_node = case_node_of(row);
            node_push_child(&node, &case_node);
        }
    }
    return node;
}

static bt_repo_node method_node_of(const bt_pack_entry *pack, const bt_seed_row *rows,
                                   size_t row_count) {
    bt_repo_node node;
    memset(&node, 0, sizeof node);
    node_set_label(&node, pack->method);
    node.kind = BT_REPO_METHOD;
    bt_repo_node stage = stage_node_of(pack, rows, row_count);
    node_push_child(&node, &stage);
    return node;
}

static void merge_method(bt_repo_node *target, const bt_pack_entry *pack, const bt_seed_row *rows,
                         size_t row_count) {
    for (size_t i = 0; i < target->children_count; i++) {
        if (strcmp(target->children[i].label, pack->method) == 0) {
            bt_repo_node stage = stage_node_of(pack, rows, row_count);
            node_push_child(&target->children[i], &stage);
            return;
        }
    }
    bt_repo_node method = method_node_of(pack, rows, row_count);
    node_push_child(target, &method);
}

static bt_repo_node sized_event_node(const char *event, const bt_pack_entry **packs, size_t count,
                                     const bt_seed_row *rows, size_t row_count) {
    bt_repo_node node;
    memset(&node, 0, sizeof node);
    node_set_label(&node, event);
    node.kind = BT_REPO_EVENT;
    long long sizes[BT_SEED_NSPEC_SIZES_MAX * 8];
    size_t size_total = event_sizes(packs, count, sizes, sizeof sizes / sizeof sizes[0]);
    for (size_t size = 0; size < size_total; size++) {
        bt_repo_node size_node;
        memset(&size_node, 0, sizeof size_node);
        snprintf(size_node.label, sizeof size_node.label, "%lldx%lld", sizes[size], sizes[size]);
        size_node.kind = BT_REPO_SIZE;
        for (size_t i = 0; i < count; i++) {
            if (size_covers(&packs[i]->n, sizes[size])) {
                merge_method(&size_node, packs[i], rows, row_count);
            }
        }
        node_push_child(&node, &size_node);
    }
    return node;
}

bt_repo_node *bt_repo_arms(const bt_pack_entry *packs, size_t pack_count, const bt_str_list *hidden,
                           const bt_seed_row *rows, size_t row_count, size_t *out_count) {
    BT_TRACE_ENTER("app.repo_arms");
    bt_repo_browser base;
    bt_repo_browser_default(&base);
    if (base.arms == NULL) {
        *out_count = 0;
        BT_TRACE_FAIL("app.repo_arms", "out of memory");
        BT_TRACE_EXIT("app.repo_arms");
        return NULL;
    }
    const char *order[64];
    size_t grouped_start[64];
    size_t grouped_count[64];
    size_t event_total = 0;
    for (size_t i = 0; i < pack_count; i++) {
        if (hidden_contains(hidden, packs[i].event)) {
            continue;
        }
        size_t at = 0;
        while (at < event_total && strcmp(order[at], packs[i].event) != 0) {
            at++;
        }
        if (at == event_total && event_total < 64) {
            order[event_total] = packs[i].event;
            grouped_start[event_total] = i;
            grouped_count[event_total] = 1;
            event_total++;
        } else if (at < event_total) {
            grouped_count[at]++;
        }
    }
    size_t capacity = base.arm_count;
    bt_repo_node *arms = base.arms;
    for (size_t event = 0; event < event_total; event++) {
        const bt_pack_entry *group[64];
        size_t group_count = 0;
        for (size_t i = grouped_start[event]; i < pack_count && group_count < grouped_count[event];
             i++) {
            if (strcmp(packs[i].event, order[event]) == 0 &&
                !hidden_contains(hidden, packs[i].event)) {
                group[group_count++] = &packs[i];
            }
        }
        int sized = 0;
        for (size_t i = 0; i < group_count; i++) {
            sized = sized || group[i]->n.kind != BT_NSPEC_UNSET;
        }
        bt_repo_node node;
        if (sized) {
            node = sized_event_node(order[event], group, group_count, rows, row_count);
        } else {
            memset(&node, 0, sizeof node);
            node_set_label(&node, order[event]);
            node.kind = BT_REPO_EVENT;
            for (size_t i = 0; i < group_count; i++) {
                merge_method(&node, group[i], rows, row_count);
            }
        }
        bt_repo_node *grown = realloc(arms, (capacity + 1) * sizeof *grown);
        if (grown == NULL) {
            BT_TRACE_FAIL("app.repo_arms", "out of memory");
            arms = NULL;
            capacity = 0;
            break;
        }
        arms = grown;
        arms[capacity] = node;
        capacity++;
    }
    if (arms == NULL) {
        bt_repo_browser_free(&base);
        *out_count = 0;
        BT_TRACE_EXIT("app.repo_arms");
        return NULL;
    }
    *out_count = capacity;
    BT_TRACE_EXIT("app.repo_arms");
    return arms;
}

const bt_repo_node *bt_repo_node_at(const bt_repo_node *arms, size_t arm_count, size_t arm,
                                    const size_t *path, size_t depth) {
    BT_TRACE_ENTER("app.repo_node_at");
    const bt_repo_node *node = NULL;
    if (arm >= arm_count) {
        BT_TRACE_FAIL("app.repo_node_at", "arm %zu of %zu", arm, arm_count);
        BT_TRACE_EXIT("app.repo_node_at");
        return NULL;
    }
    node = &arms[arm];
    for (size_t i = 0; i < depth; i++) {
        if (path[i] >= node->children_count) {
            BT_TRACE_FAIL("app.repo_node_at", "path %zu of %zu", path[i], node->children_count);
            BT_TRACE_EXIT("app.repo_node_at");
            return NULL;
        }
        node = &node->children[path[i]];
    }
    BT_TRACE_EXIT("app.repo_node_at");
    return node;
}

const bt_repo_node *bt_repo_browser_node(const bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_browser_node");
    const bt_repo_node *node = bt_repo_node_at(browser->arms, browser->arm_count, browser->arm,
                                               browser->path, browser->depth);
    BT_TRACE_EXIT("app.repo_browser_node");
    return node;
}

int bt_repo_browser_letters_focused(const bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_browser_letters_focused");
    int focused = !browser->repo_focused;
    BT_TRACE_EXIT("app.repo_browser_letters_focused");
    return focused;
}

int bt_repo_browser_has_seed_arms(const bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_browser_has_seed_arms");
    int has = browser->arm_count > 1;
    BT_TRACE_EXIT("app.repo_browser_has_seed_arms");
    return has;
}

void bt_repo_enter(const bt_stores *stores, bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_enter");
    bt_repo_browser_free(browser);
    browser->arms =
        bt_repo_arms(stores->seeds.packs, stores->seeds.pack_count, &stores->seeds.hidden,
                     stores->seeds.rows.items, stores->seeds.rows.count, &browser->arm_count);
    browser->arm = 0;
    browser->depth = 0;
    browser->repo_focused = 0;
    BT_TRACE_EXIT("app.repo_enter");
}

size_t bt_repo_list_at(const bt_repo_browser *browser, const bt_repo_node **out, size_t cap) {
    BT_TRACE_ENTER("app.repo_list_at");
    size_t count = 0;
    if (browser->depth == 0) {
        count = browser->arm_count < cap ? browser->arm_count : cap;
        for (size_t i = 0; i < count; i++) {
            out[i] = &browser->arms[i];
        }
        BT_TRACE_EXIT("app.repo_list_at");
        return count;
    }
    const bt_repo_node *parent = bt_repo_node_at(browser->arms, browser->arm_count, browser->arm,
                                                 browser->path, browser->depth - 1);
    if (parent == NULL) {
        BT_TRACE_EXIT("app.repo_list_at");
        return 0;
    }
    count = parent->children_count < cap ? parent->children_count : cap;
    for (size_t i = 0; i < count; i++) {
        out[i] = &parent->children[i];
    }
    BT_TRACE_EXIT("app.repo_list_at");
    return count;
}

size_t bt_repo_selection(const bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_selection");
    size_t selected = browser->depth == 0 ? browser->arm : browser->path[browser->depth - 1];
    BT_TRACE_EXIT("app.repo_selection");
    return selected;
}

void bt_repo_move(bt_repo_browser *browser, int step) {
    BT_TRACE_ENTER("app.repo_move");
    const bt_repo_node *list[BT_REPO_LIST_MAX];
    size_t len = bt_repo_list_at(browser, list, BT_REPO_LIST_MAX);
    if (len == 0) {
        BT_TRACE_EXIT("app.repo_move");
        return;
    }
    long long current = (long long)bt_repo_selection(browser);
    long long next = ((current + step) % (long long)len + (long long)len) % (long long)len;
    if (browser->depth == 0) {
        browser->arm = (size_t)next;
    } else {
        browser->path[browser->depth - 1] = (size_t)next;
    }
    BT_TRACE_EXIT("app.repo_move");
}

void bt_repo_deeper(bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_deeper");
    if (!browser->repo_focused) {
        if (bt_repo_browser_has_seed_arms(browser)) {
            browser->repo_focused = 1;
            browser->arm = 0;
            browser->depth = 0;
        }
        BT_TRACE_EXIT("app.repo_deeper");
        return;
    }
    if (browser->depth == 0) {
        const bt_repo_node *arm = NULL;
        if (browser->arm < browser->arm_count) {
            arm = &browser->arms[browser->arm];
        }
        if (arm == NULL || arm->kind == BT_REPO_LETTERS || arm->children_count == 0) {
            browser->repo_focused = 0;
            browser->depth = 0;
            BT_TRACE_EXIT("app.repo_deeper");
            return;
        }
    }
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node != NULL && node->children_count > 0 && browser->depth < BT_REPO_PATH_MAX) {
        browser->depth++;
        browser->path[browser->depth - 1] = 0;
    }
    BT_TRACE_EXIT("app.repo_deeper");
}

void bt_repo_shallower(bt_repo_browser *browser) {
    BT_TRACE_ENTER("app.repo_shallower");
    if (!browser->repo_focused) {
        BT_TRACE_EXIT("app.repo_shallower");
        return;
    }
    if (browser->depth == 0) {
        browser->repo_focused = 0;
        BT_TRACE_EXIT("app.repo_shallower");
        return;
    }
    browser->depth--;
    BT_TRACE_EXIT("app.repo_shallower");
}

size_t bt_repo_list_rows(const bt_repo_browser *browser, size_t cap, bt_repo_list_row *out) {
    BT_TRACE_ENTER("app.repo_list_rows");
    const bt_repo_node *entries[BT_REPO_LIST_MAX];
    size_t len = bt_repo_list_at(browser, entries, BT_REPO_LIST_MAX);
    if (len == 0 || cap == 0) {
        BT_TRACE_EXIT("app.repo_list_rows");
        return 0;
    }
    size_t selected = bt_repo_selection(browser);
    size_t start = bt_view_list_window(len, selected, len < cap ? len : cap).first;
    size_t count = len < cap ? len : cap;
    if (start + count > len) {
        start = len - count;
    }
    for (size_t i = 0; i < count; i++) {
        size_t index = start + i;
        snprintf(out[i].label, sizeof out[i].label, "%c %s", index == selected ? '>' : ' ',
                 entries[index]->label);
        out[i].selected = index == selected;
    }
    BT_TRACE_EXIT("app.repo_list_rows");
    return count;
}

void bt_repo_breadcrumb(const bt_repo_browser *browser, char *out, size_t out_len) {
    BT_TRACE_ENTER("app.repo_breadcrumb");
    size_t at = (size_t)snprintf(out, out_len, "repo");
    if (browser->depth > 0) {
        if (browser->arm < browser->arm_count) {
            at += (size_t)snprintf(out + at, out_len - at, " > %s",
                                   browser->arms[browser->arm].label);
        }
        for (size_t depth = 1; depth < browser->depth; depth++) {
            const bt_repo_node *node = bt_repo_node_at(browser->arms, browser->arm_count,
                                                       browser->arm, browser->path, depth);
            if (node != NULL) {
                at += (size_t)snprintf(out + at, out_len - at, " > %s", node->label);
            }
        }
    }
    BT_TRACE_EXIT("app.repo_breadcrumb");
}

size_t bt_repo_count_kind(const bt_repo_node *node, bt_repo_kind kind) {
    BT_TRACE_ENTER("app.repo_count_kind");
    size_t count = node->kind == kind ? 1 : 0;
    for (size_t i = 0; i < node->children_count; i++) {
        count += bt_repo_count_kind(&node->children[i], kind);
    }
    BT_TRACE_EXIT("app.repo_count_kind");
    return count;
}

int bt_repo_size_of_label(const char *label, long long *out) {
    BT_TRACE_ENTER("app.repo_size_of_label");
    char digits[BT_REPO_LABEL_MAX];
    size_t at = 0;
    while (label[at] != '\0' && label[at] != 'x' && at + 1 < sizeof digits) {
        if (label[at] < '0' || label[at] > '9') {
            BT_TRACE_EXIT("app.repo_size_of_label");
            return 0;
        }
        digits[at] = label[at];
        at++;
    }
    digits[at] = '\0';
    if (at == 0 || label[at] != 'x') {
        BT_TRACE_EXIT("app.repo_size_of_label");
        return 0;
    }
    *out = strtoll(digits, NULL, 10);
    BT_TRACE_EXIT("app.repo_size_of_label");
    return 1;
}

static bt_repo_browser *shared_repo_browser;

bt_repo_browser *bt_view_algs_repo(void) {
    BT_TRACE_ENTER("app.algs_repo");
    if (shared_repo_browser == NULL) {
        shared_repo_browser = malloc(sizeof *shared_repo_browser);
        if (shared_repo_browser == NULL) {
            BT_TRACE_FAIL("app.algs_repo", "out of memory");
            BT_TRACE_EXIT("app.algs_repo");
            return NULL;
        }
        bt_repo_browser_default(shared_repo_browser);
    }
    BT_TRACE_EXIT("app.algs_repo");
    return shared_repo_browser;
}
