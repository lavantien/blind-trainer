#ifndef BT_TRACE_H
#define BT_TRACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define BT_TRACE_MAX_SLOTS 64

int bt_trace_init(size_t ring_bytes, size_t max_threads, size_t line_max);
void bt_trace_shutdown(void);
void bt_trace_enter(const char *unit);
void bt_trace_exit(const char *unit);
void bt_trace_fail(const char *unit, const char *fmt, ...);
void bt_trace_fail_at(const char *unit, const char *file, int line, const char *fmt, ...);
void bt_trace_dump(void);
void bt_trace_dump_to(FILE *stream);
int bt_trace_install_crash_handler(void);
int bt_trace_remove_crash_handler(void);

#define BT_TRACE_ENTER(unit) bt_trace_enter(unit)

#define BT_TRACE_EXIT(unit) bt_trace_exit(unit)

#define BT_TRACE_FAIL(unit, ...) bt_trace_fail_at(unit, __FILE__, __LINE__, __VA_ARGS__)

#endif
