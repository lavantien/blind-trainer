#include "verify_test_util.h"

#include <ctype.h>
#include <string.h>

#include "common/json.h"
#include "data/seedpack/parse.h"
#include "data/seedpack/registry.h"
#include "data/seedpack/rulings.h"
#include "verify/lib.h"
#include "verify/packs.h"
#include "verify/packs_report.h"

enum {
    PYRA_OPEN_COUNT = 8,
    MEGA_OPEN_COUNT = 4,
    YAU_OPEN_COUNT = 5,
    REDUCTION_OPEN_COUNT = 12,
    M2_OPEN_COUNT = 4,
    TV_MAX_TOKENS = 48,
    TV_TOKEN_MAX = 32,
    TV_ALG_MAX = 512,
};

static const char *const PYRA_OPEN[PYRA_OPEN_COUNT] = {
    "l4e.06", "l4e.11", "l4e.21", "l4e.22", "l4e.24", "l4e.25", "l4e.28", "l4e.35",
};
static const char *const MEGA_OPEN[MEGA_OPEN_COUNT] = {
    "4lll.20",
    "4lll.21",
    "4lll.22",
    "4lll.24",
};
static const char *const YAU_OPEN[YAU_OPEN_COUNT] = {
    "parity.oll4", "parity.oll4flip", "parity.pll4", "parity.pll4adj", "parity.wing5",
};
static const char *const REDUCTION_OPEN[REDUCTION_OPEN_COUNT] = {
    "l2e.01", "l2e.02", "l2e.03", "l2e.04", "l2e.05", "l2e.06",
    "l2e.07", "l2e.08", "l2e.09", "l2e.10", "l2e.11", "l2e.12",
};
static const char *const M2_OPEN[M2_OPEN_COUNT] = {
    "m2.18",
    "m2.19",
    "m2.20",
    "m2.22",
};

static bool key_listed(const char *const *keys, size_t count, const char *key) {
    for (size_t at = 0; at < count; at++) {
        if (strcmp(keys[at], key) == 0) {
            return true;
        }
    }
    return false;
}

static void entry_of(const char *event, const char *method, const char *stage, bt_pack_entry *out) {
    char packs[TV_PATH_MAX];
    tv_packs_root(packs);
    bt_pack_entry *entries = NULL;
    size_t count = 0;
    bt_seed_error err;
    if (bt_seedpack_load_registry(packs, &entries, &count, &err) != 0) {
        BT_ABORT("registry loads");
    }
    bool found = false;
    for (size_t at = 0; at < count; at++) {
        if (strcmp(entries[at].event, event) == 0 && strcmp(entries[at].method, method) == 0 &&
            strcmp(entries[at].stage, stage) == 0) {
            *out = entries[at];
            found = true;
            break;
        }
    }
    bt_seedpack_free_entries(entries);
    if (!found) {
        BT_ABORT("registry entry %s/%s/%s exists", event, method, stage);
    }
}

static void load_section(const char *event, const char *method, const char *stage,
                         bt_pack_entry *entry, bt_seed_rows *rows, verify_section_result *section) {
    char packs[TV_PATH_MAX];
    tv_packs_root(packs);
    entry_of(event, method, stage, entry);
    bt_seed_error err;
    if (bt_seedpack_load_pack(packs, entry, rows, &err) != 0) {
        BT_ABORT("%s/%s/%s pack loads", event, method, stage);
    }
    verify_rows(entry, rows, section);
}

static const bt_seed_row *row_of(const bt_seed_rows *rows, const char *key) {
    for (size_t at = 0; at < rows->count; at++) {
        if (strcmp(rows->items[at].case_key, key) == 0) {
            return &rows->items[at];
        }
    }
    BT_ABORT("row %s exists", key);
    return NULL;
}

BT_TEST(verify_packs_sections, pinned_table_covers_the_registry_exactly) {
    BT_CHECK_EQ(VERIFY_PACK_PIN_COUNT, 21);
    size_t total = 0;
    for (size_t at = 0; at < VERIFY_PACK_PIN_COUNT; at++) {
        total += VERIFY_PACK_EXPECTED_ROWS[at].rows;
    }
    BT_CHECK_EQ(total, 1115);
    char packs[TV_PATH_MAX];
    tv_packs_root(packs);
    verify_strlist drift;
    verify_strlist_init(&drift);
    verify_pinned_vs_registry(packs, &drift);
    BT_CHECK_EQ(drift.count, 0);
    verify_strlist_free(&drift);
}

