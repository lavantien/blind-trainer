#include "data/commutator.h"

#include "common/bt_trace.h"
#include "core/cube.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    BT_CORE_COMM,
    BT_CORE_SUM,
} bt_core_kind;

typedef struct bt_unit {
    bool has_setup;
    bt_alg setup;
    bt_core_kind kind;
    bt_alg comm_a;
    bt_alg comm_b;
    struct bt_unit *subs;
    size_t sub_count;
} bt_unit;

static bt_alg alg_clone(const bt_alg *src) {
    bt_alg out;
    bt_alg_init(&out);
    for (size_t i = 0; i < bt_alg_len(src); i++) {
        bt_alg_push(&out, src->items[i]);
    }
    return out;
}

static bt_alg alg_concat(const bt_alg *head, const bt_alg *tail) {
    bt_alg out = alg_clone(head);
    for (size_t i = 0; i < bt_alg_len(tail); i++) {
        bt_alg_push(&out, tail->items[i]);
    }
    return out;
}

static void alg_append(bt_alg *dst, const bt_alg *src) {
    for (size_t i = 0; i < bt_alg_len(src); i++) {
        bt_alg_push(dst, src->items[i]);
    }
}

void bt_expanded_list_init(bt_expanded_list *list) {
    list->items = NULL;
    list->count = 0;
}

void bt_expanded_list_free(bt_expanded_list *list) {
    BT_TRACE_ENTER("commutator.list_free");
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].has_setup) {
            bt_alg_free(&list->items[i].setup);
        }
        bt_alg_free(&list->items[i].a);
        bt_alg_free(&list->items[i].b);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    BT_TRACE_EXIT("commutator.list_free");
}

bt_alg bt_expanded_expansion(const bt_expanded *unit) {
    BT_TRACE_ENTER("commutator.expansion");
    bt_alg out;
    bt_alg_init(&out);
    if (unit->has_setup) {
        alg_append(&out, &unit->setup);
    }
    const bt_alg *first = unit->b_first ? &unit->b : &unit->a;
    const bt_alg *second = unit->b_first ? &unit->a : &unit->b;
    alg_append(&out, first);
    alg_append(&out, second);
    bt_alg first_inv = bt_alg_inverse(first);
    bt_alg second_inv = bt_alg_inverse(second);
    alg_append(&out, &first_inv);
    alg_append(&out, &second_inv);
    bt_alg_free(&first_inv);
    bt_alg_free(&second_inv);
    if (unit->has_setup) {
        bt_alg setup_inv = bt_alg_inverse(&unit->setup);
        alg_append(&out, &setup_inv);
        bt_alg_free(&setup_inv);
    }
    BT_TRACE_EXIT("commutator.expansion");
    return out;
}

bool bt_expanded_eq(const bt_expanded *a, const bt_expanded *b) {
    if (a->has_setup != b->has_setup || a->b_first != b->b_first) {
        return false;
    }
    if (a->has_setup && !bt_alg_eq(&a->setup, &b->setup)) {
        return false;
    }
    return (bt_alg_eq(&a->a, &b->a) && bt_alg_eq(&a->b, &b->b)) != 0;
}

static void unit_free(bt_unit *unit) {
    if (unit->has_setup) {
        bt_alg_free(&unit->setup);
    }
    if (unit->kind == BT_CORE_COMM) {
        bt_alg_free(&unit->comm_a);
        bt_alg_free(&unit->comm_b);
    } else {
        for (size_t i = 0; i < unit->sub_count; i++) {
            unit_free(&unit->subs[i]);
        }
        free(unit->subs);
    }
}

static void units_free(bt_unit *units, size_t count) {
    for (size_t i = 0; i < count; i++) {
        unit_free(&units[i]);
    }
    free(units);
}

