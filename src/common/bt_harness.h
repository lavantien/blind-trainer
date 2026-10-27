#ifndef BT_HARNESS_H
#define BT_HARNESS_H

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define BT_NORETURN [[noreturn]]
#else
#define BT_NORETURN _Noreturn
#endif

typedef void (*bt_test_fn)(void);

typedef struct {
    const char *name;
    bt_test_fn fn;
} bt_test_entry;

int bt_harness_run(const char *suite, const bt_test_entry *tests, size_t count);
void bt_harness_fail(const char *file, int line, const char *fmt, ...);
BT_NORETURN void bt_harness_exit_failure(void);
FILE *bt_test_open_scratch(const char *name);

void bt_harness_check_str(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, const char *actual, const char *expected);
void bt_harness_check_u64(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, unsigned long long actual,
                          unsigned long long expected);
void bt_harness_check_int(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, long long actual, long long expected);
void bt_harness_check_f64(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, double actual, double expected);

#define BT_TEST(group, name) void bt_test_##group##_##name(void)

#define BT_CHECK(cond)                                                                             \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            bt_harness_fail(__FILE__, __LINE__, "check failed: %s", #cond);                        \
        }                                                                                          \
    } while (0)

#define BT_CHECK_EQ(actual, expected)                                                              \
    do {                                                                                           \
        _Generic((actual),                                                                         \
            char *: bt_harness_check_str,                                                          \
            const char *: bt_harness_check_str,                                                    \
            unsigned long long: bt_harness_check_u64,                                              \
            unsigned long: bt_harness_check_u64,                                                   \
            unsigned int: bt_harness_check_u64,                                                    \
            unsigned short: bt_harness_check_u64,                                                  \
            unsigned char: bt_harness_check_u64,                                                   \
            float: bt_harness_check_f64,                                                           \
            double: bt_harness_check_f64,                                                          \
            default: bt_harness_check_int)(__FILE__, __LINE__, #actual, #expected, (actual),       \
                                           (expected));                                            \
    } while (0)

#define BT_CHECK_F64_BITS(actual, bits)                                                            \
    do {                                                                                           \
        double bt_value = (double)(actual);                                                        \
        uint64_t bt_want = (uint64_t)(bits);                                                       \
        uint64_t bt_got = 0;                                                                       \
        memcpy(&bt_got, &bt_value, sizeof bt_got);                                                 \
        if (bt_got != bt_want) {                                                                   \
            bt_harness_fail(__FILE__, __LINE__, "f64 bits %s: got 0x%016llx want 0x%016llx",       \
                            #actual, (unsigned long long)bt_got, (unsigned long long)bt_want);     \
        }                                                                                          \
    } while (0)

#define BT_CHECK_F64_NEAR(actual, expected, tol)                                                   \
    do {                                                                                           \
        double bt_a = (double)(actual);                                                            \
        double bt_b = (double)(expected);                                                          \
        double bt_t = (double)(tol);                                                               \
        if (!(fabs(bt_a - bt_b) <= bt_t)) {                                                        \
            bt_harness_fail(__FILE__, __LINE__,                                                    \
                            "f64 near %s vs %s: got %.17g want %.17g tol %.17g", #actual,          \
                            #expected, bt_a, bt_b, bt_t);                                          \
        }                                                                                          \
    } while (0)

#define BT_ABORT(...)                                                                              \
    do {                                                                                           \
        bt_harness_fail(__FILE__, __LINE__, "abort: " __VA_ARGS__);                                \
        bt_harness_exit_failure();                                                                 \
    } while (0)

#endif
