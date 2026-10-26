#include "common/bt_trace.h"

#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

typedef struct {
    unsigned char *ring;
    size_t cap;
    size_t head;
    size_t used;
} bt_trace_slot;

static bt_trace_slot g_slots[BT_TRACE_MAX_SLOTS];
static _Atomic size_t g_next_slot;
static size_t g_max_threads;
static size_t g_line_max;
static int g_inited;

static thread_local bt_trace_slot *t_slot;

static void write_u32(bt_trace_slot *slot, size_t offset, uint32_t value) {
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        slot->ring[offset] = (unsigned char)((value >> (8 * i)) & 0xFFU);
        offset = (offset + 1) % slot->cap;
    }
}

static uint32_t read_u32(bt_trace_slot *slot, size_t offset) {
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        value |= (uint32_t)slot->ring[offset] << (8 * i);
        offset = (offset + 1) % slot->cap;
    }
    return value;
}

static void ring_pop(bt_trace_slot *slot) {
    uint32_t len = read_u32(slot, slot->head);
    slot->head = (slot->head + sizeof(uint32_t) + len) % slot->cap;
    slot->used -= sizeof(uint32_t) + (size_t)len;
}

static void ring_push(bt_trace_slot *slot, const char *data, size_t len) {
    size_t need = sizeof(uint32_t) + len;
    while (slot->used > 0 && slot->used + need > slot->cap) {
        ring_pop(slot);
    }
    if (need > slot->cap) {
        return;
    }
    size_t tail = (slot->head + slot->used) % slot->cap;
    write_u32(slot, tail, (uint32_t)len);
    tail = (tail + sizeof(uint32_t)) % slot->cap;
    for (size_t i = 0; i < len; i++) {
        slot->ring[tail] = (unsigned char)data[i];
        tail = (tail + 1) % slot->cap;
    }
    slot->used += need;
}

int bt_trace_init(size_t ring_bytes, size_t max_threads, size_t line_max) {
    if (g_inited || ring_bytes < 2 * sizeof(uint32_t) || line_max < 8) {
        return -1;
    }
    if (max_threads == 0 || max_threads > BT_TRACE_MAX_SLOTS) {
        return -1;
    }
    for (size_t i = 0; i < max_threads; i++) {
        g_slots[i].ring = malloc(ring_bytes);
        if (g_slots[i].ring == NULL) {
            for (size_t j = 0; j < i; j++) {
                free(g_slots[j].ring);
                g_slots[j].ring = NULL;
            }
            return -1;
        }
        g_slots[i].cap = ring_bytes;
        g_slots[i].head = 0;
        g_slots[i].used = 0;
    }
    atomic_store(&g_next_slot, 0);
    g_max_threads = max_threads;
    g_line_max = line_max;
    g_inited = 1;
    return 0;
}

static bt_trace_slot *claim_slot(void) {
    if (!g_inited) {
        return NULL;
    }
    if (t_slot == NULL) {
        size_t index = atomic_fetch_add(&g_next_slot, 1);
        if (index >= g_max_threads) {
            return NULL;
        }
        t_slot = &g_slots[index];
    }
    return t_slot;
}

static void emit(const char *text, size_t len) {
    bt_trace_slot *slot = claim_slot();
    if (slot == NULL) {
        return;
    }
    char clamped[512];
    if (len >= sizeof clamped) {
        len = sizeof clamped - 1;
    }
    memcpy(clamped, text, len);
    clamped[len] = '\0';
    ring_push(slot, clamped, len);
}

void bt_trace_enter(const char *unit) {
    char line[512];
    int len = snprintf(line, sizeof line, "enter %s\n", unit);
    if (len > 0) {
        emit(line, (size_t)len >= g_line_max ? g_line_max : (size_t)len);
    }
}

void bt_trace_exit(const char *unit) {
    char line[512];
    int len = snprintf(line, sizeof line, "exit %s\n", unit);
    if (len > 0) {
        emit(line, (size_t)len >= g_line_max ? g_line_max : (size_t)len);
    }
}

void bt_trace_fail(const char *unit, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char message[512];
    int mlen = vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    if (mlen < 0) {
        return;
    }
    char line[512];
    int len = snprintf(line, sizeof line, "fail %s: %s\n", unit, message);
    if (len > 0) {
        emit(line, (size_t)len >= g_line_max ? g_line_max : (size_t)len);
    }
}

void bt_trace_fail_at(const char *unit, const char *file, int line_number, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char message[512];
    int mlen = vsnprintf(message, sizeof message, fmt, ap);
    va_end(ap);
    if (mlen < 0) {
        return;
    }
    char line[512];
    int len =
        snprintf(line, sizeof line, "fail %s at %s:%d: %s\n", unit, file, line_number, message);
    if (len > 0) {
        emit(line, (size_t)len >= g_line_max ? g_line_max : (size_t)len);
    }
}

void bt_trace_dump_to(FILE *stream) {
    if (!g_inited || stream == NULL) {
        return;
    }
    for (size_t i = 0; i < g_max_threads; i++) {
        bt_trace_slot *slot = &g_slots[i];
        if (slot->ring == NULL || slot->used == 0) {
            continue;
        }
        fprintf(stream, "slot %zu\n", i);
        size_t cursor = slot->head;
        size_t remaining = slot->used;
        while (remaining > 0) {
            uint32_t len = read_u32(slot, cursor);
            cursor = (cursor + sizeof(uint32_t)) % slot->cap;
            for (uint32_t j = 0; j < len; j++) {
                fputc(slot->ring[cursor], stream);
                cursor = (cursor + 1) % slot->cap;
            }
            remaining -= sizeof(uint32_t) + (size_t)len;
        }
    }
    fflush(stream);
}

void bt_trace_dump(void) {
    bt_trace_dump_to(stderr);
}

void bt_trace_shutdown(void) {
    if (!g_inited) {
        return;
    }
    for (size_t i = 0; i < g_max_threads; i++) {
        free(g_slots[i].ring);
        g_slots[i].ring = NULL;
        g_slots[i].cap = 0;
        g_slots[i].head = 0;
        g_slots[i].used = 0;
    }
    g_max_threads = 0;
    g_line_max = 0;
    g_inited = 0;
    t_slot = NULL;
}

#ifdef _WIN32

static LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter;

static LONG WINAPI bt_trace_crash(EXCEPTION_POINTERS *info) {
    (void)info;
    bt_trace_dump();
    return EXCEPTION_CONTINUE_SEARCH;
}

int bt_trace_install_crash_handler(void) {
    g_previous_filter = SetUnhandledExceptionFilter(bt_trace_crash);
    return 0;
}

int bt_trace_remove_crash_handler(void) {
    SetUnhandledExceptionFilter(g_previous_filter);
    g_previous_filter = NULL;
    return 0;
}

#else

static void bt_trace_crash(int sig) {
    bt_trace_dump();
    signal(sig, SIG_DFL);
    raise(sig);
}

int bt_trace_install_crash_handler(void) {
    signal(SIGSEGV, bt_trace_crash);
    signal(SIGABRT, bt_trace_crash);
    signal(SIGFPE, bt_trace_crash);
    signal(SIGILL, bt_trace_crash);
    return 0;
}

int bt_trace_remove_crash_handler(void) {
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    return 0;
}

#endif
