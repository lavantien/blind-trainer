#include "ga-run/lib.h"

#include "common/bt_trace.h"
#include "common/config.h"
#include "data/paths.h"
#include "ga-run/out.h"
#include "scheme/pairs.h"

#include <ctype.h>
#include <direct.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *bt_gr_tool_id(void) {
    BT_TRACE_ENTER("gr.tool_id");
    BT_TRACE_EXIT("gr.tool_id");
    return "bt-ga-run";
}

int bt_gr_offenders_push(bt_gr_offenders *list, const char *fmt, ...) {
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    if (list->count == list->cap) {
        size_t cap = list->cap == 0 ? 8 : list->cap * 2;
        char **grown = realloc(list->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->cap = cap;
    }
    char *copy = _strdup(text);
    if (copy == NULL) {
        return -1;
    }
    list->items[list->count] = copy;
    list->count++;
    return 0;
}

void bt_gr_offenders_free(bt_gr_offenders *list) {
    BT_TRACE_ENTER("gr.offenders_free");
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
    BT_TRACE_EXIT("gr.offenders_free");
}

static int is_absolute_path(const char *path) {
    return path[0] == '/' || path[0] == '\\' || (path[0] != '\0' && path[1] == ':');
}

int bt_gr_config_path(const char *relative, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("gr.config_path");
    if (is_absolute_path(relative)) {
        snprintf(out, BT_PATH_MAX, "%s", relative);
    } else {
        char root[BT_PATH_MAX];
        bt_data_workspace_root(root);
        snprintf(out, BT_PATH_MAX, "%s/%s", root, relative);
    }
    BT_TRACE_EXIT("gr.config_path");
    return 0;
}

static char *read_file_bytes(const char *path, size_t *len, char err[BT_GA_ERR_MAX]) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    *len = got;
    return text;
}

static size_t utf8_char_count(const char *text) {
    size_t count = 0;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
        if ((*cursor & 0xC0U) != 0x80U) {
            count++;
        }
    }
    return count;
}

