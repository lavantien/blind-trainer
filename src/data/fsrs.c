#include "data/fsrs.h"

#include "common/bt_trace.h"
#include "common/fsrs_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const double DEFAULT_WEIGHTS[BT_FSRS_WEIGHT_COUNT] = {
    0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046,  1.54575, 0.1192,
    1.01925, 1.9395,  0.11,  0.29605,  2.2698, 0.2315, 2.9898, 0.51655, 0.6621,
};

static const double DECAY = -0.5;

double bt_fsrs_factor(void) {
    return pow(0.9, 1.0 / DECAY) - 1.0;
}

void bt_fsrs_config_defaults(bt_fsrs_config *cfg) {
    BT_TRACE_ENTER("fsrs.config_defaults");
    memcpy(cfg->weights, DEFAULT_WEIGHTS, sizeof DEFAULT_WEIGHTS);
    cfg->request_retention = 0.9;
    cfg->interval_clamp_days[0] = 1;
    cfg->interval_clamp_days[1] = 36500;
    BT_TRACE_EXIT("fsrs.config_defaults");
}

static int read_weights(const bt_json_value *section, bt_fsrs_config *out, char *error,
                        size_t error_len) {
    const bt_json_value *weights = bt_json_get(section, "weights");
    if (weights == NULL || weights->type != BT_JSON_ARRAY ||
        weights->as.array.count != BT_FSRS_WEIGHT_COUNT) {
        snprintf(error, error_len, "config section malformed: weights must hold %d numbers",
                 BT_FSRS_WEIGHT_COUNT);
        return -1;
    }
    for (size_t i = 0; i < (size_t)BT_FSRS_WEIGHT_COUNT; i++) {
        const bt_json_value *item = bt_json_at(weights, i);
        if (item == NULL || item->type != BT_JSON_NUMBER) {
            snprintf(error, error_len, "config section malformed: weights[%zu] is not a number", i);
            return -1;
        }
        out->weights[i] = item->as.number;
    }
    return 0;
}

int bt_fsrs_config_from_json(const bt_json_value *config, bt_fsrs_config *out, char *error,
                             size_t error_len) {
    BT_TRACE_ENTER("fsrs.config_from_json");
    const bt_json_value *srs = bt_json_get(config, "srs");
    const bt_json_value *section = srs == NULL ? NULL : bt_json_get(srs, "fsrs");
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_len, "config section missing: srs.fsrs");
        BT_TRACE_FAIL("fsrs.config_from_json", "missing srs.fsrs");
        return -1;
    }
    bt_fsrs_config parsed;
    if (read_weights(section, &parsed, error, error_len) != 0) {
        BT_TRACE_FAIL("fsrs.config_from_json", "weights malformed");
        return -1;
    }
    const bt_json_value *retention = bt_json_get(section, "request_retention");
    if (retention == NULL || retention->type != BT_JSON_NUMBER) {
        snprintf(error, error_len, "config section malformed: request_retention is not a number");
        return -1;
    }
    parsed.request_retention = retention->as.number;
    const bt_json_value *clamp = bt_json_get(section, "interval_clamp_days");
    if (clamp == NULL || clamp->type != BT_JSON_ARRAY || clamp->as.array.count != 2) {
        snprintf(error, error_len,
                 "config section malformed: interval_clamp_days must hold 2 numbers");
        return -1;
    }
    for (int i = 0; i < 2; i++) {
        const bt_json_value *item = bt_json_at(clamp, (size_t)i);
        if (item == NULL || item->type != BT_JSON_NUMBER) {
            snprintf(error, error_len,
                     "config section malformed: interval_clamp_days[%d] is not a number", i);
            return -1;
        }
        parsed.interval_clamp_days[i] = (long long)item->as.number;
    }
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        if (!isfinite(parsed.weights[i])) {
            snprintf(error, error_len, "config section invalid: weights must be finite");
            BT_TRACE_FAIL("fsrs.config_from_json", "non finite weight");
            return -1;
        }
    }
    if (!isfinite(parsed.request_retention) || parsed.request_retention <= 0.0 ||
        parsed.request_retention > 1.0) {
        snprintf(error, error_len,
                 "config section invalid: request_retention must be finite and in (0, 1]");
        return -1;
    }
    if (parsed.interval_clamp_days[0] < 1 ||
        parsed.interval_clamp_days[1] < parsed.interval_clamp_days[0]) {
        snprintf(
            error, error_len,
            "config section invalid: interval_clamp_days must be [at least 1, at least the floor]");
        return -1;
    }
    *out = parsed;
    BT_TRACE_EXIT("fsrs.config_from_json");
    return 0;
}