BT_TEST(verify_packs_sections, twoby2_cll_rows_pass_tier1_and_the_bottom_layer_predicate) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("2x2", "cll", "cll", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 42);
    BT_CHECK_EQ(section.honesty_count, 0);
    for (size_t at = 0; at < section.count; at++) {
        BT_CHECK(section.rows[at].tier1_ok);
        BT_CHECK(section.rows[at].tier2.kind == VERIFY_TIER2_PASS);
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, twoby2_tier2_rejects_a_case_with_no_solved_layer) {
    bt_pack_entry probe;
    memset(&probe, 0, sizeof probe);
    snprintf(probe.file, sizeof probe.file, "%s", "probe.tsv");
    snprintf(probe.event, sizeof probe.event, "%s", "2x2");
    snprintf(probe.method, sizeof probe.method, "%s", "cll");
    snprintf(probe.stage, sizeof probe.stage, "%s", "cll");
    probe.n.kind = BT_NSPEC_UNSET;
    probe.expected_rows = 1;
    const char *text = "case_key\talg\tfront_json\tsource_url\tsource_at\n"
                       "cll.probe\tR U R'\t{\"case\":\"P\"}\thttps://example.test\t2026-10-10\n";
    bt_seed_rows rows;
    bt_seed_error err;
    if (bt_seedpack_parse_pack(&probe, text, &rows, &err) != 0) {
        char reason[512];
        bt_seed_error_string(&err, reason, sizeof reason);
        BT_ABORT("probe pack parses: %s", reason);
    }
    verify_section_result section;
    verify_rows(&probe, &rows, &section);
    BT_CHECK_EQ(section.count, 1);
    BT_CHECK(section.rows[0].tier1_ok);
    BT_CHECK(section.rows[0].tier2.kind == VERIFY_TIER2_FAIL);
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, sq1_cubeshape_fails_exactly_the_nine_ruled_rows) {
    static const char *const FLIPPED[8] = {
        "cs.075", "cs.078", "cs.084", "cs.089", "cs.097", "cs.109", "cs.118", "cs.129",
    };
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("sq1", "lbl", "cubeshape", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 169);
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        if (strcmp(row->case_key, "cs.119") == 0) {
            BT_CHECK(!row->tier1_ok);
            BT_CHECK(row->tier2.kind == VERIFY_TIER2_SKIPPED);
        } else if (key_listed(FLIPPED, 8, row->case_key)) {
            BT_CHECK(row->tier1_ok);
            BT_CHECK(row->tier2.kind == VERIFY_TIER2_FAIL);
        } else {
            BT_CHECK(row->tier1_ok);
            BT_CHECK(row->tier2.kind == VERIFY_TIER2_PASS);
        }
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, sq1_stage_rows_hold_their_stage_start_state) {
    static const struct {
        const char *stage;
        size_t count;
    } STAGES[4] = {{"co", 7}, {"eo", 7}, {"cp", 16}, {"ep", 99}};
    size_t ruled = 0;
    size_t failures = 0;
    for (size_t at = 0; at < 4; at++) {
        bt_pack_entry entry;
        bt_seed_rows rows;
        verify_section_result section;
        load_section("sq1", "lbl", STAGES[at].stage, &entry, &rows, &section);
        BT_CHECK_EQ(section.count, STAGES[at].count);
        for (size_t row_at = 0; row_at < section.count; row_at++) {
            const verify_row_outcome *row = &section.rows[row_at];
            const char *reason =
                bt_expected_failure(entry.event, entry.method, entry.stage, row->case_key);
            bool failed = (bool)(!row->tier1_ok || row->tier2.kind != VERIFY_TIER2_PASS);
            if (failed) {
                if (reason != NULL) {
                    BT_CHECK(!row->tier1_ok);
                    BT_CHECK(reason[0] != '\0');
                    ruled += 1;
                } else {
                    failures += 1;
                }
            } else if (reason != NULL) {
                failures += 1;
            }
        }
        verify_section_free(&section);
        bt_seed_rows_free(&rows);
    }
    BT_CHECK_EQ(ruled, 9);
    BT_CHECK_EQ(failures, 0);
}