int bt_gr_cross_check_cases(const bt_json_value *config, bt_gr_offenders *offenders) {
    BT_TRACE_ENTER("gr.cross_check_cases");
    const char *vendor = NULL;
    if (bt_config_str(config, "paths.data_vendor", &vendor) != 0) {
        bt_gr_offenders_push(offenders, "config section missing: paths.data_vendor");
        BT_TRACE_FAIL("gr.cross_check_cases", "paths.data_vendor missing");
        return -1;
    }
    char vendor_dir[BT_PATH_MAX];
    bt_gr_config_path(vendor, vendor_dir);
    char cases_file[BT_PATH_MAX];
    snprintf(cases_file, sizeof cases_file, "%s/cases.json", vendor_dir);
    char err[BT_GA_ERR_MAX];
    size_t bytes_len = 0;
    char *bytes = read_file_bytes(cases_file, &bytes_len, err);
    if (bytes == NULL) {
        bt_gr_offenders_push(offenders, "%s", err);
        BT_TRACE_FAIL("gr.cross_check_cases", "cases.json read failed: %s", err);
        return -1;
    }
    bt_json_value *root = NULL;
    bt_json_error json_error;
    if (bt_json_parse(bytes, bytes_len, &root, &json_error) != 0) {
        free(bytes);
        bt_gr_offenders_push(offenders, "parse cases.json: %s", json_error.message);
        BT_TRACE_FAIL("gr.cross_check_cases", "cases.json parse failed: %s", json_error.message);
        return -1;
    }
    free(bytes);
    const bt_json_value *recorded = bt_json_get(root, "cases");
    if (recorded == NULL || recorded->type != BT_JSON_ARRAY) {
        bt_json_free(root);
        bt_gr_offenders_push(offenders, "cases.json missing cases array");
        BT_TRACE_FAIL("gr.cross_check_cases", "cases array missing");
        return -1;
    }
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    size_t expected_count = edge_count + corner_count;
    if (recorded->as.array.count != expected_count) {
        bt_gr_offenders_push(offenders, "cases.json has %zu rows, scheme enumerates %zu",
                             recorded->as.array.count, expected_count);
        bt_json_free(root);
        BT_TRACE_FAIL("gr.cross_check_cases", "row count %zu vs %zu", recorded->as.array.count,
                      expected_count);
        return -1;
    }
    for (size_t i = 0; i < expected_count; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        const char *orbit_text = case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner";
        const bt_json_value *row = bt_json_at(recorded, i);
        const bt_json_value *got_orbit = row != NULL ? bt_json_get(row, "orbit") : NULL;
        const bt_json_value *got_t1 = row != NULL ? bt_json_get(row, "t1") : NULL;
        const bt_json_value *got_t2 = row != NULL ? bt_json_get(row, "t2") : NULL;
        const bt_json_value *got_l1 = row != NULL ? bt_json_get(row, "t1_letter") : NULL;
        const bt_json_value *got_l2 = row != NULL ? bt_json_get(row, "t2_letter") : NULL;
        if (got_orbit == NULL || got_orbit->type != BT_JSON_STRING) {
            bt_gr_offenders_push(offenders, "cases.json row missing orbit");
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu missing orbit", i);
            return -1;
        }
        if (got_t1 == NULL || !bt_json_is_integer(got_t1) || got_t2 == NULL ||
            !bt_json_is_integer(got_t2)) {
            bt_gr_offenders_push(offenders, "cases.json row missing t1 or t2");
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu missing t1 or t2", i);
            return -1;
        }
        if (got_l1 == NULL || got_l1->type != BT_JSON_STRING) {
            bt_gr_offenders_push(offenders, "cases.json row missing t1_letter");
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu missing t1_letter", i);
            return -1;
        }
        if (got_l2 == NULL || got_l2->type != BT_JSON_STRING) {
            bt_gr_offenders_push(offenders, "cases.json row missing t2_letter");
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu missing t2_letter", i);
            return -1;
        }
        if (strcmp(got_orbit->as.string, orbit_text) != 0 ||
            (uint64_t)got_t1->integer != (uint64_t)case_id->t1 ||
            (uint64_t)got_t2->integer != (uint64_t)case_id->t2) {
            bt_gr_offenders_push(offenders, "cases.json row mismatch: %s %lld %lld vs %s %u %u",
                                 got_orbit->as.string, got_t1->integer, got_t2->integer, orbit_text,
                                 (unsigned)case_id->t1, (unsigned)case_id->t2);
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu mismatch", i);
            return -1;
        }
        char letters[3];
        letters[0] = bt_scheme_letter_of(case_id->t1);
        letters[1] = bt_scheme_letter_of(case_id->t2);
        letters[2] = '\0';
        char got[64];
        size_t at = 0;
        for (const unsigned char *c = (const unsigned char *)got_l1->as.string;
             *c != '\0' && at < sizeof got - 1; c++) {
            got[at] = (char)tolower(*c);
            at++;
        }
        for (const unsigned char *c = (const unsigned char *)got_l2->as.string;
             *c != '\0' && at < sizeof got - 1; c++) {
            got[at] = (char)tolower(*c);
            at++;
        }
        got[at] = '\0';
        char want[3];
        want[0] = (char)tolower((unsigned char)letters[0]);
        want[1] = (char)tolower((unsigned char)letters[1]);
        want[2] = '\0';
        if (strcmp(got, want) != 0) {
            bt_gr_offenders_push(offenders, "cases.json letters %s%s vs scheme %s",
                                 got_l1->as.string, got_l2->as.string, letters);
            bt_json_free(root);
            BT_TRACE_FAIL("gr.cross_check_cases", "row %zu letters mismatch", i);
            return -1;
        }
    }
    bt_json_free(root);
    BT_TRACE_EXIT("gr.cross_check_cases");
    return 0;
}

static int load_features(const bt_gr_run_inputs *inputs, const bt_ga_inventory_entry *entries,
                         size_t entry_count, bt_ga_features *feats, bt_gr_offenders *offenders) {
    for (size_t p = 0; p < inputs->pool_count; p++) {
        const bt_ga_inventory_entry *source = NULL;
        for (size_t e = 0; e < entry_count; e++) {
            if (strcmp(entries[e].text, inputs->pool[p].text) == 0) {
                source = &entries[e];
                break;
            }
        }
        if (source == NULL) {
            bt_gr_offenders_push(offenders, "pool entry %s missing from inventory",
                                 inputs->pool[p].text);
            BT_TRACE_FAIL("gr.load_features", "pool entry %s missing", inputs->pool[p].text);
            return -1;
        }
        feats[p].tone = source->tone;
        feats[p].has_onset = bt_ga_onset_features_of(source->onset, &feats[p].onset) == 1;
        feats[p].nucleus = source->nucleus;
        feats[p].coda = source->coda;
    }
    return 0;
}

