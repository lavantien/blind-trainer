#include "mutate/mutate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"

#ifdef _WIN32
#include <windows.h>
#endif

enum {
    BT_UNION_ERROR_MAX = 512,
    BT_UNION_SUITE_MAX = 128,
};

static bt_strbuf g_union_error;
static int g_union_error_ready;

const char *bt_mut_union_last_error(void) {
    if (!g_union_error_ready) {
        if (bt_strbuf_init(&g_union_error, 64) != 0) {
            return "";
        }
        g_union_error_ready = 1;
    }
    return g_union_error.data;
}

static int union_fail(const char *fmt, ...) {
    if (!g_union_error_ready) {
        if (bt_strbuf_init(&g_union_error, 64) != 0) {
            return -1;
        }
        g_union_error_ready = 1;
    }
    g_union_error.len = 0;
    g_union_error.data[0] = '\0';
    va_list ap;
    va_start(ap, fmt);
    char formatted[BT_UNION_ERROR_MAX];
    vsnprintf(formatted, sizeof formatted, fmt, ap);
    va_end(ap);
    bt_strbuf_append_str(&g_union_error, formatted);
    return -1;
}

typedef struct {
    bt_mut_ledger ledger;
    char suite[BT_UNION_SUITE_MAX];
} bt_union_source;

typedef struct {
    const bt_mut_row *row;
    size_t source;
} bt_union_ref;

typedef struct {
    const bt_mut_row *rep;
    bt_mut_status verdict;
} bt_union_site;

typedef struct {
    bt_union_site *items;
    size_t count;
    size_t cap;
} bt_union_sites;

typedef struct {
    char file[BT_MUT_PATH_MAX];
    char *text;
    bt_mut_tokens tokens;
    int valid;
} bt_union_func_cache;

static int str_ptr_cmp(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static int expand_union_input(const char *input, bt_mut_paths *out) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(input);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return union_fail("ledger input not found: %s", input);
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if (bt_mut_paths_push(out, input) != 0) {
            return union_fail("out of memory");
        }
        return 0;
    }
    size_t base = out->count;
    char pattern[BT_MUT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/ledger*.tsv", input);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return union_fail("no ledger-*.tsv ledgers under %s", input);
    }
    int rc = 0;
    do {
        char path[BT_MUT_PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", input, entry.cFileName);
        if (bt_mut_paths_push(out, path) != 0) {
            rc = union_fail("out of memory");
            break;
        }
    } while (rc == 0 && FindNextFileA(find, &entry));
    FindClose(find);
    if (rc == 0 && out->count == base) {
        return union_fail("no ledger-*.tsv ledgers under %s", input);
    }
    if (rc == 0 && out->count > base + 1) {
        qsort(out->items + base, out->count - base, sizeof(char *), str_ptr_cmp);
    }
    return rc;
#else
    return union_fail("ledger directory scan needs windows: %s", input);
#endif
}

static void suite_from_path(const char *path, char out[BT_UNION_SUITE_MAX]) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = slash > backslash ? slash : backslash;
    base = base == NULL ? path : base + 1;
    snprintf(out, BT_UNION_SUITE_MAX, "%s", base);
    size_t len = strlen(out);
    if (len > 4 && strcmp(out + len - 4, ".tsv") == 0) {
        out[len - 4] = '\0';
        len -= 4;
    }
    if (len > 7 && strncmp(out, "ledger-", 7) == 0) {
        memmove(out, out + 7, len - 7 + 1);
    }
}

static int ref_cmp(const void *left, const void *right) {
    const bt_union_ref *a = left;
    const bt_union_ref *b = right;
    int rc = strcmp(a->row->file, b->row->file);
    if (rc != 0) {
        return rc;
    }
    if (a->row->line != b->row->line) {
        return a->row->line < b->row->line ? -1 : 1;
    }
    if (a->row->col != b->row->col) {
        return a->row->col < b->row->col ? -1 : 1;
    }
    rc = strcmp(a->row->op, b->row->op);
    if (rc != 0) {
        return rc;
    }
    rc = strcmp(a->row->from, b->row->from);
    if (rc != 0) {
        return rc;
    }
    return strcmp(a->row->to, b->row->to);
}