BT_TEST(verify_packs_sections, skewb_sarah_rows_pass_tier1_with_tier2_declared_skipped) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("skewb", "sarah_intermediate", "sarah", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 15);
    BT_CHECK_EQ(section.honesty_count, 1);
    for (size_t at = 0; at < section.count; at++) {
        BT_CHECK(section.rows[at].tier1_ok);
        BT_CHECK(section.rows[at].tier2.kind == VERIFY_TIER2_SKIPPED);
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, pyraminx_l4e_rows_hold_the_four_edge_case_shape) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("pyraminx", "l4e", "l4e", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 35);
    BT_CHECK_EQ(section.honesty_count, 1);
    size_t failing = 0;
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        bool passing = (bool)(row->tier1_ok && row->tier2.kind == VERIFY_TIER2_PASS);
        if (key_listed(PYRA_OPEN, PYRA_OPEN_COUNT, row->case_key)) {
            BT_CHECK(!passing);
            failing += 1;
        } else {
            BT_CHECK(passing);
        }
    }
    BT_CHECK_EQ(failing, PYRA_OPEN_COUNT);
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, megaminx_4lll_rows_hold_their_stage_start_state) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("megaminx", "4lll", "4lll", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 39);
    BT_CHECK_EQ(section.honesty_count, 1);
    size_t failing = 0;
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        bool passing = (bool)(row->tier1_ok && row->tier2.kind == VERIFY_TIER2_PASS);
        if (key_listed(MEGA_OPEN, MEGA_OPEN_COUNT, row->case_key)) {
            BT_CHECK(!passing);
            failing += 1;
        } else {
            BT_CHECK(passing);
        }
    }
    BT_CHECK_EQ(failing, MEGA_OPEN_COUNT);
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, fto_l4t_rows_pass_tier1_with_tier2_declared_skipped) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("fto", "l4t_redux", "l4t", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 36);
    BT_CHECK_EQ(section.honesty_count, 1);
    for (size_t at = 0; at < section.count; at++) {
        BT_CHECK(section.rows[at].tier1_ok);
        BT_CHECK(section.rows[at].tier2.kind == VERIFY_TIER2_SKIPPED);
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, yau_parity_rows_hold_the_reduction_invariants) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("bigcube", "yau", "parity", &entry, &rows, &section);
    size_t failing = 0;
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        bool passing = (bool)(row->tier1_ok && row->tier2.kind == VERIFY_TIER2_PASS);
        if (key_listed(YAU_OPEN, YAU_OPEN_COUNT, row->case_key)) {
            BT_CHECK(!passing);
            failing += 1;
        } else {
            BT_CHECK(passing);
        }
    }
    BT_CHECK_EQ(failing, YAU_OPEN_COUNT);
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, reduction_l2e_rows_hold_the_reduction_invariants) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("bigcube", "reduction", "parity", &entry, &rows, &section);
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        bool passing = (bool)(row->tier1_ok && row->tier2.kind == VERIFY_TIER2_PASS);
        if (key_listed(REDUCTION_OPEN, REDUCTION_OPEN_COUNT, row->case_key)) {
            BT_CHECK(!passing);
        } else {
            BT_CHECK(passing);
        }
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, oldpoch_rows_split_into_swaps_specials_letters_and_prose) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("3bld", "old_pochmann", "concept", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 40);
    size_t prose = 0;
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        if (row->tier2.kind == VERIFY_TIER2_SKIPPED) {
            const bt_seed_row *raw = row_of(&rows, row->case_key);
            if (raw->alg[0] == '\0') {
                prose += 1;
            }
        }
    }
    BT_CHECK_EQ(prose, 2);
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        const bt_seed_row *raw = row_of(&rows, row->case_key);
        if (raw->alg[0] == '\0') {
            BT_CHECK(!row->tier1_ok);
            continue;
        }
        BT_CHECK(row->tier1_ok);
        bt_json_value *front = NULL;
        bt_json_error json_err;
        bt_json_value *probe = NULL;
        if (bt_json_parse(raw->front_json, strlen(raw->front_json), &probe, &json_err) != 0) {
            BT_ABORT("front json parses for %s", row->case_key);
        }
        front = bt_json_get(probe, "letter");
        bool letter = front != NULL;
        bt_json_free(probe);
        if (letter) {
            BT_CHECK(row->tier2.kind == VERIFY_TIER2_SKIPPED);
        } else {
            BT_CHECK(row->tier2.kind == VERIFY_TIER2_PASS);
        }
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, m2_rows_deliver_their_targets_into_the_buffer) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("3bld", "m2", "concept", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 24);
    size_t failing = 0;
    for (size_t at = 0; at < section.count; at++) {
        const verify_row_outcome *row = &section.rows[at];
        bool passing = (bool)(row->tier1_ok && row->tier2.kind == VERIFY_TIER2_PASS);
        if (key_listed(M2_OPEN, M2_OPEN_COUNT, row->case_key)) {
            BT_CHECK(!passing);
            failing += 1;
        } else {
            BT_CHECK(passing);
        }
    }
    BT_CHECK_EQ(failing, M2_OPEN_COUNT);
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, style_specials_hold_their_declared_shapes) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("3bld", "3style", "parity", &entry, &rows, &section);
    BT_CHECK_EQ(section.count, 85);
    for (size_t at = 0; at < section.count; at++) {
        BT_CHECK(section.rows[at].tier1_ok);
        BT_CHECK(section.rows[at].tier2.kind == VERIFY_TIER2_PASS);
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, cfop_rows_hold_their_step_start_states) {
    static const struct {
        const char *event;
        const char *stage;
        size_t count;
    } CASES[6] = {
        {"3x3", "pll", 21}, {"3x3", "oll", 57}, {"3x3", "f2l", 164},
        {"oh", "pll", 21},  {"oh", "oll", 57},  {"oh", "f2l", 164},
    };
    for (size_t at = 0; at < 6; at++) {
        bt_pack_entry entry;
        bt_seed_rows rows;
        verify_section_result section;
        load_section(CASES[at].event, "cfop", CASES[at].stage, &entry, &rows, &section);
        BT_CHECK_EQ(section.count, CASES[at].count);
        for (size_t row_at = 0; row_at < section.count; row_at++) {
            BT_CHECK(section.rows[row_at].tier1_ok);
            BT_CHECK(section.rows[row_at].tier2.kind == VERIFY_TIER2_PASS);
        }
        verify_section_free(&section);
        bt_seed_rows_free(&rows);
    }
}

