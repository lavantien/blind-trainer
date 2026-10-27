#include "mutants-gate/lib.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/strbuf.h"

#define BT_GATE_EMPTY_SENTINEL "(empty)"
#define BT_GATE_MAX_UINT_DOUBLE 9007199254740992.0

typedef struct {
    char *file;
    char *function;
    char *genre;
    char *replacement;
} bt_gate_key;

typedef struct {
    bt_gate_key key;
    unsigned long long count;
} bt_gate_entry;

typedef struct {
    bt_gate_entry *items;
    size_t count;
    size_t cap;
} bt_gate_entries;

static bt_strbuf g_gate_error;
static int g_gate_error_ready;

const char *bt_gate_last_error(void) {
    if (!g_gate_error_ready) {
        if (bt_strbuf_init(&g_gate_error, 64) != 0) {
            return "";
        }
        g_gate_error_ready = 1;
    }
    return g_gate_error.data;
}

static void set_error(const char *fmt, ...) {
    if (!g_gate_error_ready) {
        if (bt_strbuf_init(&g_gate_error, 64) != 0) {
            return;
        }
        g_gate_error_ready = 1;
    }
    g_gate_error.len = 0;
    g_gate_error.data[0] = '\0';
    va_list ap;
    va_start(ap, fmt);
    char formatted[8192];
    vsnprintf(formatted, sizeof formatted, fmt, ap);
    va_end(ap);
    bt_strbuf_append_str(&g_gate_error, formatted);
}

static char *copy_text(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static int json_uint(const bt_json_value *value, unsigned long long *out) {
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return -1;
    }
    double number = value->as.number;
    if (number < 0 || number > BT_GATE_MAX_UINT_DOUBLE || number != (double)(long long)number) {
        return -1;
    }
    *out = (unsigned long long)number;
    return 0;
}

static int json_string(const bt_json_value *container, const char *name, const char **out) {
    const bt_json_value *value = bt_json_get(container, name);
    if (value == NULL || value->type != BT_JSON_STRING) {
        return -1;
    }
    *out = value->as.string;
    return 0;
}

static void entries_free(bt_gate_entries *entries) {
    for (size_t i = 0; i < entries->count; i++) {
        free(entries->items[i].key.file);
        free(entries->items[i].key.function);
        free(entries->items[i].key.genre);
        free(entries->items[i].key.replacement);
    }
    free(entries->items);
    entries->items = NULL;
    entries->count = 0;
    entries->cap = 0;
}

static int key_init(bt_gate_key *key, const char *file, const char *function, const char *genre,
                    const char *replacement) {
    key->file = copy_text(file);
    key->function = copy_text(function);
    key->genre = copy_text(genre);
    key->replacement = copy_text(replacement);
    if (key->file == NULL || key->function == NULL || key->genre == NULL ||
        key->replacement == NULL) {
        free(key->file);
        free(key->function);
        free(key->genre);
        free(key->replacement);
        key->file = NULL;
        key->function = NULL;
        key->genre = NULL;
        key->replacement = NULL;
        return -1;
    }
    return 0;
}

static int key_cmp(const bt_gate_key *a, const bt_gate_key *b) {
    int rc = strcmp(a->file, b->file);
    if (rc != 0) {
        return rc;
    }
    rc = strcmp(a->function, b->function);
    if (rc != 0) {
        return rc;
    }
    rc = strcmp(a->genre, b->genre);
    if (rc != 0) {
        return rc;
    }
    return strcmp(a->replacement, b->replacement);
}

static int entry_cmp_qsort(const void *left, const void *right) {
    return key_cmp(&((const bt_gate_entry *)left)->key, &((const bt_gate_entry *)right)->key);
}

static bt_gate_entry *entries_find(bt_gate_entries *entries, const bt_gate_key *key) {
    for (size_t i = 0; i < entries->count; i++) {
        if (key_cmp(&entries->items[i].key, key) == 0) {
            return &entries->items[i];
        }
    }
    return NULL;
}