static bt_alg unit_expansion(const bt_unit *unit) {
    bt_alg out;
    bt_alg_init(&out);
    if (unit->has_setup) {
        alg_append(&out, &unit->setup);
    }
    if (unit->kind == BT_CORE_COMM) {
        alg_append(&out, &unit->comm_a);
        alg_append(&out, &unit->comm_b);
        bt_alg a_inv = bt_alg_inverse(&unit->comm_a);
        bt_alg b_inv = bt_alg_inverse(&unit->comm_b);
        alg_append(&out, &a_inv);
        alg_append(&out, &b_inv);
        bt_alg_free(&a_inv);
        bt_alg_free(&b_inv);
    } else {
        for (size_t i = 0; i < unit->sub_count; i++) {
            bt_alg sub = unit_expansion(&unit->subs[i]);
            alg_append(&out, &sub);
            bt_alg_free(&sub);
        }
    }
    if (unit->has_setup) {
        bt_alg setup_inv = bt_alg_inverse(&unit->setup);
        alg_append(&out, &setup_inv);
        bt_alg_free(&setup_inv);
    }
    return out;
}

typedef struct {
    char **parts;
    size_t count;
} bt_parts;

static void parts_free(bt_parts *parts) {
    for (size_t i = 0; i < parts->count; i++) {
        free(parts->parts[i]);
    }
    free(parts->parts);
    parts->parts = NULL;
    parts->count = 0;
}

static char *trim_dup(const char *start, size_t len) {
    size_t begin = 0;
    size_t end = len;
    while (begin < end && (start[begin] == ' ' || start[begin] == '\t' || start[begin] == '\n' ||
                           start[begin] == '\r')) {
        begin += 1;
    }
    while (end > begin && (start[end - 1] == ' ' || start[end - 1] == '\t' ||
                           start[end - 1] == '\n' || start[end - 1] == '\r')) {
        end -= 1;
    }
    size_t trimmed = end - begin;
    char *out = malloc(trimmed + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start + begin, trimmed);
    out[trimmed] = '\0';
    return out;
}

static bool split_depth_zero(const char *text, char sep, bt_parts *out) {
    out->parts = NULL;
    out->count = 0;
    size_t depth = 0;
    size_t start = 0;
    size_t len = strlen(text);
    size_t cap = 8;
    out->parts = malloc(cap * sizeof *out->parts);
    if (out->parts == NULL) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        char ch = text[i];
        if (ch == '[') {
            depth += 1;
            continue;
        }
        if (ch == ']') {
            if (depth == 0) {
                parts_free(out);
                return false;
            }
            depth -= 1;
            continue;
        }
        if (ch == sep && depth == 0) {
            if (out->count == cap) {
                cap *= 2;
                char **grown = realloc(out->parts, cap * sizeof *grown);
                if (grown == NULL) {
                    parts_free(out);
                    return false;
                }
                out->parts = grown;
            }
            out->parts[out->count] = trim_dup(text + start, i - start);
            if (out->parts[out->count] == NULL) {
                parts_free(out);
                return false;
            }
            out->count += 1;
            start = i + 1;
        }
    }
    if (depth != 0) {
        parts_free(out);
        return false;
    }
    if (out->count == cap) {
        char **grown = realloc(out->parts, (cap + 1) * sizeof *grown);
        if (grown == NULL) {
            parts_free(out);
            return false;
        }
        out->parts = grown;
    }
    out->parts[out->count] = trim_dup(text + start, len - start);
    if (out->parts[out->count] == NULL) {
        parts_free(out);
        return false;
    }
    out->count += 1;
    return true;
}

static bool parse_expr(const char *text, unsigned depth, bt_unit **out_units, size_t *out_count);

static bool parse_core(const char *text, unsigned depth, bt_unit *unit);

static bool parse_slot(const char *text, unsigned depth, bt_alg *out) {
    bt_alg_init(out);
    if (strchr(text, '[') != NULL) {
        if (depth >= BT_COMMUTATOR_MAX_DEPTH) {
            return false;
        }
        bt_unit *units = NULL;
        size_t count = 0;
        bool ok = parse_expr(text, depth + 1, &units, &count);
        if (!ok) {
            units_free(units, count);
            return false;
        }
        for (size_t i = 0; i < count; i++) {
            bt_alg sub = unit_expansion(&units[i]);
            alg_append(out, &sub);
            bt_alg_free(&sub);
        }
        units_free(units, count);
        if (bt_alg_len(out) == 0) {
            bt_alg_free(out);
            bt_alg_init(out);
            return false;
        }
        return true;
    }
    char err[64];
    if (!bt_alg_parse(text, out, err, sizeof err)) {
        bt_alg_free(out);
        bt_alg_init(out);
        return false;
    }
    if (bt_alg_len(out) == 0) {
        bt_alg_free(out);
        bt_alg_init(out);
        return false;
    }
    return true;
}

