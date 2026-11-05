#ifndef BT_CORE_DEATH_TEST_H
#define BT_CORE_DEATH_TEST_H

#include "common/bt_harness.h"
#include "common/bt_trace.h"

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

static void bt_death_scratch_path(const char *case_name, char *out, size_t cap) {
    snprintf(out, cap, "build/test-tmp/death_%s.txt", case_name);
}

static void bt_death_run_child(const char *case_name, void (*fn)(void)) {
    char path[256];
    bt_death_scratch_path(case_name, path, sizeof path);
    _mkdir("build");
    _mkdir("build/test-tmp");
    int fd = _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IWRITE);
    if (fd < 0) {
        BT_ABORT("death case %s cannot open %s", case_name, path);
    }
    fflush(stdout);
    fflush(stderr);
    _dup2(fd, _fileno(stdout));
    _dup2(fd, _fileno(stderr));
    _close(fd);
    bt_trace_init(65536, 8, 256);
    bt_trace_install_crash_handler();
    fn();
    fflush(stdout);
    fflush(stderr);
    fprintf(stderr, "death case %s returned without aborting\n", case_name);
    exit(2);
}

static void bt_death_expect_code(const char *case_name, const char *message, long long want_code,
                                 void (*fn)(void)) {
    const char *child = getenv("BT_DEATH_CHILD");
    const char *target = getenv("BT_DEATH_CASE");
    if (child != NULL) {
        if (target != NULL && strcmp(target, case_name) == 0) {
            bt_death_run_child(case_name, fn);
        }
        return;
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    _putenv("BT_DEATH_CHILD=1");
    char setting[192];
    snprintf(setting, sizeof setting, "BT_DEATH_CASE=%s", case_name);
    _putenv(setting);
    intptr_t code = _wspawnl(_P_WAIT, exe, exe, NULL);
    _putenv("BT_DEATH_CHILD=");
    _putenv("BT_DEATH_CASE=");
    BT_CHECK_EQ((long long)code, want_code);
    char path[256];
    bt_death_scratch_path(case_name, path, sizeof path);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        BT_ABORT("death case %s left no capture file", case_name);
    }
    static char body[262144];
    size_t got = fread(body, 1, sizeof body - 1, file);
    body[got] = '\0';
    fclose(file);
    remove(path);
    BT_CHECK(strstr(body, message) != NULL);
}

static void bt_death_expect_abort(const char *case_name, const char *message, void (*fn)(void)) {
    bt_death_expect_code(case_name, message, -1073740791LL, fn);
}

#endif
