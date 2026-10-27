#include "sleeper/lib.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"

enum {
    BT_SLEEPER_SECS_NUMERIC = 0,
    BT_SLEEPER_SECS_NOT_A_NUMBER = 1,
    BT_SLEEPER_SECS_BAD_VALUE = 2,
};

static int classify_secs(const char *raw, double *out) {
    char *end = NULL;
    double parsed = strtod(raw, &end);
    if (end == raw || *end != '\0' || strchr(raw, 'x') != NULL || strchr(raw, 'X') != NULL ||
        isspace((unsigned char)raw[0]) != 0) {
        return BT_SLEEPER_SECS_NOT_A_NUMBER;
    }
    if (!isfinite(parsed) || parsed < 0.0) {
        return BT_SLEEPER_SECS_BAD_VALUE;
    }
    *out = parsed;
    return BT_SLEEPER_SECS_NUMERIC;
}

static int parse_u64_strict(const char *raw, unsigned long long *out) {
    if (raw[0] == '\0') {
        return 0;
    }
    unsigned long long value = 0;
    for (const char *cursor = raw; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
        unsigned long long digit = (unsigned long long)(*cursor - '0');
        if (value > (ULLONG_MAX - digit) / 10ULL) {
            return 0;
        }
        value = value * 10ULL + digit;
    }
    *out = value;
    return 1;
}

int bt_sleeper_parse_args(int argc, char *const argv[], double *secs, char *err, size_t err_cap) {
    BT_TRACE_ENTER("sleeper.parse_args");
    if (argc != 1) {
        snprintf(err, err_cap, "expected 1 argument <secs>, got %d", argc);
        BT_TRACE_FAIL("sleeper.parse_args", "argc %d", argc);
        BT_TRACE_EXIT("sleeper.parse_args");
        return -1;
    }
    double parsed = 0.0;
    int verdict = classify_secs(argv[0], &parsed);
    if (verdict == BT_SLEEPER_SECS_NOT_A_NUMBER) {
        snprintf(err, err_cap, "secs is not a number: %s", argv[0]);
        BT_TRACE_FAIL("sleeper.parse_args", "not a number: %s", argv[0]);
        BT_TRACE_EXIT("sleeper.parse_args");
        return -1;
    }
    if (verdict == BT_SLEEPER_SECS_BAD_VALUE) {
        snprintf(err, err_cap, "secs must be finite and non-negative: %s", argv[0]);
        BT_TRACE_FAIL("sleeper.parse_args", "bad value: %s", argv[0]);
        BT_TRACE_EXIT("sleeper.parse_args");
        return -1;
    }
    *secs = parsed;
    BT_TRACE_EXIT("sleeper.parse_args");
    return 0;
}

int bt_sleeper_parse_mode(int argc, char *const argv[], int *mode, unsigned long long *value,
                          char *err, size_t err_cap) {
    BT_TRACE_ENTER("sleeper.parse_mode");
    int kind = BT_SLEEPER_PLAIN;
    const char *label = "";
    unsigned long long minimum = 0;
    if (argv[0] != NULL && strcmp(argv[0], "--selftest-alloc-mb") == 0) {
        kind = BT_SLEEPER_ALLOC_MB;
        label = "alloc mb";
        minimum = 1;
    } else if (argv[0] != NULL && strcmp(argv[0], "--selftest-hold-secs") == 0) {
        kind = BT_SLEEPER_HOLD_SECS;
        label = "hold secs";
    } else {
        *mode = BT_SLEEPER_PLAIN;
        BT_TRACE_EXIT("sleeper.parse_mode");
        return 0;
    }
    if (argc != 2) {
        snprintf(err, err_cap, "selftest %s takes exactly one argument", label);
        BT_TRACE_FAIL("sleeper.parse_mode", "selftest %s argc %d", label, argc);
        BT_TRACE_EXIT("sleeper.parse_mode");
        return -1;
    }
    unsigned long long parsed = 0;
    if (parse_u64_strict(argv[1], &parsed) == 0) {
        snprintf(err, err_cap, "selftest %s is not a number: %s", label, argv[1]);
        BT_TRACE_FAIL("sleeper.parse_mode", "selftest %s not a number: %s", label, argv[1]);
        BT_TRACE_EXIT("sleeper.parse_mode");
        return -1;
    }
    if (parsed < minimum) {
        snprintf(err, err_cap, "selftest %s must be >= %llu", label, minimum);
        BT_TRACE_FAIL("sleeper.parse_mode", "selftest %s below %llu", label, minimum);
        BT_TRACE_EXIT("sleeper.parse_mode");
        return -1;
    }
    *mode = kind;
    *value = parsed;
    BT_TRACE_EXIT("sleeper.parse_mode");
    return 0;
}
