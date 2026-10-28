#include "verify/packs.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "verify/ev_bigcube.h"
#include "verify/ev_bld.h"
#include "verify/ev_cfop.h"
#include "verify/ev_fto.h"
#include "verify/ev_mega.h"
#include "verify/ev_nxn.h"
#include "verify/ev_pyra.h"
#include "verify/ev_skewb.h"
#include "verify/ev_sq1.h"

const verify_pack_pin VERIFY_PACK_EXPECTED_ROWS[] = {
    {"twoby2_cll.tsv", "2x2", "cll", "cll", 42},
    {"skewb_sarah.tsv", "skewb", "sarah_intermediate", "sarah", 15},
    {"pyraminx_l4e.tsv", "pyraminx", "l4e", "l4e", 35},
    {"megaminx_4lll.tsv", "megaminx", "4lll", "4lll", 39},
    {"sq1_cubeshape.tsv", "sq1", "lbl", "cubeshape", 169},
    {"sq1_co.tsv", "sq1", "lbl", "co", 7},
    {"sq1_eo.tsv", "sq1", "lbl", "eo", 7},
    {"sq1_cp.tsv", "sq1", "lbl", "cp", 16},
    {"sq1_ep.tsv", "sq1", "lbl", "ep", 99},
    {"bigcube_parity.tsv", "bigcube", "yau", "parity", 5},
    {"bigcube_l2e.tsv", "bigcube", "reduction", "parity", 12},
    {"fto_l4t_redux.tsv", "fto", "l4t_redux", "l4t", 36},
    {"bld_3style_specials.tsv", "3bld", "3style", "parity", 85},
    {"bld_oldpoch.tsv", "3bld", "old_pochmann", "concept", 40},
    {"bld_m2.tsv", "3bld", "m2", "concept", 24},
    {"cfop_pll.tsv", "3x3", "cfop", "pll", 21},
    {"cfop_oll.tsv", "3x3", "cfop", "oll", 57},
    {"cfop_f2l.tsv", "3x3", "cfop", "f2l", 164},
    {"cfop_pll.tsv", "oh", "cfop", "pll", 21},
    {"cfop_oll.tsv", "oh", "cfop", "oll", 57},
    {"cfop_f2l.tsv", "oh", "cfop", "f2l", 164},
};

const size_t VERIFY_PACK_PIN_COUNT =
    sizeof VERIFY_PACK_EXPECTED_ROWS / sizeof VERIFY_PACK_EXPECTED_ROWS[0];

int verify_puzzle_for_event(const char *event, verify_puzzle *out, char *err, size_t err_len) {
    BT_TRACE_ENTER("packs.puzzle_for_event");
    if (strcmp(event, "3x3") == 0 || strcmp(event, "oh") == 0 || strcmp(event, "3bld") == 0) {
        *out = VERIFY_PUZZLE_CORE;
    } else if (strcmp(event, "2x2") == 0) {
        *out = VERIFY_PUZZLE_NXN;
    } else if (strcmp(event, "sq1") == 0) {
        *out = VERIFY_PUZZLE_SQ1;
    } else if (strcmp(event, "pyraminx") == 0) {
        *out = VERIFY_PUZZLE_PYRA;
    } else if (strcmp(event, "skewb") == 0) {
        *out = VERIFY_PUZZLE_SKEWB;
    } else if (strcmp(event, "megaminx") == 0) {
        *out = VERIFY_PUZZLE_MEGA;
    } else if (strcmp(event, "fto") == 0) {
        *out = VERIFY_PUZZLE_FTO;
    } else if (strcmp(event, "bigcube") == 0) {
        snprintf(err, err_len, "bigcube rows resolve their own n");
        BT_TRACE_FAIL("packs.puzzle_for_event", "bigcube %s", event);
        BT_TRACE_EXIT("packs.puzzle_for_event");
        return 1;
    } else {
        snprintf(err, err_len, "unknown event %s", event);
        BT_TRACE_FAIL("packs.puzzle_for_event", "unknown %s", event);
        BT_TRACE_EXIT("packs.puzzle_for_event");
        return 1;
    }
    BT_TRACE_EXIT("packs.puzzle_for_event");
    return 0;
}

verify_tier2 verify_tier2_pass(void) {
    verify_tier2 outcome = {VERIFY_TIER2_PASS, {0}, NULL};
    return outcome;
}

verify_tier2 verify_tier2_fail(const char *message) {
    verify_tier2 outcome = {VERIFY_TIER2_FAIL, {0}, NULL};
    snprintf(outcome.fail, sizeof outcome.fail, "%s", message);
    return outcome;
}

verify_tier2 verify_tier2_skipped(const char *reason) {
    verify_tier2 outcome = {VERIFY_TIER2_SKIPPED, {0}, reason};
    return outcome;
}

void verify_section_init(verify_section_result *section) {
    section->rows = NULL;
    section->count = 0;
    section->cap = 0;
    section->honesty_count = 0;
}

void verify_section_free(verify_section_result *section) {
    free(section->rows);
    section->rows = NULL;
    section->count = 0;
    section->cap = 0;
    section->honesty_count = 0;
}