static bt_gate_entry *entries_upsert(bt_gate_entries *entries, const bt_gate_key *key,
                                     unsigned long long initial) {
    bt_gate_entry *found = entries_find(entries, key);
    if (found != NULL) {
        found->count += initial;
        return found;
    }
    if (entries->count == entries->cap) {
        size_t want = entries->cap == 0 ? 8 : entries->cap * 2;
        bt_gate_entry *grown = realloc(entries->items, want * sizeof(bt_gate_entry));
        if (grown == NULL) {
            return NULL;
        }
        entries->items = grown;
        entries->cap = want;
    }
    bt_gate_entry *slot = &entries->items[entries->count];
    if (key_init(&slot->key, key->file, key->function, key->genre, key->replacement) != 0) {
        return NULL;
    }
    slot->count = initial;
    entries->count++;
    return slot;
}

static void key_display(bt_strbuf *out, const bt_gate_key *key) {
    bt_strbuf_appendf(out, "file=%s function=%s genre=%s replacement=%s", key->file, key->function,
                      key->genre, key->replacement);
}

static int parse_mutant_key(const bt_json_value *mutant, bt_gate_key *key) {
    const char *file = NULL;
    if (json_string(mutant, "file", &file) != 0) {
        set_error("malformed outcome: missing string field 'file'");
        return BT_GATE_VIOLATION;
    }
    const bt_json_value *function = bt_json_get(mutant, "function");
    const char *function_name = NULL;
    if (function == NULL) {
        set_error("malformed outcome: mutant without function object");
        return BT_GATE_VIOLATION;
    }
    if (function->type == BT_JSON_NULL) {
        function_name = "";
    } else if (function->type == BT_JSON_OBJECT) {
        if (json_string(function, "function_name", &function_name) != 0) {
            set_error("malformed outcome: missing string field 'function_name'");
            return BT_GATE_VIOLATION;
        }
    } else {
        set_error("malformed outcome: mutant without function object");
        return BT_GATE_VIOLATION;
    }
    const char *genre = NULL;
    if (json_string(mutant, "genre", &genre) != 0) {
        set_error("malformed outcome: missing string field 'genre'");
        return BT_GATE_VIOLATION;
    }
    const char *replacement = NULL;
    if (json_string(mutant, "replacement", &replacement) != 0) {
        set_error("malformed outcome: missing string field 'replacement'");
        return BT_GATE_VIOLATION;
    }
    if (key_init(key, file, function_name, genre, replacement) != 0) {
        set_error("out of memory");
        return BT_GATE_VIOLATION;
    }
    return BT_GATE_OK;
}

static int uint_counter(const bt_json_value *root, const char *name, unsigned long long *out) {
    if (json_uint(bt_json_get(root, name), out) != 0) {
        set_error("malformed outcomes file: missing integer counter '%s'", name);
        return BT_GATE_VIOLATION;
    }
    return BT_GATE_OK;
}