static bool parse_unit(const char *text, unsigned depth, bt_unit *unit) {
    if (text[0] == '\0') {
        return false;
    }
    bt_parts parts;
    if (!split_depth_zero(text, ':', &parts)) {
        return false;
    }
    if (parts.count > 2) {
        parts_free(&parts);
        return false;
    }
    memset(unit, 0, sizeof *unit);
    bt_alg_init(&unit->setup);
    bt_alg_init(&unit->comm_a);
    bt_alg_init(&unit->comm_b);
    const char *core_text = parts.count == 2 ? parts.parts[1] : parts.parts[0];
    if (parts.count == 2) {
        char err[64];
        if (!bt_alg_parse(parts.parts[0], &unit->setup, err, sizeof err)) {
            parts_free(&parts);
            return false;
        }
        if (bt_alg_len(&unit->setup) == 0) {
            parts_free(&parts);
            return false;
        }
        unit->has_setup = true;
    }
    char *core_copy = malloc(strlen(core_text) + 1);
    if (core_copy == NULL) {
        parts_free(&parts);
        return false;
    }
    strcpy(core_copy, core_text);
    parts_free(&parts);
    bool ok = parse_core(core_copy, depth, unit);
    free(core_copy);
    return ok;
}

static bool parse_core(const char *text, unsigned depth, bt_unit *unit) {
    size_t len = strlen(text);
    if (len < 2 || text[0] != '[' || text[len - 1] != ']') {
        return false;
    }
    char *content = trim_dup(text + 1, len - 2);
    if (content == NULL) {
        return false;
    }
    bt_parts parts;
    bool ok = split_depth_zero(content, ',', &parts);
    free(content);
    if (!ok) {
        return false;
    }
    if (parts.count == 2) {
        unit->kind = BT_CORE_COMM;
        bool slot_a = parse_slot(parts.parts[0], depth, &unit->comm_a);
        bool slot_b = (slot_a && parse_slot(parts.parts[1], depth, &unit->comm_b)) != 0;
        parts_free(&parts);
        if (!slot_b) {
            bt_alg_free(&unit->comm_a);
            bt_alg_free(&unit->comm_b);
            bt_alg_init(&unit->comm_a);
            bt_alg_init(&unit->comm_b);
            return false;
        }
        return true;
    }
    if (parts.count == 1) {
        bool has_bracket = strchr(parts.parts[0], '[') != NULL;
        char *inner = malloc(strlen(parts.parts[0]) + 1);
        if (inner == NULL) {
            parts_free(&parts);
            return false;
        }
        strcpy(inner, parts.parts[0]);
        parts_free(&parts);
        if (!has_bracket || depth >= BT_COMMUTATOR_MAX_DEPTH) {
            free(inner);
            return false;
        }
        unit->kind = BT_CORE_SUM;
        bool sub_ok = parse_expr(inner, depth + 1, &unit->subs, &unit->sub_count);
        free(inner);
        if (!sub_ok) {
            unit->subs = NULL;
            unit->sub_count = 0;
            return false;
        }
        return true;
    }
    parts_free(&parts);
    return false;
}

static bool parse_expr(const char *text, unsigned depth, bt_unit **out_units, size_t *out_count) {
    bt_parts parts;
    if (!split_depth_zero(text, '+', &parts)) {
        return false;
    }
    size_t count = parts.count;
    bt_unit *units = calloc(count, sizeof *units);
    if (units == NULL) {
        parts_free(&parts);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!parse_unit(parts.parts[i], depth, &units[i])) {
            units_free(units, i + 1);
            parts_free(&parts);
            return false;
        }
    }
    parts_free(&parts);
    *out_units = units;
    *out_count = count;
    return true;
}

