#ifndef VERIFY_PACKS_H
#define VERIFY_PACKS_H

#include <stdbool.h>
#include <stddef.h>

#include "data/seedpack/mod.h"
#include "data/seedpack/registry.h"

#define VERIFY_ERR_MAX 256
#define VERIFY_OFFENDER_MAX 256
#define VERIFY_HONESTY_MAX 256
#define VERIFY_HONESTY_CAP 96

#define VERIFY_PROSE_ALG "prose row holds no mechanical alg"

typedef enum {
    VERIFY_PUZZLE_CORE,
    VERIFY_PUZZLE_NXN,
    VERIFY_PUZZLE_SQ1,
    VERIFY_PUZZLE_PYRA,
    VERIFY_PUZZLE_SKEWB,
    VERIFY_PUZZLE_MEGA,
    VERIFY_PUZZLE_FTO,
} verify_puzzle;

int verify_puzzle_for_event(const char *event, verify_puzzle *out, char *err, size_t err_len);

typedef enum {
    VERIFY_TIER2_PASS,
    VERIFY_TIER2_FAIL,
    VERIFY_TIER2_SKIPPED,
} verify_tier2_kind;

typedef struct {
    verify_tier2_kind kind;
    char fail[VERIFY_ERR_MAX];
    const char *skip;
} verify_tier2;

verify_tier2 verify_tier2_pass(void);
verify_tier2 verify_tier2_fail(const char *message);
verify_tier2 verify_tier2_skipped(const char *reason);

typedef struct {
    char case_key[BT_SEED_CASE_KEY_MAX];
    bool tier1_ok;
    char tier1_err[VERIFY_ERR_MAX];
    verify_tier2 tier2;
} verify_row_outcome;

typedef struct {
    verify_row_outcome *rows;
    size_t count;
    size_t cap;
    char honesty[VERIFY_HONESTY_CAP][VERIFY_HONESTY_MAX];
    size_t honesty_count;
} verify_section_result;

void verify_section_init(verify_section_result *section);
void verify_section_free(verify_section_result *section);
void verify_section_push_row(verify_section_result *section, const verify_row_outcome *row);
void verify_section_push_honesty(verify_section_result *section, const char *note);

typedef struct {
    char (*items)[VERIFY_OFFENDER_MAX];
    size_t count;
    size_t cap;
} verify_strlist;

void verify_strlist_init(verify_strlist *list);
void verify_strlist_free(verify_strlist *list);
void verify_strlist_push(verify_strlist *list, const char *text);
void verify_strlist_pushf(verify_strlist *list, const char *fmt, ...);

typedef struct {
    const char *file;
    const char *event;
    const char *method;
    const char *stage;
    size_t rows;
} verify_pack_pin;

extern const verify_pack_pin VERIFY_PACK_EXPECTED_ROWS[21];
extern const size_t VERIFY_PACK_PIN_COUNT;

void verify_nspec_text(const bt_nspec *n, char *out, size_t out_len);
void verify_pinned_vs_entries(const bt_pack_entry *entries, size_t count, verify_strlist *out);
void verify_pinned_vs_registry(const char *packs_root, verify_strlist *out);
void verify_rows(const bt_pack_entry *entry, const bt_seed_rows *rows,
                 verify_section_result *out);

#endif