int bt_gr_load_run_inputs(bt_gr_run_inputs *inputs, bt_gr_offenders *offenders) {
    BT_TRACE_ENTER("gr.load_run_inputs");
    memset(inputs, 0, sizeof *inputs);
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        bt_gr_offenders_push(offenders, "load config failed");
        BT_TRACE_FAIL("gr.load_run_inputs", "config load failed");
        return -1;
    }
    char err[BT_GA_ERR_MAX];
    int failed = 0;
    if (bt_ga_config_params(config, &inputs->params, err) != 1) {
        bt_gr_offenders_push(offenders, "%s", err);
        failed = 1;
    }
    size_t top_k = 0;
    if (!failed && bt_ga_config_pool_top_k(config, &top_k, err) != 1) {
        bt_gr_offenders_push(offenders, "%s", err);
        failed = 1;
    }
    const char *ref_doc = NULL;
    if (!failed && bt_config_str(config, "paths.ref_doc", &ref_doc) != 0) {
        bt_gr_offenders_push(offenders, "config section missing: paths.ref_doc");
        BT_TRACE_FAIL("gr.load_run_inputs", "paths.ref_doc missing");
        failed = 1;
    }
    char vietnamese[BT_PATH_MAX];
    if (!failed) {
        char ref_root[BT_PATH_MAX];
        bt_gr_config_path(ref_doc, ref_root);
        snprintf(vietnamese, sizeof vietnamese, "%s/%s", ref_root, BT_GR_VIETNAMESE_DIR);
    }
    bt_ga_strset wordlist = {0};
    if (!failed) {
        char wordlist_path[BT_PATH_MAX];
        snprintf(wordlist_path, sizeof wordlist_path, "%s/%s", vietnamese, BT_GR_WORDLIST_FILE);
        if (bt_ga_load_wordlist(wordlist_path, &wordlist, err) != 1) {
            bt_gr_offenders_push(offenders, "%s", err);
            BT_TRACE_FAIL("gr.load_run_inputs", "wordlist load failed: %s", err);
            failed = 1;
        }
    }
    bt_ga_freqs syllable_freqs = {0};
    if (!failed) {
        char freqs_path[BT_PATH_MAX];
        snprintf(freqs_path, sizeof freqs_path, "%s/%s", vietnamese, BT_GR_FREQS_FILE);
        if (bt_ga_load_syllable_frequencies(freqs_path, &syllable_freqs, err) != 1) {
            bt_gr_offenders_push(offenders, "%s", err);
            BT_TRACE_FAIL("gr.load_run_inputs", "syllable frequencies load failed: %s", err);
            failed = 1;
        }
    }
    bt_ga_inventory_entry *entries = NULL;
    size_t entry_count = 0;
    if (!failed && bt_ga_build_inventory(&wordlist, &entries, &entry_count,
                                         &inputs->inventory_report, err) != 1) {
        bt_gr_offenders_push(offenders, "%s", err);
        BT_TRACE_FAIL("gr.load_run_inputs", "inventory build failed: %s", err);
        failed = 1;
    }
    if (!failed) {
        bt_ga_build_pool(entries, entry_count, &syllable_freqs, top_k, &inputs->pool,
                         &inputs->pool_count);
        if (inputs->pool_count < bt_ga_case_count()) {
            bt_gr_offenders_push(offenders, "pool %zu smaller than %zu", inputs->pool_count,
                                 bt_ga_case_count());
            BT_TRACE_FAIL("gr.load_run_inputs", "pool %zu below case count %zu", inputs->pool_count,
                          bt_ga_case_count());
            failed = 1;
        }
    }
    char freq_file[BT_PATH_MAX] = {0};
    if (!failed) {
        const char *data_ga = NULL;
        if (bt_config_str(config, "paths.data_ga", &data_ga) != 0) {
            bt_gr_offenders_push(offenders, "config section missing: paths.data_ga");
            BT_TRACE_FAIL("gr.load_run_inputs", "paths.data_ga missing");
            failed = 1;
        } else {
            char data_ga_dir[BT_PATH_MAX];
            bt_gr_config_path(data_ga, data_ga_dir);
            snprintf(freq_file, sizeof freq_file, "%s/freq.json", data_ga_dir);
            inputs->freq_bytes = read_file_bytes(freq_file, &inputs->freq_bytes_len, err);
            if (inputs->freq_bytes == NULL) {
                bt_gr_offenders_push(offenders, "%s", err);
                BT_TRACE_FAIL("gr.load_run_inputs", "freq.json read failed: %s", err);
                failed = 1;
            }
        }
    }
    if (!failed && bt_ga_load_freq_data(freq_file, &inputs->freq, err) != 1) {
        bt_gr_offenders_push(offenders, "parse freq.json: %s", err);
        BT_TRACE_FAIL("gr.load_run_inputs", "freq.json parse failed: %s", err);
        failed = 1;
    }
    if (!failed && bt_gr_cross_check_cases(config, offenders) != 0) {
        failed = 1;
    }
    float *lengths = NULL;
    bt_ga_features *feats = NULL;
    bt_ga_pair_weight *cooc = NULL;
    bt_ga_pair_weight *adj = NULL;
    if (!failed) {
        lengths = malloc(inputs->pool_count * sizeof *lengths);
        feats = calloc(inputs->pool_count, sizeof *feats);
        if (lengths == NULL || feats == NULL) {
            bt_gr_offenders_push(offenders, "pool table allocation failed");
            BT_TRACE_FAIL("gr.load_run_inputs", "pool table allocation failed");
            failed = 1;
        }
    }
    if (!failed && load_features(inputs, entries, entry_count, feats, offenders) != 0) {
        failed = 1;
    }
    if (!failed) {
        for (size_t p = 0; p < inputs->pool_count; p++) {
            lengths[p] = (float)utf8_char_count(inputs->pool[p].text);
        }
    }
    if (!failed) {
        uint64_t cooc_total = 0;
        uint64_t adj_total = 0;
        for (size_t k = 0; k < inputs->freq.cooccurring_count; k++) {
            cooc_total += inputs->freq.cooccurring[k].count;
        }
        for (size_t k = 0; k < inputs->freq.adjacent_count; k++) {
            adj_total += inputs->freq.adjacent[k].count;
        }
        cooc = malloc((inputs->freq.cooccurring_count > 0 ? inputs->freq.cooccurring_count : 1) *
                      sizeof *cooc);
        adj = malloc((inputs->freq.adjacent_count > 0 ? inputs->freq.adjacent_count : 1) *
                     sizeof *adj);
        if (cooc == NULL || adj == NULL) {
            bt_gr_offenders_push(offenders, "pair weight allocation failed");
            BT_TRACE_FAIL("gr.load_run_inputs", "pair weight allocation failed");
            failed = 1;
        } else {
            for (size_t k = 0; k < inputs->freq.cooccurring_count; k++) {
                cooc[k].i = inputs->freq.cooccurring[k].from;
                cooc[k].j = inputs->freq.cooccurring[k].to;
                cooc[k].w = (float)inputs->freq.cooccurring[k].count / (float)cooc_total;
            }
            for (size_t k = 0; k < inputs->freq.adjacent_count; k++) {
                adj[k].i = inputs->freq.adjacent[k].from;
                adj[k].j = inputs->freq.adjacent[k].to;
                adj[k].w = (float)inputs->freq.adjacent[k].count / (float)adj_total;
            }
        }
    }
    if (!failed &&
        bt_ga_build_env(inputs->pool_count, lengths, inputs->pool_count, feats, inputs->pool_count,
                        bt_ga_case_count(), cooc, inputs->freq.cooccurring_count, adj,
                        inputs->freq.adjacent_count, &inputs->env, err) != 1) {
        bt_gr_offenders_push(offenders, "%s", err);
        BT_TRACE_FAIL("gr.load_run_inputs", "env build failed: %s", err);
        failed = 1;
    }
    bt_json_free(config);
    bt_ga_strset_free(&wordlist);
    bt_ga_freqs_free(&syllable_freqs);
    bt_ga_inventory_free(entries);
    if (failed) {
        free(lengths);
        free(feats);
        free(cooc);
        free(adj);
        bt_gr_run_inputs_free(inputs);
        return -1;
    }
    BT_TRACE_EXIT("gr.load_run_inputs");
    return 0;
}

