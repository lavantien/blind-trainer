#include "common/bt_harness.h"
#include "common/alloc.h"
#include "common/bt_trace.h"

#include <signal.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef __LLVM_INSTR_PROFILE_GENERATE
int __llvm_profile_write_file(void);
#endif

static void bt_harness_sigabrt(int sig) {
#ifdef __LLVM_INSTR_PROFILE_GENERATE
    __llvm_profile_write_file();
#endif
    signal(sig, SIG_DFL);
}

static void bt_harness_install_sigabrt_writer(void) {
    signal(SIGABRT, bt_harness_sigabrt);
}

FILE *bt_test_open_scratch(const char *name) {
#ifdef _WIN32
    _mkdir("build");
    _mkdir("build/test-tmp");
#else
    mkdir("build", 0755);
    mkdir("build/test-tmp", 0755);
#endif
    char path[256];
    snprintf(path, sizeof path, "build/test-tmp/%s", name);
    return fopen(path, "w+");
}

static size_t g_passed;
static size_t g_failed;
static int g_current_failed;
static const char *g_current_name;

static void bt_harness_vfail(const char *file, int line, const char *fmt, va_list ap) {
    printf("  check failed %s:%d: ", file, line);
    vprintf(fmt, ap);
    printf("\n");
    g_current_failed = 1;
}

void bt_harness_fail(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    bt_harness_vfail(file, line, fmt, ap);
    va_end(ap);
}

void bt_harness_check(const char *file, int line, const char *expr, int ok) {
    if (!ok) {
        bt_harness_fail(file, line, "check failed: %s", expr);
    }
}

void bt_harness_check_msg(const char *file, int line, int ok, const char *fmt, ...) {
    if (!ok) {
        va_list ap;
        va_start(ap, fmt);
        bt_harness_vfail(file, line, fmt, ap);
        va_end(ap);
    }
}

void bt_harness_check_str(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, const char *actual, const char *expected) {
    const char *a = actual ? actual : "(null)";
    const char *b = expected ? expected : "(null)";
    if (strcmp(a, b) != 0) {
        bt_harness_fail(file, line, "%s vs %s: got \"%s\" want \"%s\"", actual_expr, expected_expr,
                        a, b);
    }
}

void bt_harness_check_u64(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, unsigned long long actual,
                          unsigned long long expected) {
    if (actual != expected) {
        bt_harness_fail(file, line, "%s vs %s: got %llu (0x%llx) want %llu (0x%llx)", actual_expr,
                        expected_expr, actual, actual, expected, expected);
    }
}

void bt_harness_check_int(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, long long actual, long long expected) {
    if (actual != expected) {
        bt_harness_fail(file, line, "%s vs %s: got %lld want %lld", actual_expr, expected_expr,
                        actual, expected);
    }
}

void bt_harness_check_f64(const char *file, int line, const char *actual_expr,
                          const char *expected_expr, double actual, double expected) {
    if (actual != expected) {
        bt_harness_fail(file, line, "%s vs %s: got %.17g want %.17g", actual_expr, expected_expr,
                        actual, expected);
    }
}

void bt_harness_check_near(const char *file, int line, const char *actual_expr,
                           const char *expected_expr, double actual, double expected, double tol) {
    if (!(fabs(actual - expected) <= tol)) {
        bt_harness_fail(file, line, "f64 near %s vs %s: got %.17g want %.17g tol %.17g",
                        actual_expr, expected_expr, actual, expected, tol);
    }
}

BT_NORETURN void bt_harness_exit_failure(void) {
    bt_trace_dump();
    printf("suite abort in %s: %lu passed, %lu failed\n",
           g_current_name ? g_current_name : "(no test)", (unsigned long)g_passed,
           (unsigned long)(g_failed + 1));
    fflush(stdout);
    exit(134);
}

void bt_harness_alloc_arm(size_t ok_count) {
    bt_alloc_fail_arm(ok_count);
}

void bt_harness_alloc_disarm(void) {
    bt_alloc_fail_disarm();
}

size_t bt_harness_alloc_failures(void) {
    return bt_alloc_fail_count();
}

int bt_harness_run(const char *suite, const bt_test_entry *tests, size_t count) {
    bt_harness_install_sigabrt_writer();
    g_passed = 0;
    g_failed = 0;
    for (size_t i = 0; i < count; i++) {
        g_current_failed = 0;
        g_current_name = tests[i].name;
        bt_alloc_fail_disarm();
        tests[i].fn();
        bt_alloc_fail_disarm();
        if (g_current_failed) {
            g_failed++;
            printf("FAIL %s %s\n", suite, tests[i].name);
        } else {
            g_passed++;
            printf("ok %s %s\n", suite, tests[i].name);
        }
        fflush(stdout);
    }
    printf("suite %s: %lu passed, %lu failed\n", suite, (unsigned long)g_passed,
           (unsigned long)g_failed);
    fflush(stdout);
    return g_failed > 0 ? 1 : 0;
}