static double clamp_difficulty(double difficulty) {
    if (difficulty < BT_FSRS_MIN_DIFFICULTY) {
        return BT_FSRS_MIN_DIFFICULTY;
    }
    if (difficulty > BT_FSRS_MAX_DIFFICULTY) {
        return BT_FSRS_MAX_DIFFICULTY;
    }
    return difficulty;
}

static double initial_difficulty(bt_grade grade, const double *w) {
    return clamp_difficulty(w[4] - exp(w[5] * ((double)grade - 1.0)) + 1.0);
}

static double next_difficulty(double difficulty, bt_grade grade, const double *w) {
    double delta = -(w[6] * ((double)grade - 3.0));
    double damped = difficulty + (BT_FSRS_MAX_DIFFICULTY - difficulty) * delta / 9.0;
    double anchor = initial_difficulty(BT_GRADE_EASY, w);
    return clamp_difficulty(w[7] * anchor + (1.0 - w[7]) * damped);
}

static double short_term_stability(double stability, bt_grade grade, const double *w) {
    return stability * exp(w[17] * ((double)grade - 3.0 + w[18]));
}

static double recall_stability(double stability, double difficulty, double retrievability,
                               bt_grade grade, const double *w) {
    double hard_penalty = grade == BT_GRADE_HARD ? w[15] : 1.0;
    double easy_bonus = grade == BT_GRADE_EASY ? w[16] : 1.0;
    return stability *
           (1.0 + exp(w[8]) * (11.0 - difficulty) * pow(stability, -w[9]) *
                      (exp((1.0 - retrievability) * w[10]) - 1.0) * hard_penalty * easy_bonus);
}

static double forget_stability(double stability, double difficulty, double retrievability,
                               const double *w) {
    return w[11] * pow(difficulty, -w[12]) * (pow(stability + 1.0, w[13]) - 1.0) *
           exp((1.0 - retrievability) * w[14]);
}

int bt_fsrs_memory_is_valid(bt_fsrs_memory memory) {
    return isfinite(memory.stability) && memory.stability > 0.0 &&
           memory.difficulty >= BT_FSRS_MIN_DIFFICULTY &&
           memory.difficulty <= BT_FSRS_MAX_DIFFICULTY;
}

bt_fsrs_memory bt_fsrs_memory_initial(bt_grade grade, const bt_fsrs_config *cfg) {
    BT_TRACE_ENTER("fsrs.memory_initial");
    double stability = cfg->weights[(size_t)grade - 1];
    if (stability < BT_FSRS_MIN_INITIAL_STABILITY) {
        stability = BT_FSRS_MIN_INITIAL_STABILITY;
    }
    bt_fsrs_memory out = {stability, initial_difficulty(grade, cfg->weights)};
    BT_TRACE_EXIT("fsrs.memory_initial");
    return out;
}

double bt_fsrs_memory_retrievability(bt_fsrs_memory memory, double elapsed_days) {
    double elapsed = floor(elapsed_days);
    if (elapsed < 0.0) {
        elapsed = 0.0;
    }
    return pow(1.0 + bt_fsrs_factor() * elapsed / memory.stability, DECAY);
}

bt_fsrs_memory bt_fsrs_memory_review(bt_fsrs_memory memory, bt_grade grade, double elapsed_days,
                                     const bt_fsrs_config *cfg) {
    BT_TRACE_ENTER("fsrs.memory_review");
    const double *w = cfg->weights;
    double difficulty = next_difficulty(memory.difficulty, grade, w);
    double stability;
    if (elapsed_days < 1.0) {
        stability = short_term_stability(memory.stability, grade, w);
    } else {
        double retrievability = bt_fsrs_memory_retrievability(memory, elapsed_days);
        if (grade == BT_GRADE_AGAIN) {
            double lapsed =
                forget_stability(memory.stability, memory.difficulty, retrievability, w);
            double ceiling = memory.stability / exp(w[17] * w[18]);
            stability = lapsed < ceiling ? lapsed : ceiling;
        } else {
            stability =
                recall_stability(memory.stability, memory.difficulty, retrievability, grade, w);
        }
    }
    bt_fsrs_memory out = {stability, difficulty};
    BT_TRACE_EXIT("fsrs.memory_review");
    return out;
}

long long bt_fsrs_next_interval_days(double stability, const bt_fsrs_config *cfg) {
    BT_TRACE_ENTER("fsrs.next_interval_days");
    double raw = (stability / bt_fsrs_factor()) * (pow(cfg->request_retention, 1.0 / DECAY) - 1.0);
    long long rounded = (long long)bt_round_half_even(raw);
    long long floor = cfg->interval_clamp_days[0] > 1 ? cfg->interval_clamp_days[0] : 1;
    if (rounded < floor) {
        rounded = floor;
    }
    if (rounded > cfg->interval_clamp_days[1]) {
        rounded = cfg->interval_clamp_days[1];
    }
    BT_TRACE_EXIT("fsrs.next_interval_days");
    return rounded;
}