void bt_gr_run_inputs_free(bt_gr_run_inputs *inputs) {
    BT_TRACE_ENTER("gr.run_inputs_free");
    bt_ga_env_free(&inputs->env);
    free(inputs->env.lengths);
    free(inputs->env.feats);
    free(inputs->env.cooc);
    free(inputs->env.adj);
    inputs->env.lengths = NULL;
    inputs->env.feats = NULL;
    inputs->env.cooc = NULL;
    inputs->env.adj = NULL;
    bt_ga_pool_free(inputs->pool);
    inputs->pool = NULL;
    inputs->pool_count = 0;
    bt_ga_freq_data_free(&inputs->freq);
    free(inputs->freq_bytes);
    inputs->freq_bytes = NULL;
    inputs->freq_bytes_len = 0;
    BT_TRACE_EXIT("gr.run_inputs_free");
}

int bt_gr_build_assignment(const bt_gr_run_inputs *inputs, const bt_ga_run_outcome *outcome,
                           bt_gr_syllable_assignment *assignment, bt_gr_offenders *offenders) {
    BT_TRACE_ENTER("gr.build_assignment");
    memset(assignment, 0, sizeof *assignment);
    size_t case_count = bt_ga_case_count();
    if (outcome->best_count != case_count) {
        bt_gr_offenders_push(offenders, "best chromosome has %zu genes, expected %zu",
                             outcome->best_count, case_count);
        BT_TRACE_FAIL("gr.build_assignment", "gene count %zu vs %zu", outcome->best_count,
                      case_count);
        return -1;
    }
    for (size_t i = 0; i < outcome->best_count; i++) {
        int seen = 0;
        for (size_t j = 0; j < i; j++) {
            if (outcome->best[j] == outcome->best[i]) {
                seen = 1;
                break;
            }
        }
        if (seen) {
            bt_gr_offenders_push(offenders, "duplicate pool index %u in assignment",
                                 (unsigned)outcome->best[i]);
        }
        if ((size_t)outcome->best[i] >= inputs->pool_count) {
            bt_gr_offenders_push(offenders, "pool index %u outside pool %zu",
                                 (unsigned)outcome->best[i], inputs->pool_count);
        }
    }
    if (offenders->count > 0) {
        BT_TRACE_FAIL("gr.build_assignment", "chromosome rejected with %zu offenders",
                      offenders->count);
        return -1;
    }
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    assignment->cases = malloc(case_count * sizeof *assignment->cases);
    if (assignment->cases == NULL) {
        bt_gr_offenders_push(offenders, "assignment allocation failed");
        BT_TRACE_FAIL("gr.build_assignment", "allocation failed");
        return -1;
    }
    assignment->cases_count = case_count;
    for (size_t position = 0; position < case_count; position++) {
        const bt_case *case_id =
            position < edge_count ? &edge[position] : &corner[position - edge_count];
        const char *orbit_text = case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner";
        char letters[3];
        letters[0] = bt_scheme_letter_of(case_id->t1);
        letters[1] = bt_scheme_letter_of(case_id->t2);
        letters[2] = '\0';
        snprintf(assignment->cases[position].orbit, sizeof assignment->cases[position].orbit, "%s",
                 orbit_text);
        snprintf(assignment->cases[position].letters, sizeof assignment->cases[position].letters,
                 "%s", letters);
        snprintf(assignment->cases[position].syllable, sizeof assignment->cases[position].syllable,
                 "%s", inputs->pool[outcome->best[position]].text);
        assignment->cases[position].frequency = inputs->freq.cases[position].count;
    }
    for (size_t i = 0; i < case_count; i++) {
        int seen = 0;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(assignment->cases[i].syllable, assignment->cases[j].syllable) == 0) {
                seen = 1;
                break;
            }
        }
        if (seen) {
            bt_gr_offenders_push(offenders, "duplicate syllable %s", assignment->cases[i].syllable);
        }
    }
    if (offenders->count > 0) {
        bt_gr_assignment_free(assignment);
        BT_TRACE_FAIL("gr.build_assignment", "duplicate syllable rejected");
        return -1;
    }
    bt_strbuf compact;
    bt_strbuf_init(&compact, 4096);
    if (bt_gr_cases_text_compact(assignment->cases, assignment->cases_count, &compact) != 0) {
        bt_strbuf_free(&compact);
        bt_gr_assignment_free(assignment);
        bt_gr_offenders_push(offenders, "syllable serialization failed");
        BT_TRACE_FAIL("gr.build_assignment", "compact serialization failed");
        return -1;
    }
    bt_ga_sha256_hex(compact.data, compact.len, assignment->syllables_sha256);
    bt_strbuf_free(&compact);
    bt_ga_sha256_hex(inputs->freq_bytes, inputs->freq_bytes_len, assignment->freq_sha256);
    assignment->pool_size = inputs->pool_count;
    assignment->seed = inputs->params.seed;
    assignment->best_fitness = outcome->best_fitness;
    BT_TRACE_EXIT("gr.build_assignment");
    return 0;
}