void verify_section_push_row(verify_section_result *section, const verify_row_outcome *row) {
    if (section->count == section->cap) {
        size_t cap = section->cap == 0 ? 16 : section->cap * 2;
        verify_row_outcome *rows = realloc(section->rows, cap * sizeof *rows);
        if (rows == NULL) {
            BT_TRACE_FAIL("packs.section_push_row", "oom at %zu", section->count);
            return;
        }
        section->rows = rows;
        section->cap = cap;
    }
    section->rows[section->count] = *row;
    section->count += 1;
}

void verify_section_push_honesty(verify_section_result *section, const char *note) {
    if (section->honesty_count >= VERIFY_HONESTY_CAP) {
        BT_TRACE_FAIL("packs.section_push_honesty", "cap %d reached", VERIFY_HONESTY_CAP);
        return;
    }
    snprintf(section->honesty[section->honesty_count], VERIFY_HONESTY_MAX, "%s", note);
    section->honesty_count += 1;
}

void verify_strlist_init(verify_strlist *list) {
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void verify_strlist_free(verify_strlist *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void verify_strlist_push(verify_strlist *list, const char *text) {
    if (list->count == list->cap) {
        size_t cap = list->cap == 0 ? 16 : list->cap * 2;
        char (*items)[VERIFY_OFFENDER_MAX] = realloc(list->items, cap * sizeof *items);
        if (items == NULL) {
            BT_TRACE_FAIL("packs.strlist_push", "oom at %zu", list->count);
            return;
        }
        list->items = items;
        list->cap = cap;
    }
    snprintf(list->items[list->count], VERIFY_OFFENDER_MAX, "%s", text);
    list->count += 1;
}

void verify_strlist_pushf(verify_strlist *list, const char *fmt, ...) {
    char text[VERIFY_OFFENDER_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    verify_strlist_push(list, text);
}

void verify_nspec_text(const bt_nspec *n, char *out, size_t out_len) {
    BT_TRACE_ENTER("packs.nspec_text");
    if (n->kind == BT_NSPEC_UNSET) {
        snprintf(out, out_len, "unset");
    } else if (n->kind == BT_NSPEC_FIXED) {
        snprintf(out, out_len, "n=%lld", n->fixed);
    } else {
        size_t used = (size_t)snprintf(out, out_len, "n=");
        for (size_t at = 0; at < n->size_count; at++) {
            used += (size_t)snprintf(out + used, out_len - used, "%s%lld", at == 0 ? "" : ",",
                                     n->sizes[at]);
        }
    }
    BT_TRACE_EXIT("packs.nspec_text");
}

static const verify_pack_pin *find_pin(const bt_pack_entry *entry) {
    for (size_t at = 0; at < VERIFY_PACK_PIN_COUNT; at++) {
        const verify_pack_pin *pin = &VERIFY_PACK_EXPECTED_ROWS[at];
        if (strcmp(pin->file, entry->file) == 0 && strcmp(pin->event, entry->event) == 0 &&
            strcmp(pin->method, entry->method) == 0 && strcmp(pin->stage, entry->stage) == 0) {
            return pin;
        }
    }
    return NULL;
}

void verify_pinned_vs_entries(const bt_pack_entry *entries, size_t count, verify_strlist *out) {
    BT_TRACE_ENTER("packs.pinned_vs_entries");
    verify_strlist_init(out);
    for (size_t at = 0; at < count; at++) {
        const bt_pack_entry *entry = &entries[at];
        const verify_pack_pin *pin = find_pin(entry);
        if (pin == NULL) {
            char nspec[64];
            verify_nspec_text(&entry->n, nspec, sizeof nspec);
            verify_strlist_pushf(out, "registry entry %s/%s/%s/%s (%s) is not pinned", entry->event,
                                 entry->method, entry->stage, entry->file, nspec);
            continue;
        }
        if (entry->expected_rows != pin->rows) {
            verify_strlist_pushf(out, "%s/%s/%s/%s registry holds %zu rows, pin holds %zu",
                                 entry->event, entry->method, entry->stage, entry->file,
                                 entry->expected_rows, pin->rows);
        }
    }
    for (size_t at = 0; at < VERIFY_PACK_PIN_COUNT; at++) {
        const verify_pack_pin *pin = &VERIFY_PACK_EXPECTED_ROWS[at];
        bool present = false;
        for (size_t entry_at = 0; entry_at < count; entry_at++) {
            const bt_pack_entry *entry = &entries[entry_at];
            if (strcmp(pin->file, entry->file) == 0 && strcmp(pin->event, entry->event) == 0 &&
                strcmp(pin->method, entry->method) == 0 && strcmp(pin->stage, entry->stage) == 0) {
                present = true;
                break;
            }
        }
        if (!present) {
            verify_strlist_pushf(out, "pin %s/%s/%s/%s is missing from the registry", pin->event,
                                 pin->method, pin->stage, pin->file);
        }
    }
    BT_TRACE_EXIT("packs.pinned_vs_entries");
}

void verify_pinned_vs_registry(const char *packs_root, verify_strlist *out) {
    BT_TRACE_ENTER("packs.pinned_vs_registry");
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs_root, &entries, &count, &err) != 0) {
        char text[512];
        bt_seed_error_string(&err, text, sizeof text);
        verify_strlist_init(out);
        verify_strlist_push(out, text);
        BT_TRACE_FAIL("packs.pinned_vs_registry", "registry load failed");
        BT_TRACE_EXIT("packs.pinned_vs_registry");
        return;
    }
    verify_pinned_vs_entries(entries, count, out);
    bt_seedpack_free_entries(entries);
    BT_TRACE_EXIT("packs.pinned_vs_registry");
}

void verify_rows(const bt_pack_entry *entry, const bt_seed_rows *rows, verify_section_result *out) {
    BT_TRACE_ENTER("packs.verify_rows");
    const char *event = entry->event;
    const char *method = entry->method;
    const char *stage = entry->stage;
    bool bigcube = (bool)(strcmp(event, "bigcube") == 0 &&
                          (strcmp(method, "yau") == 0 || strcmp(method, "reduction") == 0) &&
                          strcmp(stage, "parity") == 0);
    bool cfop = (bool)((strcmp(event, "3x3") == 0 || strcmp(event, "oh") == 0) &&
                       strcmp(method, "cfop") == 0);
    if (strcmp(event, "2x2") == 0 && strcmp(method, "cll") == 0 && strcmp(stage, "cll") == 0) {
        verify_nxn_verify_twoby2(rows, out);
    } else if (strcmp(event, "sq1") == 0 && strcmp(method, "lbl") == 0 &&
               strcmp(stage, "cubeshape") == 0) {
        verify_sq1_verify_cubeshape(rows, out);
    } else if (strcmp(event, "sq1") == 0 && strcmp(method, "lbl") == 0 &&
               strcmp(stage, "co") == 0) {
        verify_sq1_verify_co(rows, out);
    } else if (strcmp(event, "sq1") == 0 && strcmp(method, "lbl") == 0 &&
               strcmp(stage, "eo") == 0) {
        verify_sq1_verify_eo(rows, out);
    } else if (strcmp(event, "sq1") == 0 && strcmp(method, "lbl") == 0 &&
               strcmp(stage, "cp") == 0) {
        verify_sq1_verify_cp(rows, out);
    } else if (strcmp(event, "sq1") == 0 && strcmp(method, "lbl") == 0 &&
               strcmp(stage, "ep") == 0) {
        verify_sq1_verify_ep(rows, out);
    } else if (strcmp(event, "skewb") == 0 && strcmp(method, "sarah_intermediate") == 0 &&
               strcmp(stage, "sarah") == 0) {
        verify_skewb_verify_sarah(rows, out);
    } else if (strcmp(event, "pyraminx") == 0 && strcmp(method, "l4e") == 0 &&
               strcmp(stage, "l4e") == 0) {
        verify_pyra_verify_l4e(rows, out);
    } else if (strcmp(event, "megaminx") == 0 && strcmp(method, "4lll") == 0 &&
               strcmp(stage, "4lll") == 0) {
        verify_mega_verify_4lll(rows, out);
    } else if (strcmp(event, "fto") == 0 && strcmp(method, "l4t_redux") == 0 &&
               strcmp(stage, "l4t") == 0) {
        verify_fto_verify_l4t(rows, out);
    } else if (bigcube) {
        verify_bigcube_verify_parity(entry, rows, out);
    } else if (strcmp(event, "3bld") == 0 && strcmp(method, "old_pochmann") == 0 &&
               strcmp(stage, "concept") == 0) {
        verify_bld_verify_oldpoch(rows, out);
    } else if (strcmp(event, "3bld") == 0 && strcmp(method, "m2") == 0 &&
               strcmp(stage, "concept") == 0) {
        verify_bld_verify_m2(rows, out);
    } else if (strcmp(event, "3bld") == 0 && strcmp(method, "3style") == 0 &&
               strcmp(stage, "parity") == 0) {
        verify_bld_verify_3style(rows, out);
    } else if (cfop && strcmp(stage, "pll") == 0) {
        verify_cfop_verify_pll(rows, out);
    } else if (cfop && strcmp(stage, "oll") == 0) {
        verify_cfop_verify_oll(rows, out);
    } else if (cfop && strcmp(stage, "f2l") == 0) {
        verify_cfop_verify_f2l(rows, out);
    } else {
        BT_TRACE_FAIL("packs.verify_rows", "section %s/%s/%s is not implemented", event, method,
                      stage);
        verify_section_init(out);
        for (size_t at = 0; at < rows->count; at++) {
            const bt_seed_row *row = &rows->items[at];
            verify_row_outcome outcome = {0};
            snprintf(outcome.case_key, sizeof outcome.case_key, "%s", row->case_key);
            snprintf(outcome.tier1_err, sizeof outcome.tier1_err,
                     "section %s/%s/%s is not implemented", event, method, stage);
            outcome.tier2 = verify_tier2_skipped("section not implemented");
            verify_section_push_row(out, &outcome);
        }
    }
    BT_TRACE_EXIT("packs.verify_rows");
}