static int survivor_multiset(const char *json, bt_gate_entries *survivors,
                             unsigned long long counters[5]) {
    bt_json_value *root = NULL;
    bt_json_error error;
    if (bt_json_parse(json, strlen(json), &root, &error) != 0) {
        set_error("invalid outcomes JSON: %s", error.message);
        return BT_GATE_USAGE;
    }
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    if (outcomes == NULL || outcomes->type != BT_JSON_ARRAY) {
        bt_json_free(root);
        set_error("malformed outcomes file: missing outcomes array");
        return BT_GATE_VIOLATION;
    }
    unsigned long long missed = 0;
    unsigned long long timeout = 0;
    unsigned long long caught = 0;
    unsigned long long unviable = 0;
    unsigned long long mutants = 0;
    int rc = BT_GATE_OK;
    for (size_t i = 0; i < outcomes->as.array.count && rc == BT_GATE_OK; i++) {
        const bt_json_value *outcome = outcomes->as.array.items[i];
        const bt_json_value *scenario = bt_json_get(outcome, "scenario");
        if (scenario == NULL) {
            set_error("malformed outcome: missing scenario");
            rc = BT_GATE_VIOLATION;
            break;
        }
        const char *summary = NULL;
        if (json_string(outcome, "summary", &summary) != 0) {
            set_error("malformed outcome: missing summary string");
            rc = BT_GATE_VIOLATION;
            break;
        }
        if (scenario->type == BT_JSON_STRING) {
            if (strcmp(scenario->as.string, "Baseline") == 0) {
                if (strcmp(summary, "Success") != 0) {
                    set_error("baseline summary is '%s', not Success", summary);
                    rc = BT_GATE_VIOLATION;
                }
                continue;
            }
            set_error("malformed outcome: scenario string '%s' is not Baseline",
                      scenario->as.string);
            rc = BT_GATE_VIOLATION;
            break;
        }
        const bt_json_value *mutant = bt_json_get(scenario, "Mutant");
        if (mutant == NULL || mutant->type != BT_JSON_OBJECT) {
            set_error("malformed outcome: scenario object without Mutant");
            rc = BT_GATE_VIOLATION;
            break;
        }
        bt_gate_key key;
        rc = parse_mutant_key(mutant, &key);
        if (rc != BT_GATE_OK) {
            break;
        }
        mutants++;
        if (strcmp(summary, "CaughtMutant") == 0) {
            caught++;
        } else if (strcmp(summary, "Unviable") == 0) {
            unviable++;
        } else if (strcmp(summary, "MissedMutant") == 0) {
            bt_gate_entry *slot = entries_upsert(survivors, &key, 1);
            if (slot == NULL) {
                rc = BT_GATE_VIOLATION;
                set_error("out of memory");
            } else {
                missed++;
            }
        } else if (strcmp(summary, "Timeout") == 0) {
            bt_gate_entry *slot = entries_upsert(survivors, &key, 1);
            if (slot == NULL) {
                rc = BT_GATE_VIOLATION;
                set_error("out of memory");
            } else {
                timeout++;
            }
        } else {
            set_error("unknown summary '%s' for file=%s function=%s genre=%s replacement=%s "
                      "(allowed: CaughtMutant, MissedMutant, Timeout, Unviable)",
                      summary, key.file, key.function, key.genre, key.replacement);
            rc = BT_GATE_VIOLATION;
        }
        free(key.file);
        free(key.function);
        free(key.genre);
        free(key.replacement);
    }
    if (rc == BT_GATE_OK && mutants == 0) {
        set_error("no mutant outcomes present, refusing an empty run");
        rc = BT_GATE_VIOLATION;
    }
    static const char *names[5] = {"missed", "timeout", "caught", "unviable", "total_mutants"};
    unsigned long long seen[5] = {missed, timeout, caught, unviable, mutants};
    for (int i = 0; i < 5 && rc == BT_GATE_OK; i++) {
        unsigned long long declared = 0;
        if (uint_counter(root, names[i], &declared) != 0) {
            rc = BT_GATE_VIOLATION;
            break;
        }
        if (seen[i] != declared) {
            set_error("%s counter mismatch: outcomes count %llu != file counter %llu", names[i],
                      seen[i], declared);
            rc = BT_GATE_VIOLATION;
        }
    }
    counters[0] = mutants;
    counters[1] = caught;
    counters[2] = missed;
    counters[3] = timeout;
    counters[4] = unviable;
    bt_json_free(root);
    return rc;
}