void bt_gr_assignment_free(bt_gr_syllable_assignment *assignment) {
    BT_TRACE_ENTER("gr.assignment_free");
    free(assignment->cases);
    assignment->cases = NULL;
    assignment->cases_count = 0;
    BT_TRACE_EXIT("gr.assignment_free");
}

void bt_gr_build_report(const bt_gr_run_inputs *inputs, const bt_ga_run_outcome *outcome,
                        const bt_gr_syllable_assignment *assignment, const bt_json_value *config,
                        bt_gr_ga_report *report) {
    BT_TRACE_ENTER("gr.build_report");
    memset(report, 0, sizeof *report);
    report->run_config = config;
    report->inventory = inputs->inventory_report;
    report->pool_size = inputs->pool_count;
    size_t top = inputs->pool_count < BT_GR_POOL_TOP_MAX ? inputs->pool_count : BT_GR_POOL_TOP_MAX;
    for (size_t i = 0; i < top; i++) {
        report->pool_top[i] = inputs->pool[i];
        report->pool_top_count++;
    }
    report->best_fitness = outcome->best_fitness;
    report->best_parts[0] = outcome->best_parts[0];
    report->best_parts[1] = outcome->best_parts[1];
    report->best_parts[2] = outcome->best_parts[2];
    report->best_generation = outcome->best_generation;
    report->generations_run = inputs->params.generations;
    report->per_gen_best = malloc((outcome->per_gen_count > 0 ? outcome->per_gen_count : 1) *
                                  sizeof *report->per_gen_best);
    for (size_t i = 0; i < outcome->per_gen_count; i++) {
        report->per_gen_best[i] = outcome->per_gen_best[i];
    }
    report->per_gen_count = outcome->per_gen_count;
    report->stagnation_tail = outcome->stagnation_tail;
    report->max_stagnation = outcome->max_stagnation;
    report->evaluations = outcome->evaluations;
    snprintf(report->freq_sha256, BT_GA_SHA256_HEX_LEN, "%s", assignment->freq_sha256);
    snprintf(report->syllables_sha256, BT_GA_SHA256_HEX_LEN, "%s", assignment->syllables_sha256);
    BT_TRACE_EXIT("gr.build_report");
}