static int site_push(bt_union_sites *sites, const bt_union_ref *ref) {
    if (sites->count == sites->cap) {
        size_t want = sites->cap == 0 ? 16 : sites->cap * 2;
        bt_union_site *grown = realloc(sites->items, want * sizeof(bt_union_site));
        if (grown == NULL) {
            return -1;
        }
        sites->items = grown;
        sites->cap = want;
    }
    bt_union_site *slot = &sites->items[sites->count];
    slot->rep = ref->row;
    slot->verdict = BT_MUT_BUILDERR;
    sites->count++;
    return 0;
}

static void func_cache_reset(bt_union_func_cache *cache) {
    if (cache->valid) {
        bt_mut_tokens_free(&cache->tokens);
        cache->valid = 0;
    }
    free(cache->text);
    cache->text = NULL;
    cache->file[0] = '\0';
}

static void func_cache_load(bt_union_func_cache *cache, const char *repo_root, const char *file) {
    if (cache->valid && strcmp(cache->file, file) == 0) {
        return;
    }
    func_cache_reset(cache);
    snprintf(cache->file, BT_MUT_PATH_MAX, "%s", file);
    size_t len = 0;
    char path[BT_MUT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", repo_root, file);
    if (bt_mut_read_file(path, &cache->text, &len) == 0) {
        if (bt_mut_tokenize(cache->text, len, &cache->tokens) == 0) {
            cache->valid = 1;
        } else {
            free(cache->text);
            cache->text = NULL;
        }
    } else {
        cache->text = NULL;
    }
}

static void func_cache_free(bt_union_func_cache *cache) {
    func_cache_reset(cache);
}

int bt_mut_union_report(const char *const *ledger_inputs, size_t input_count, const char *repo_root,
                        const char *out_path) {
    BT_TRACE_ENTER("mutate.union_report");
    bt_mut_paths files = {0};
    bt_union_source *sources = NULL;
    bt_union_ref *refs = NULL;
    bt_union_sites sites = {0};
    bt_json_value *root = NULL;
    int rc = 0;
    if (input_count == 0) {
        rc = union_fail("no ledger inputs given");
    }
    for (size_t i = 0; i < input_count && rc == 0; i++) {
        rc = expand_union_input(ledger_inputs[i], &files);
    }
    size_t source_count = 0;
    if (rc == 0) {
        sources = calloc(files.count, sizeof(bt_union_source));
        if (sources == NULL) {
            rc = union_fail("out of memory");
        }
    }
    size_t row_total = 0;
    for (size_t i = 0; i < files.count && rc == 0; i++) {
        if (bt_mut_ledger_load(files.items[i], &sources[i].ledger) != 0) {
            rc = union_fail("malformed ledger %s", files.items[i]);
            break;
        }
        suite_from_path(files.items[i], sources[i].suite);
        source_count++;
        row_total += sources[i].ledger.count;
    }
    if (rc == 0 && row_total == 0) {
        rc = union_fail("no ledger rows across %zu ledgers", files.count);
    }
    if (rc == 0) {
        refs = malloc(row_total * sizeof(bt_union_ref));
        if (refs == NULL) {
            rc = union_fail("out of memory");
        }
    }
    if (rc == 0) {
        size_t cursor = 0;
        for (size_t i = 0; i < source_count; i++) {
            for (size_t r = 0; r < sources[i].ledger.count; r++) {
                refs[cursor].row = &sources[i].ledger.rows[r];
                refs[cursor].source = i;
                cursor++;
            }
        }
        qsort(refs, row_total, sizeof(bt_union_ref), ref_cmp);
        for (size_t start = 0; start < row_total && rc == 0;) {
            size_t end = start + 1;
            int killed = 0;
            int timed_out = 0;
            int survived = 0;
            for (size_t i = start; i < row_total && rc == 0; i++) {
                const bt_mut_row *row = refs[i].row;
                if (i > start && ref_cmp(&refs[start], &refs[i]) != 0) {
                    end = i;
                    break;
                }
                if (row->status == BT_MUT_PENDING) {
                    rc = union_fail("pending site %s:%zu:%zu %s %s->%s in suite %s", row->file,
                                    row->line, row->col, row->op, row->from, row->to,
                                    sources[refs[i].source].suite);
                    break;
                }
                if (row->status == BT_MUT_KILLED) {
                    killed = 1;
                } else if (row->status == BT_MUT_TIMEOUT) {
                    timed_out = 1;
                } else if (row->status == BT_MUT_SURVIVED) {
                    survived = 1;
                }
                if (i + 1 == row_total) {
                    end = row_total;
                }
            }
            if (rc == 0) {
                if (site_push(&sites, &refs[start]) != 0) {
                    rc = union_fail("out of memory");
                    break;
                }
                bt_mut_status verdict = BT_MUT_BUILDERR;
                if (killed) {
                    verdict = BT_MUT_KILLED;
                } else if (timed_out) {
                    verdict = BT_MUT_TIMEOUT;
                } else if (survived) {
                    verdict = BT_MUT_SURVIVED;
                }
                sites.items[sites.count - 1].verdict = verdict;
                start = end;
            }
        }
    }
    if (rc == 0) {
        root = bt_json_new_object();
        bt_json_value *outcomes = bt_json_new_array();
        bt_json_value *baseline = bt_json_new_object();
        bt_json_object_set(baseline, "scenario", bt_json_new_string("Baseline"));
        bt_json_object_set(baseline, "summary", bt_json_new_string("Success"));
        bt_json_array_push(outcomes, baseline);
        unsigned long long counters[5] = {0};
        bt_union_func_cache cache;
        memset(&cache, 0, sizeof cache);
        for (size_t i = 0; i < sites.count && rc == 0; i++) {
            const bt_union_site *site = &sites.items[i];
            func_cache_load(&cache, repo_root, site->rep->file);
            const char *function = "";
            if (cache.valid) {
                size_t offset =
                    bt_mut_offset_at(cache.tokens.text, site->rep->line, site->rep->col);
                if (offset != (size_t)-1) {
                    function = bt_mut_function_at(&cache.tokens, offset);
                }
            }
            bt_json_value *outcome = bt_json_new_object();
            bt_json_object_set(outcome, "scenario", bt_mut_mutant_scenario(site->rep, function));
            bt_json_object_set(outcome, "summary",
                               bt_json_new_string(bt_mut_outcome_summary(site->verdict)));
            bt_json_array_push(outcomes, outcome);
            counters[0]++;
            if (site->verdict == BT_MUT_KILLED) {
                counters[1]++;
            } else if (site->verdict == BT_MUT_SURVIVED) {
                counters[2]++;
            } else if (site->verdict == BT_MUT_TIMEOUT) {
                counters[3]++;
            } else {
                counters[4]++;
            }
        }
        func_cache_free(&cache);
        char now[32];
        bt_mut_now_iso(now);
        bt_json_object_set(root, "total_mutants", bt_json_new_integer((long long)counters[0]));
        bt_json_object_set(root, "caught", bt_json_new_integer((long long)counters[1]));
        bt_json_object_set(root, "missed", bt_json_new_integer((long long)counters[2]));
        bt_json_object_set(root, "timeout", bt_json_new_integer((long long)counters[3]));
        bt_json_object_set(root, "unviable", bt_json_new_integer((long long)counters[4]));
        bt_json_object_set(root, "success", bt_json_new_integer(0));
        bt_json_object_set(root, "cargo_mutants_version", bt_json_new_string("bt-mutate"));
        bt_json_object_set(root, "start_time", bt_json_new_string(now));
        bt_json_object_set(root, "end_time", bt_json_new_string(now));
        bt_json_object_set(root, "outcomes", outcomes);
        bt_strbuf serialized;
        if (bt_strbuf_init(&serialized, 8192) != 0 || bt_json_serialize(root, &serialized) != 0) {
            rc = union_fail("cannot serialize union outcomes");
        } else if (bt_mut_write_file(out_path, serialized.data, serialized.len) != 0) {
            rc = union_fail("cannot write %s", out_path);
        }
        bt_strbuf_free(&serialized);
    }
    free(refs);
    free(sites.items);
    bt_json_free(root);
    for (size_t i = 0; i < source_count; i++) {
        bt_mut_ledger_free(&sources[i].ledger);
    }
    free(sources);
    bt_mut_paths_free(&files);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.union_report", "%s", bt_mut_union_last_error());
    }
    BT_TRACE_EXIT("mutate.union_report");
    return rc == 0 ? 0 : 1;
}