BT_TEST(verify_packs_sections, the_live_pack_audit_is_fail_closed_clean) {
    char packs[TV_PATH_MAX];
    tv_packs_root(packs);
    verify_pack_audit audit;
    verify_audit_packs(packs, &audit);
    BT_CHECK_EQ(audit.drift.count, 0);
    BT_CHECK_EQ(audit.offenders.count, 0);
    BT_CHECK_EQ(audit.section_count, VERIFY_PACK_PIN_COUNT);
    size_t ruled = 0;
    for (size_t at = 0; at < audit.section_count; at++) {
        const verify_pack_section_summary *summary = &audit.sections[at];
        ruled += summary->ruled;
        BT_CHECK_EQ(summary->rows,
                    summary->passed + summary->failed + summary->skipped + summary->excused);
    }
    BT_CHECK_EQ(ruled, 51);
    static const struct {
        const char *event;
        const char *method;
        const char *stage;
        size_t open;
    } RULED_KEYS[5] = {
        {"megaminx", "4lll", "4lll", MEGA_OPEN_COUNT},
        {"pyraminx", "l4e", "l4e", PYRA_OPEN_COUNT},
        {"bigcube", "yau", "parity", YAU_OPEN_COUNT},
        {"bigcube", "reduction", "parity", REDUCTION_OPEN_COUNT},
        {"3bld", "m2", "concept", M2_OPEN_COUNT},
    };
    for (size_t at = 0; at < 5; at++) {
        const verify_pack_section_summary *summary = NULL;
        for (size_t section_at = 0; section_at < audit.section_count; section_at++) {
            const verify_pack_section_summary *candidate = &audit.sections[section_at];
            if (strcmp(candidate->event, RULED_KEYS[at].event) == 0 &&
                strcmp(candidate->method, RULED_KEYS[at].method) == 0 &&
                strcmp(candidate->stage, RULED_KEYS[at].stage) == 0) {
                summary = candidate;
                break;
            }
        }
        if (summary == NULL) {
            BT_ABORT("%s/%s/%s summary exists", RULED_KEYS[at].event, RULED_KEYS[at].method,
                     RULED_KEYS[at].stage);
        }
        BT_CHECK_EQ(summary->ruled, RULED_KEYS[at].open);
        BT_CHECK_EQ(summary->failed, RULED_KEYS[at].open);
    }
    verify_pack_audit_free(&audit);
}

static size_t split_tokens(const char *alg, char out[TV_MAX_TOKENS][TV_TOKEN_MAX]) {
    size_t count = 0;
    const char *cursor = alg;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor += 1;
        }
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            cursor += 1;
        }
        size_t len = (size_t)(cursor - start);
        if (len == 0) {
            break;
        }
        snprintf(out[count], TV_TOKEN_MAX, "%.*s", (int)len, start);
        count += 1;
    }
    return count;
}

static size_t letter_prefix_len(const char *token) {
    size_t len = 0;
    while (token[len] != '\0' && (isalpha((unsigned char)token[len]) != 0 || token[len] == 'w')) {
        len += 1;
    }
    return len;
}