void bt_gr_report_free(bt_gr_ga_report *report) {
    BT_TRACE_ENTER("gr.report_free");
    free(report->per_gen_best);
    report->per_gen_best = NULL;
    report->per_gen_count = 0;
    report->run_config = NULL;
    BT_TRACE_EXIT("gr.report_free");
}

static uint64_t config_u64(const bt_json_value *config, const char *dotted) {
    const bt_json_value *value = bt_config_value(config, dotted);
    if (value == NULL || !bt_json_is_integer(value) || value->integer < 0) {
        return 0;
    }
    return (uint64_t)value->integer;
}

static double config_f64_or_zero(const bt_json_value *config, const char *dotted) {
    const bt_json_value *value = bt_config_value(config, dotted);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return 0.0;
    }
    return value->as.number;
}

int bt_gr_render_markdown(const bt_gr_ga_report *report, bt_strbuf *out) {
    BT_TRACE_ENTER("gr.render_markdown");
    const bt_json_value *config = report->run_config;
    double w_conf = config_f64_or_zero(config, "ga.weights.confusion");
    double w_flow = config_f64_or_zero(config, "ga.weights.flow");
    double w_cplx = config_f64_or_zero(config, "ga.weights.complexity");
    int rc = bt_strbuf_append_str(out, "# ga run report\n\n## run config\n\n") == 0 &&
             bt_strbuf_appendf(out,
                               "seed %llu population %llu generations %llu elite %llu "
                               "tournament 2 weights conf ",
                               (unsigned long long)config_u64(config, "ga.seed"),
                               (unsigned long long)config_u64(config, "ga.population"),
                               (unsigned long long)config_u64(config, "ga.generations"),
                               (unsigned long long)config_u64(config, "ga.elite")) == 0 &&
             bt_gr_display_f64(out, w_conf) == 0 && bt_strbuf_append_str(out, " flow ") == 0 &&
             bt_gr_display_f64(out, w_flow) == 0 && bt_strbuf_append_str(out, " cplx ") == 0 &&
             bt_gr_display_f64(out, w_cplx) == 0 &&
             bt_strbuf_append_str(out, " mutation swap ") == 0 &&
             bt_gr_display_f64(out, config_f64_or_zero(config, "ga.mutation.swap")) == 0 &&
             bt_strbuf_append_str(out, " replace ") == 0 &&
             bt_gr_display_f64(out, config_f64_or_zero(config, "ga.mutation.replace")) == 0 &&
             bt_strbuf_appendf(out,
                               "\n\n## inventory\n\ngenerated %zu kept %zu dropped %zu "
                               "wordlist %zu covered %zu (%.1f%%)\n\n",
                               report->inventory.generated, report->inventory.kept,
                               report->inventory.dropped_not_in_wordlist,
                               report->inventory.wordlist_size, report->inventory.wordlist_covered,
                               100.0 * (double)report->inventory.wordlist_covered /
                                   (double)report->inventory.wordlist_size) == 0 &&
             bt_strbuf_appendf(out,
                               "## pool\n\nsize %zu top entries\n\n| syllable | corpus "
                               "frequency |\n| --- | --- |\n",
                               report->pool_size) == 0;
    if (rc) {
        for (size_t i = 0; i < report->pool_top_count && rc; i++) {
            rc = bt_strbuf_appendf(out, "| %s | %llu |\n", report->pool_top[i].text,
                                   (unsigned long long)report->pool_top[i].frequency) == 0;
        }
    }
    if (rc) {
        rc = bt_strbuf_appendf(out, "\n## result\n\nbest fitness %.6f at generation %zu of %zu\n",
                               (double)report->best_fitness, report->best_generation,
                               report->generations_run) == 0 &&
             bt_strbuf_appendf(out,
                               "parts conf %.6f flow %.6f cplx %.6f (weighted %.6f %.6f "
                               "%.6f)\n",
                               (double)report->best_parts[0], (double)report->best_parts[1],
                               (double)report->best_parts[2],
                               (double)(report->best_parts[0] * (float)w_conf),
                               (double)(report->best_parts[1] * (float)w_flow),
                               (double)(report->best_parts[2] * (float)w_cplx)) == 0 &&
             bt_strbuf_appendf(out, "stagnation tail %u max %u\nevaluations %llu\n\n",
                               (unsigned)report->stagnation_tail, (unsigned)report->max_stagnation,
                               (unsigned long long)report->evaluations) == 0 &&
             bt_strbuf_append_str(out, "## trajectory\n\n") == 0;
    }
    if (rc) {
        size_t step = report->per_gen_count / 20;
        if (step < 1) {
            step = 1;
        }
        for (size_t i = 0; i < report->per_gen_count && rc; i += step) {
            if (i > 0) {
                rc = bt_strbuf_append_str(out, " ") == 0;
            }
            if (rc) {
                rc = bt_strbuf_appendf(out, "%.6f", (double)report->per_gen_best[i]) == 0;
            }
        }
    }
    if (rc) {
        rc = bt_strbuf_appendf(out,
                               "\n\n## artifacts\n\nfreq.json sha256 %s\nsyllables.json sha256 "
                               "%s\n",
                               report->freq_sha256, report->syllables_sha256) == 0;
    }
    if (!rc) {
        BT_TRACE_FAIL("gr.render_markdown", "markdown rendering failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.render_markdown");
    return 0;
}

static void ensure_parent_dirs(const char *path) {
    BT_TRACE_ENTER("gr.ensure_parent_dirs");
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            char saved = *cursor;
            *cursor = '\0';
            _mkdir(copy);
            *cursor = saved;
        }
    }
    BT_TRACE_EXIT("gr.ensure_parent_dirs");
}