static bool pick_orders(const bt_expanded_list *units, size_t index, const bt_cube *cube,
                        const bt_cube *reference, bool *chosen) {
    if (index == units->count) {
        return bt_cube_eq(cube, reference);
    }
    for (int b_first = 0; b_first < 2; b_first++) {
        bt_expanded candidate;
        candidate.has_setup = units->items[index].has_setup;
        candidate.setup = units->items[index].setup;
        candidate.a = units->items[index].a;
        candidate.b = units->items[index].b;
        candidate.b_first = b_first != 0;
        bt_cube next = *cube;
        bt_alg expansion = bt_expanded_expansion(&candidate);
        bt_cube_apply_alg(&next, &expansion);
        bt_alg_free(&expansion);
        chosen[index] = b_first != 0;
        if (pick_orders(units, index + 1, &next, reference, chosen)) {
            return true;
        }
    }
    return false;
}

static bool flatten(const bt_unit *units, size_t count, const bt_alg *inherited,
                    bt_expanded_list *out) {
    for (size_t i = 0; i < count; i++) {
        const bt_unit *unit = &units[i];
        bool has_setup = false;
        bt_alg setup;
        bt_alg_init(&setup);
        if (inherited != NULL && unit->has_setup) {
            setup = alg_concat(inherited, &unit->setup);
            has_setup = true;
        } else if (inherited != NULL) {
            setup = alg_clone(inherited);
            has_setup = true;
        } else if (unit->has_setup) {
            setup = alg_clone(&unit->setup);
            has_setup = true;
        }
        if (unit->kind == BT_CORE_COMM) {
            bt_expanded *grown = realloc(out->items, (out->count + 1) * sizeof *grown);
            if (grown == NULL) {
                if (has_setup) {
                    bt_alg_free(&setup);
                }
                BT_TRACE_FAIL("commutator.flatten", "growth failed at unit %zu", i);
                return false;
            }
            out->items = grown;
            bt_expanded *slot = &out->items[out->count];
            slot->has_setup = has_setup;
            slot->setup = setup;
            slot->a = alg_clone(&unit->comm_a);
            slot->b = alg_clone(&unit->comm_b);
            slot->b_first = false;
            out->count += 1;
        } else {
            bool ok = flatten(unit->subs, unit->sub_count, has_setup ? &setup : NULL, out);
            if (has_setup) {
                bt_alg_free(&setup);
            }
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

bool bt_decompose(const char *text, bt_expanded_list *out) {
    BT_TRACE_ENTER("commutator.decompose");
    bt_expanded_list_init(out);
    char *trimmed = trim_dup(text, strlen(text));
    if (trimmed == NULL) {
        BT_TRACE_FAIL("commutator.decompose", "trim failed");
        return false;
    }
    bt_unit *units = NULL;
    size_t count = 0;
    bool parsed = parse_expr(trimmed, 0, &units, &count);
    free(trimmed);
    if (!parsed) {
        BT_TRACE_EXIT("commutator.decompose");
        return false;
    }
    bool ok = flatten(units, count, NULL, out);
    units_free(units, count);
    if (!ok) {
        bt_expanded_list_free(out);
        BT_TRACE_FAIL("commutator.decompose", "flatten failed");
        return false;
    }
    BT_TRACE_EXIT("commutator.decompose");
    return true;
}

bool bt_decompose_verified(const char *text, const bt_alg *alg, bt_expanded_list *out) {
    BT_TRACE_ENTER("commutator.decompose_verified");
    if (!bt_decompose(text, out)) {
        BT_TRACE_EXIT("commutator.decompose_verified");
        return false;
    }
    bt_cube reference = bt_cube_solved();
    bt_cube_apply_alg(&reference, alg);
    bool *chosen = calloc(out->count, sizeof *chosen);
    if (chosen == NULL) {
        bt_expanded_list_free(out);
        BT_TRACE_FAIL("commutator.decompose_verified", "chosen alloc failed");
        return false;
    }
    bt_cube start = bt_cube_solved();
    bool ok = pick_orders(out, 0, &start, &reference, chosen);
    if (ok) {
        for (size_t i = 0; i < out->count; i++) {
            out->items[i].b_first = chosen[i];
        }
    } else {
        bt_expanded_list_free(out);
    }
    free(chosen);
    BT_TRACE_EXIT("commutator.decompose_verified");
    return ok;
}
