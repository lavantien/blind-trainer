#ifndef BT_DATA_SEEDPACK_RULINGS_H
#define BT_DATA_SEEDPACK_RULINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    BT_FAILURE_PARSE,
    BT_FAILURE_APPLY,
    BT_FAILURE_TIER2,
} bt_failure_class;

const char *bt_failure_class_name(bt_failure_class class);
bool bt_classify_failure(const char *tier1_err, bool tier2_failed, bt_failure_class *out);
typedef struct {
    const char *event;
    const char *method;
    const char *stage;
    const char *case_key;
    bt_failure_class class;
    const char *reason;
} bt_expected_failure_row;
const bt_expected_failure_row *bt_seedpack_expected_failures(size_t *count);
const char *bt_expected_failure(const char *event, const char *method, const char *stage,
                                const char *case_key);
bool bt_expected_failure_class(const char *event, const char *method, const char *stage,
                               const char *case_key, bt_failure_class *out);

#endif
