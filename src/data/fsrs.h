#ifndef BT_DATA_FSRS_H
#define BT_DATA_FSRS_H

#include "common/json.h"

#include <stddef.h>

#define BT_FSRS_WEIGHT_COUNT 19
#define BT_FSRS_MIN_INITIAL_STABILITY 0.1
#define BT_FSRS_MIN_DIFFICULTY 1.0
#define BT_FSRS_MAX_DIFFICULTY 10.0

typedef enum {
    BT_GRADE_AGAIN = 1,
    BT_GRADE_HARD = 2,
    BT_GRADE_GOOD = 3,
    BT_GRADE_EASY = 4,
} bt_grade;

typedef struct {
    double weights[BT_FSRS_WEIGHT_COUNT];
    double request_retention;
    long long interval_clamp_days[2];
} bt_fsrs_config;

typedef struct {
    double stability;
    double difficulty;
} bt_fsrs_memory;

void bt_fsrs_config_defaults(bt_fsrs_config *cfg);
int bt_fsrs_config_from_json(const bt_json_value *config, bt_fsrs_config *out, char *error,
                             size_t error_len);
double bt_fsrs_factor(void);
int bt_fsrs_memory_is_valid(bt_fsrs_memory memory);
bt_fsrs_memory bt_fsrs_memory_initial(bt_grade grade, const bt_fsrs_config *cfg);
double bt_fsrs_memory_retrievability(bt_fsrs_memory memory, double elapsed_days);
bt_fsrs_memory bt_fsrs_memory_review(bt_fsrs_memory memory, bt_grade grade, double elapsed_days,
                                     const bt_fsrs_config *cfg);
long long bt_fsrs_next_interval_days(double stability, const bt_fsrs_config *cfg);

#endif