static int parse_count_column(const char *raw, unsigned long long *out) {
    while (*raw == ' ' || *raw == '\t') {
        raw++;
    }
    const char *end = raw + strlen(raw);
    while (end > raw && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    if (end == raw) {
        return -1;
    }
    errno = 0;
    char *parse_end = NULL;
    unsigned long long value = strtoull(raw, &parse_end, 10);
    if (parse_end != end || errno == ERANGE) {
        return -1;
    }
    *out = value;
    return 0;
}

static int parse_allowlist(const char *text, bt_gate_entries *entries) {
    const char *cursor = text;
    size_t row = 0;
    while (cursor != NULL) {
        const char *newline = strchr(cursor, '\n');
        size_t line_len = newline == NULL ? strlen(cursor) : (size_t)(newline - cursor);
        row++;
        char line[4096];
        if (line_len >= sizeof line) {
            set_error("allowlist row %zu: line too long", row);
            return BT_GATE_VIOLATION;
        }
        memcpy(line, cursor, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }
        cursor = newline == NULL ? NULL : newline + 1;
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        size_t tabs = 0;
        for (const char *scan = line; *scan != '\0'; scan++) {
            if (*scan == '\t') {
                tabs++;
            }
        }
        if (tabs != 5) {
            set_error("allowlist row %zu: expected 6 tab-separated columns, found %zu", row,
                      tabs + 1);
            return BT_GATE_VIOLATION;
        }
        char *cols[6];
        cols[0] = line;
        size_t col = 1;
        for (char *scan = line; *scan != '\0'; scan++) {
            if (*scan == '\t') {
                *scan = '\0';
                cols[col++] = scan + 1;
            }
        }
        if (cols[0][0] == '\0' || cols[2][0] == '\0') {
            set_error("allowlist row %zu: file and genre must be non-empty", row);
            return BT_GATE_VIOLATION;
        }
        if (cols[1][0] == '\0') {
            set_error("allowlist row %zu: function must be non-empty, write (empty) for a "
                      "module-level mutant",
                      row);
            return BT_GATE_VIOLATION;
        }
        const char *function = strcmp(cols[1], BT_GATE_EMPTY_SENTINEL) == 0 ? "" : cols[1];
        const char *replacement = NULL;
        const char *arrow = NULL;
        for (const char *scan = cols[3]; scan[0] != '\0'; scan++) {
            if (scan[0] == '-' && scan[1] == '>') {
                arrow = scan;
            }
        }
        if (strcmp(cols[3], BT_GATE_EMPTY_SENTINEL) == 0) {
            replacement = "";
        } else if (arrow != NULL) {
            const char *token = arrow + 2;
            if (*token == '\0') {
                set_error("allowlist row %zu: replacement must have a non-empty token after '->'",
                          row);
                return BT_GATE_VIOLATION;
            }
            replacement = token;
        } else if (cols[3][0] == '\0') {
            set_error("allowlist row %zu: replacement must be non-empty, write (empty) for a "
                      "deleted token",
                      row);
            return BT_GATE_VIOLATION;
        } else {
            replacement = cols[3];
        }
        unsigned long long count = 0;
        if (parse_count_column(cols[4], &count) != 0) {
            set_error("allowlist row %zu: count '%s' is not an integer", row, cols[4]);
            return BT_GATE_VIOLATION;
        }
        if (count < 1) {
            set_error("allowlist row %zu: count must be >= 1", row);
            return BT_GATE_VIOLATION;
        }
        if (cols[5][0] == '\0') {
            set_error("allowlist row %zu: justification must be non-empty", row);
            return BT_GATE_VIOLATION;
        }
        bt_gate_key key;
        if (key_init(&key, cols[0], function, cols[2], replacement) != 0) {
            set_error("out of memory");
            return BT_GATE_VIOLATION;
        }
        if (entries_find(entries, &key) != NULL) {
            set_error("allowlist row %zu: duplicate entry for file=%s function=%s genre=%s "
                      "replacement=%s",
                      row, key.file, key.function, key.genre, key.replacement);
            free(key.file);
            free(key.function);
            free(key.genre);
            free(key.replacement);
            return BT_GATE_VIOLATION;
        }
        if (entries_upsert(entries, &key, count) == NULL) {
            set_error("out of memory");
            free(key.file);
            free(key.function);
            free(key.genre);
            free(key.replacement);
            return BT_GATE_VIOLATION;
        }
        free(key.file);
        free(key.function);
        free(key.genre);
        free(key.replacement);
    }
    return BT_GATE_OK;
}

int bt_gate_run(const char *outcomes_json, const char *allowlist_text, char *summary,
                size_t summary_cap) {
    BT_TRACE_ENTER("mutants-gate.run");
    bt_gate_entries survivors = {0};
    unsigned long long counters[5] = {0};
    int rc = survivor_multiset(outcomes_json, &survivors, counters);
    bt_gate_entries allowlist = {0};
    if (rc == BT_GATE_OK) {
        rc = parse_allowlist(allowlist_text, &allowlist);
    }
    if (rc == BT_GATE_OK) {
        qsort(survivors.items, survivors.count, sizeof(bt_gate_entry), entry_cmp_qsort);
        qsort(allowlist.items, allowlist.count, sizeof(bt_gate_entry), entry_cmp_qsort);
        bt_strbuf problems;
        if (bt_strbuf_init(&problems, 256) != 0) {
            set_error("out of memory");
            rc = BT_GATE_VIOLATION;
        } else {
            unsigned long long gated = 0;
            for (size_t i = 0; i < survivors.count; i++) {
                bt_gate_entry *survivor = &survivors.items[i];
                gated += survivor->count;
                bt_gate_entry *allowed = entries_find(&allowlist, &survivor->key);
                if (allowed == NULL) {
                    if (problems.len > 0) {
                        bt_strbuf_append_str(&problems, "\n");
                    }
                    bt_strbuf_append_str(&problems, "surviving mutant not in allowlist: ");
                    key_display(&problems, &survivor->key);
                } else if (allowed->count != survivor->count) {
                    if (problems.len > 0) {
                        bt_strbuf_append_str(&problems, "\n");
                    }
                    bt_strbuf_appendf(&problems, "count mismatch: ");
                    key_display(&problems, &survivor->key);
                    bt_strbuf_appendf(&problems, " outcomes=%llu allowlist=%llu", survivor->count,
                                      allowed->count);
                }
            }
            for (size_t i = 0; i < allowlist.count; i++) {
                if (entries_find(&survivors, &allowlist.items[i].key) == NULL) {
                    if (problems.len > 0) {
                        bt_strbuf_append_str(&problems, "\n");
                    }
                    bt_strbuf_append_str(&problems,
                                         "stale allowlist entry with no surviving outcome: ");
                    key_display(&problems, &allowlist.items[i].key);
                }
            }
            if (problems.len > 0) {
                set_error("%s", problems.data);
                rc = BT_GATE_VIOLATION;
            } else {
                snprintf(summary, summary_cap, "survivors gated: %llu, all covered", gated);
            }
            bt_strbuf_free(&problems);
        }
    }
    entries_free(&survivors);
    entries_free(&allowlist);
    if (rc != BT_GATE_OK) {
        BT_TRACE_FAIL("mutants-gate.run", "verdict %d: %s", rc, bt_gate_last_error());
    }
    BT_TRACE_EXIT("mutants-gate.run");
    return rc;
}

static int check_complete_run(const bt_json_value *root, size_t input) {
    const bt_json_value *end_time_value = bt_json_get(root, "end_time");
    if (end_time_value == NULL || end_time_value->type != BT_JSON_STRING) {
        set_error("input %zu has no end_time string, refusing a killed or partial run", input);
        return BT_GATE_VIOLATION;
    }
    if (end_time_value->as.string[0] == '\0') {
        set_error("input %zu has an empty end_time, refusing a killed or partial run", input);
        return BT_GATE_VIOLATION;
    }
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    if (outcomes == NULL || outcomes->type != BT_JSON_ARRAY) {
        set_error("malformed outcomes file: input %zu is missing the outcomes array", input);
        return BT_GATE_VIOLATION;
    }
    static const char *summary_names[4] = {"CaughtMutant", "Unviable", "MissedMutant", "Timeout"};
    static const char *counter_names[4] = {"caught", "unviable", "missed", "timeout"};
    unsigned long long seen[4] = {0};
    unsigned long long mutants = 0;
    for (size_t i = 0; i < outcomes->as.array.count; i++) {
        const bt_json_value *outcome = outcomes->as.array.items[i];
        const char *summary = NULL;
        if (json_string(outcome, "summary", &summary) != 0) {
            set_error("malformed outcome: missing summary string");
            return BT_GATE_VIOLATION;
        }
        const bt_json_value *scenario = bt_json_get(outcome, "scenario");
        if (scenario != NULL && scenario->type == BT_JSON_STRING &&
            strcmp(scenario->as.string, "Baseline") == 0) {
            if (strcmp(summary, "Success") != 0) {
                set_error("input %zu baseline summary is '%s', not Success", input, summary);
                return BT_GATE_VIOLATION;
            }
            continue;
        }
        const bt_json_value *mutant = bt_json_get(scenario, "Mutant");
        if (scenario == NULL || scenario->type != BT_JSON_OBJECT || mutant == NULL) {
            set_error("malformed outcome: scenario object without Mutant");
            return BT_GATE_VIOLATION;
        }
        mutants++;
        int matched = 0;
        for (int c = 0; c < 4; c++) {
            if (strcmp(summary, summary_names[c]) == 0) {
                seen[c]++;
                matched = 1;
                break;
            }
        }
        if (!matched) {
            set_error("unknown summary '%s' in input %zu", summary, input);
            return BT_GATE_VIOLATION;
        }
    }
    if (mutants == 0) {
        set_error("input %zu has no mutant outcomes, refusing an empty run", input);
        return BT_GATE_VIOLATION;
    }
    for (int c = 0; c < 4; c++) {
        unsigned long long declared = 0;
        if (uint_counter(root, counter_names[c], &declared) != 0) {
            return BT_GATE_VIOLATION;
        }
        if (seen[c] != declared) {
            set_error("input %zu counter mismatch for %s: outcomes count %llu != file counter %llu",
                      input, summary_names[c], seen[c], declared);
            return BT_GATE_VIOLATION;
        }
    }
    unsigned long long declared_total = 0;
    if (uint_counter(root, "total_mutants", &declared_total) != 0) {
        return BT_GATE_VIOLATION;
    }
    if (mutants != declared_total) {
        set_error("input %zu total_mutants mismatch: outcomes count %llu != file counter %llu",
                  input, mutants, declared_total);
        return BT_GATE_VIOLATION;
    }
    return BT_GATE_OK;
}

int bt_gate_merge(const char *const *inputs, size_t input_count, char **json_out,
                  unsigned long long *total_mutants) {
    BT_TRACE_ENTER("mutants-gate.merge");
    *json_out = NULL;
    *total_mutants = 0;
    if (input_count < 2) {
        set_error("merge needs at least two input files");
        BT_TRACE_FAIL("mutants-gate.merge", "too few inputs");
        BT_TRACE_EXIT("mutants-gate.merge");
        return BT_GATE_USAGE;
    }
    bt_json_value **roots = calloc(input_count, sizeof(bt_json_value *));
    int rc = BT_GATE_OK;
    if (roots == NULL) {
        set_error("out of memory");
        BT_TRACE_EXIT("mutants-gate.merge");
        return BT_GATE_VIOLATION;
    }
    for (size_t i = 0; i < input_count && rc == BT_GATE_OK; i++) {
        bt_json_error error;
        if (bt_json_parse(inputs[i], strlen(inputs[i]), &roots[i], &error) != 0) {
            set_error("invalid outcomes JSON in input %zu: %s", i + 1, error.message);
            rc = BT_GATE_USAGE;
            break;
        }
        rc = check_complete_run(roots[i], i + 1);
    }
    if (rc == BT_GATE_OK) {
        const char *first_version = NULL;
        for (size_t i = 0; i < input_count; i++) {
            const char *version = NULL;
            if (json_string(roots[i], "cargo_mutants_version", &version) == 0) {
                if (first_version == NULL) {
                    first_version = version;
                } else if (strcmp(version, first_version) != 0) {
                    bt_strbuf listed;
                    bt_strbuf_init(&listed, 64);
                    for (size_t j = 0; j < input_count; j++) {
                        const char *each = NULL;
                        if (json_string(roots[j], "cargo_mutants_version", &each) == 0) {
                            if (listed.len > 0) {
                                bt_strbuf_append_str(&listed, ", ");
                            }
                            bt_strbuf_appendf(&listed, "\"%s\"", each);
                        }
                    }
                    set_error("cargo_mutants_version disagreement across inputs: [%s]",
                              listed.data);
                    bt_strbuf_free(&listed);
                    rc = BT_GATE_VIOLATION;
                    break;
                }
            }
        }
    }
    bt_json_value *merged = bt_json_new_object();
    if (merged == NULL) {
        set_error("out of memory");
        rc = BT_GATE_VIOLATION;
    }
    if (rc == BT_GATE_OK) {
        static const char *sums[6] = {
            "total_mutants", "caught", "missed", "timeout", "unviable", "success",
        };
        for (int i = 0; i < 6 && rc == BT_GATE_OK; i++) {
            unsigned long long total = 0;
            for (size_t j = 0; j < input_count && rc == BT_GATE_OK; j++) {
                unsigned long long value = 0;
                if (json_uint(bt_json_get(roots[j], sums[i]), &value) != 0) {
                    set_error("malformed outcomes file: missing integer counter '%s'", sums[i]);
                    rc = BT_GATE_VIOLATION;
                } else {
                    total += value;
                }
            }
            if (rc == BT_GATE_OK) {
                bt_json_object_set(merged, sums[i], bt_json_new_number((double)total));
            }
        }
    }
    if (rc == BT_GATE_OK) {
        const char *version = NULL;
        for (size_t i = 0; i < input_count; i++) {
            if (json_string(roots[i], "cargo_mutants_version", &version) == 0) {
                bt_json_object_set(merged, "cargo_mutants_version", bt_json_new_string(version));
                break;
            }
        }
        const char *earliest = NULL;
        const char *latest = NULL;
        for (size_t i = 0; i < input_count; i++) {
            const char *stamp = NULL;
            if (json_string(roots[i], "start_time", &stamp) == 0 &&
                (earliest == NULL || strcmp(stamp, earliest) < 0)) {
                earliest = stamp;
            }
            if (json_string(roots[i], "end_time", &stamp) == 0 &&
                (latest == NULL || strcmp(stamp, latest) > 0)) {
                latest = stamp;
            }
        }
        if (earliest != NULL) {
            bt_json_object_set(merged, "start_time", bt_json_new_string(earliest));
        }
        if (latest != NULL) {
            bt_json_object_set(merged, "end_time", bt_json_new_string(latest));
        }
        bt_json_value *outcomes = bt_json_new_array();
        for (size_t i = 0; i < input_count; i++) {
            bt_json_value *source = NULL;
            for (size_t k = 0; k < roots[i]->as.object.count; k++) {
                if (strcmp(roots[i]->as.object.keys[k], "outcomes") == 0) {
                    source = roots[i]->as.object.values[k];
                    break;
                }
            }
            for (size_t j = 0; j < source->as.array.count; j++) {
                bt_json_array_push(outcomes, source->as.array.items[j]);
            }
            source->as.array.count = 0;
            source->as.array.items = NULL;
        }
        bt_json_object_set(merged, "outcomes", outcomes);
        bt_strbuf out;
        if (bt_strbuf_init(&out, 4096) != 0 || bt_json_serialize(merged, &out) != 0) {
            set_error("cannot serialize merged outcomes");
            rc = BT_GATE_USAGE;
        } else {
            *json_out = copy_text(out.data);
            if (*json_out == NULL) {
                set_error("out of memory");
                rc = BT_GATE_VIOLATION;
            } else {
                const bt_json_value *total = bt_json_get(merged, "total_mutants");
                json_uint(total, total_mutants);
            }
        }
        bt_strbuf_free(&out);
    }
    for (size_t i = 0; i < input_count; i++) {
        bt_json_free(roots[i]);
    }
    free(roots);
    bt_json_free(merged);
    if (rc != BT_GATE_OK && *json_out != NULL) {
        free(*json_out);
        *json_out = NULL;
    }
    if (rc != BT_GATE_OK) {
        BT_TRACE_FAIL("mutants-gate.merge", "verdict %d: %s", rc, bt_gate_last_error());
    }
    BT_TRACE_EXIT("mutants-gate.merge");
    return rc;
}
