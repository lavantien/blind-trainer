#ifndef MUTANTS_GATE_LIB_H
#define MUTANTS_GATE_LIB_H

#include <stddef.h>

#define BT_GATE_SUMMARY_MAX 64

typedef enum {
    BT_GATE_OK = 0,
    BT_GATE_VIOLATION = 1,
    BT_GATE_USAGE = 2,
} bt_gate_kind;

int bt_gate_run(const char *outcomes_json, const char *allowlist_text, char *summary,
                size_t summary_cap);
int bt_gate_merge(const char *const *inputs, size_t input_count, char **json_out,
                  unsigned long long *total_mutants);
const char *bt_gate_last_error(void);

#endif
