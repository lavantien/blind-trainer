#ifndef MUTATE_TESTS_SHARED_H
#define MUTATE_TESTS_SHARED_H

#include "mutate/mutate.h"

#include "common/json.h"

#define FX_REL "tests/unit/tools/fixtures/fx_target.c"

enum {
    FX_LINE_ADD_0 = 6,
    FX_LINE_SPIN_1 = 13,
    FX_LINE_SPIN_WHILE = 11,
    FX_LINE_NOT_BANG = 19,
    FX_LINE_DEL_10 = 26,
    FX_LINE_DEL_STMT = 27,
    FX_LINE_UNUSED_3 = 36,
    FX_LINE_UNUSED_INC = 37,
};

enum {
    MUT_TEST_TIMEOUT_MS = 60000,
};

extern char g_root[BT_MUT_PATH_MAX];
extern char g_dir[BT_MUT_PATH_MAX];
extern bt_json_value *g_config;

void engine_for(const char *slot, unsigned long long timeout_ms, const char *clang_override,
                bt_mut_engine *engine);
void fixture_target(bt_mut_target *target);
void ws_fixture_target(bt_mut_target *target);
void enumerate_fixture(bt_mut_mutant_list *list);
bt_mut_row row_for(const char *ledger_path, const char *id);

#endif