static size_t reconcile_flips(const bt_pack_entry *entry, const bt_seed_row *row) {
    char tokens[TV_MAX_TOKENS][TV_TOKEN_MAX];
    size_t count = split_tokens(row->alg, tokens);
    static const char *const SUFFIXES[4] = {"", "'", "2", "2'"};
    size_t reconciled = 0;
    for (size_t index = 0; index < count; index++) {
        size_t letter = letter_prefix_len(tokens[index]);
        if (letter == 0) {
            continue;
        }
        char stem[TV_TOKEN_MAX];
        snprintf(stem, sizeof stem, "%.*s", (int)letter, tokens[index]);
        for (size_t suffix_at = 0; suffix_at < 4; suffix_at++) {
            char candidate[TV_TOKEN_MAX];
            snprintf(candidate, sizeof candidate, "%s%s", stem, SUFFIXES[suffix_at]);
            if (strcmp(candidate, tokens[index]) == 0) {
                continue;
            }
            char variant[TV_ALG_MAX];
            variant[0] = '\0';
            for (size_t at = 0; at < count; at++) {
                if (variant[0] != '\0') {
                    snprintf(variant + strlen(variant), sizeof variant - strlen(variant), " ");
                }
                snprintf(variant + strlen(variant), sizeof variant - strlen(variant), "%s",
                         at == index ? candidate : tokens[at]);
            }
            bt_seed_rows probe_rows_holder;
            bt_seed_row probe = *row;
            probe.alg = variant;
            probe_rows_holder.items = &probe;
            probe_rows_holder.count = 1;
            verify_section_result section;
            verify_rows(entry, &probe_rows_holder, &section);
            if (section.rows[0].tier1_ok && section.rows[0].tier2.kind == VERIFY_TIER2_PASS) {
                reconciled += 1;
            }
            verify_section_free(&section);
        }
    }
    return reconciled;
}

BT_TEST(verify_packs_sections, no_single_token_flip_reconciles_the_ruled_mega_rows) {
    bt_pack_entry entry;
    bt_seed_rows rows;
    verify_section_result section;
    load_section("megaminx", "4lll", "4lll", &entry, &rows, &section);
    for (size_t at = 0; at < MEGA_OPEN_COUNT; at++) {
        const bt_seed_row *row = row_of(&rows, MEGA_OPEN[at]);
        BT_CHECK_EQ(reconcile_flips(&entry, row), 0);
    }
    verify_section_free(&section);
    bt_seed_rows_free(&rows);
}

BT_TEST(verify_packs_sections, no_global_conjugation_reconciles_the_ruled_bigcube_rows) {
    static const struct {
        const char *rotation;
        const char *inverse;
    } ROTATIONS[6] = {
        {"x", "x'"}, {"x'", "x"}, {"x2", "x2"}, {"y", "y'"}, {"y2", "y2"}, {"z", "z'"},
    };
    bt_pack_entry entries[2];
    bt_seed_rows packs[2];
    verify_section_result sections[2];
    load_section("bigcube", "yau", "parity", &entries[0], &packs[0], &sections[0]);
    load_section("bigcube", "reduction", "parity", &entries[1], &packs[1], &sections[1]);
    for (size_t rotation_at = 0; rotation_at < 6; rotation_at++) {
        size_t reconciled = 0;
        for (size_t pack_at = 0; pack_at < 2; pack_at++) {
            for (size_t row_at = 0; row_at < packs[pack_at].count; row_at++) {
                const bt_seed_row *row = &packs[pack_at].items[row_at];
                char variant[TV_ALG_MAX];
                snprintf(variant, sizeof variant, "%s %s %s", ROTATIONS[rotation_at].rotation,
                         row->alg, ROTATIONS[rotation_at].inverse);
                bt_seed_rows probe_rows_holder;
                bt_seed_row probe = *row;
                probe.alg = variant;
                probe_rows_holder.items = &probe;
                probe_rows_holder.count = 1;
                verify_section_result section;
                verify_rows(&entries[pack_at], &probe_rows_holder, &section);
                if (section.rows[0].tier1_ok && section.rows[0].tier2.kind == VERIFY_TIER2_PASS) {
                    reconciled += 1;
                }
                verify_section_free(&section);
            }
        }
        BT_CHECK_EQ(reconciled, 0);
    }
    for (size_t pack_at = 0; pack_at < 2; pack_at++) {
        verify_section_free(&sections[pack_at]);
        bt_seed_rows_free(&packs[pack_at]);
    }
}