static int write_plain(const char *path, const char *bytes, size_t len) {
    BT_TRACE_ENTER("gr.write_plain");
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_TRACE_FAIL("gr.write_plain", "open failed for %s", path);
        return -1;
    }
    size_t wrote = fwrite(bytes, 1, len, file);
    int closed = fclose(file);
    if (wrote != len || closed != 0) {
        BT_TRACE_FAIL("gr.write_plain", "short write on %s", path);
        return -1;
    }
    BT_TRACE_EXIT("gr.write_plain");
    return 0;
}

static int write_outputs(const char *data_ga, const char *artifacts,
                         const bt_gr_syllable_assignment *assignment, const bt_gr_ga_report *report,
                         bt_gr_offenders *offenders) {
    BT_TRACE_ENTER("gr.write_outputs");
    char syllables_path[BT_PATH_MAX];
    char report_path[BT_PATH_MAX];
    char markdown_dir[BT_PATH_MAX];
    char markdown_path[BT_PATH_MAX];
    snprintf(syllables_path, sizeof syllables_path, "%s/syllables.json", data_ga);
    snprintf(report_path, sizeof report_path, "%s/report.json", data_ga);
    snprintf(markdown_dir, sizeof markdown_dir, "%s/ga", artifacts);
    snprintf(markdown_path, sizeof markdown_path, "%s/report.md", markdown_dir);
    bt_strbuf syllables;
    bt_strbuf_init(&syllables, 4096);
    bt_strbuf report_json;
    bt_strbuf_init(&report_json, 2048);
    bt_strbuf markdown;
    bt_strbuf_init(&markdown, 1024);
    int failed = 0;
    if (bt_gr_assignment_text(assignment, &syllables) != 0) {
        bt_gr_offenders_push(offenders, "serialize syllables.json failed");
        BT_TRACE_FAIL("gr.write_outputs", "syllables serialization failed");
        failed = 1;
    }
    if (!failed && bt_gr_report_text(report, &report_json) != 0) {
        bt_gr_offenders_push(offenders, "serialize report.json failed");
        BT_TRACE_FAIL("gr.write_outputs", "report serialization failed");
        failed = 1;
    }
    if (!failed && bt_gr_render_markdown(report, &markdown) != 0) {
        bt_gr_offenders_push(offenders, "render report.md failed");
        BT_TRACE_FAIL("gr.write_outputs", "markdown rendering failed");
        failed = 1;
    }
    if (!failed) {
        ensure_parent_dirs(syllables_path);
        if (bt_paths_write_atomic(syllables_path, syllables.data, syllables.len) != 0) {
            bt_gr_offenders_push(offenders, "write %s", syllables_path);
            BT_TRACE_FAIL("gr.write_outputs", "write failed for %s", syllables_path);
            failed = 1;
        }
    }
    if (!failed) {
        ensure_parent_dirs(report_path);
        if (bt_paths_write_atomic(report_path, report_json.data, report_json.len) != 0) {
            bt_gr_offenders_push(offenders, "write %s", report_path);
            BT_TRACE_FAIL("gr.write_outputs", "write failed for %s", report_path);
            failed = 1;
        }
    }
    if (!failed) {
        ensure_parent_dirs(markdown_path);
        if (write_plain(markdown_path, markdown.data, markdown.len) != 0) {
            bt_gr_offenders_push(offenders, "write %s", markdown_path);
            BT_TRACE_FAIL("gr.write_outputs", "write failed for %s", markdown_path);
            failed = 1;
        }
    }
    bt_strbuf_free(&syllables);
    bt_strbuf_free(&report_json);
    bt_strbuf_free(&markdown);
    BT_TRACE_EXIT("gr.write_outputs");
    return failed ? -1 : 0;
}

static size_t distinct_syllables(const bt_gr_syllable_assignment *assignment) {
    size_t distinct = 0;
    for (size_t i = 0; i < assignment->cases_count; i++) {
        int seen = 0;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(assignment->cases[i].syllable, assignment->cases[j].syllable) == 0) {
                seen = 1;
                break;
            }
        }
        if (!seen) {
            distinct++;
        }
    }
    return distinct;
}

int bt_gr_run_all(bt_gr_offenders *offenders) {
    BT_TRACE_ENTER("gr.run_all");
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        bt_gr_offenders_push(offenders, "load config failed");
        BT_TRACE_FAIL("gr.run_all", "config load failed");
        return -1;
    }
    bt_gr_run_inputs inputs;
    if (bt_gr_load_run_inputs(&inputs, offenders) != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("gr.run_all", "inputs rejected");
        return -1;
    }
    bt_ga_run_outcome *outcome = bt_ga_run(&inputs.env, &inputs.params);
    bt_gr_syllable_assignment assignment;
    if (bt_gr_build_assignment(&inputs, outcome, &assignment, offenders) != 0) {
        bt_ga_run_outcome_free(outcome);
        bt_gr_run_inputs_free(&inputs);
        bt_json_free(config);
        BT_TRACE_FAIL("gr.run_all", "assignment rejected");
        return -1;
    }
    bt_gr_ga_report report;
    bt_gr_build_report(&inputs, outcome, &assignment, config, &report);
    const char *data_ga = NULL;
    const char *artifacts = NULL;
    int failed = 0;
    if (bt_config_str(config, "paths.data_ga", &data_ga) != 0) {
        bt_gr_offenders_push(offenders, "config section missing: paths.data_ga");
        BT_TRACE_FAIL("gr.run_all", "paths.data_ga missing");
        failed = 1;
    } else if (bt_config_str(config, "paths.artifacts", &artifacts) != 0) {
        bt_gr_offenders_push(offenders, "config section missing: paths.artifacts");
        BT_TRACE_FAIL("gr.run_all", "paths.artifacts missing");
        failed = 1;
    }
    if (!failed) {
        char data_ga_dir[BT_PATH_MAX];
        char artifacts_dir[BT_PATH_MAX];
        bt_gr_config_path(data_ga, data_ga_dir);
        bt_gr_config_path(artifacts, artifacts_dir);
        if (write_outputs(data_ga_dir, artifacts_dir, &assignment, &report, offenders) != 0) {
            failed = 1;
        } else {
            char wrote[BT_PATH_MAX];
            snprintf(wrote, sizeof wrote, "%s/syllables.json", data_ga_dir);
            printf("ga: seed %llu pool %zu best fitness %.6f at gen %zu cases %zu distinct %zu, "
                   "wrote %s\n",
                   (unsigned long long)inputs.params.seed, inputs.pool_count,
                   (double)outcome->best_fitness, outcome->best_generation, assignment.cases_count,
                   distinct_syllables(&assignment), wrote);
        }
    }
    bt_gr_report_free(&report);
    bt_gr_assignment_free(&assignment);
    bt_ga_run_outcome_free(outcome);
    bt_gr_run_inputs_free(&inputs);
    bt_json_free(config);
    if (failed) {
        BT_TRACE_FAIL("gr.run_all", "run failed");
        return -1;
    }
    BT_TRACE_EXIT("gr.run_all");
    return 0;
}
