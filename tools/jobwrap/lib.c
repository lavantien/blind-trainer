#include "jobwrap/lib.h"

#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "jobwrap/ffi.h"

const char BT_JOBWRAP_USAGE[] = "usage: bt-jobwrap [--anchor NAME] -- <command> <args...>\n"
                                "       bt-jobwrap --selftest-alloc-mb <N>\n"
                                "       bt-jobwrap --selftest-hold-secs <N>";

const unsigned long long BT_JOBWRAP_DOTNET_TICKS_AT_FILETIME_ZERO = 504911232000000000ULL;

unsigned long long bt_jobwrap_dotnet_ticks(unsigned long long filetime) {
    BT_TRACE_ENTER("jobwrap.dotnet_ticks");
    BT_TRACE_EXIT("jobwrap.dotnet_ticks");
    return filetime + BT_JOBWRAP_DOTNET_TICKS_AT_FILETIME_ZERO;
}

int bt_jobwrap_is_anchor_name(const wchar_t *raw) {
    BT_TRACE_ENTER("jobwrap.is_anchor_name");
    int ok = raw[0] != L'\0';
    for (const wchar_t *cursor = raw; ok != 0 && *cursor != L'\0'; cursor++) {
        wchar_t unit = *cursor;
        int good = (unit >= L'0' && unit <= L'9') || (unit >= L'a' && unit <= L'z') ||
                   (unit >= L'A' && unit <= L'Z') || unit == L'.' || unit == L'_' || unit == L'-';
        if (good == 0) {
            ok = 0;
        }
    }
    BT_TRACE_EXIT("jobwrap.is_anchor_name");
    return ok;
}

static int parse_u64_strict(const wchar_t *raw, unsigned long long *out) {
    if (raw[0] == L'+') {
        raw++;
    }
    if (raw[0] == L'\0') {
        return 0;
    }
    unsigned long long value = 0;
    for (const wchar_t *cursor = raw; *cursor != L'\0'; cursor++) {
        if (*cursor < L'0' || *cursor > L'9') {
            return 0;
        }
        unsigned long long digit = (unsigned long long)(*cursor - L'0');
        if (value > (ULLONG_MAX - digit) / 10ULL) {
            return 0;
        }
        value = value * 10ULL + digit;
    }
    *out = value;
    return 1;
}

static int parse_selftest_number(int argc, const wchar_t *const argv[], unsigned long long minimum,
                                 const char *label, int kind, bt_jobwrap_invocation *out, char *err,
                                 size_t err_cap) {
    if (argc != 2) {
        snprintf(err, err_cap, "selftest %s takes exactly one argument", label);
        BT_TRACE_FAIL("jobwrap.parse_invocation", "selftest %s argc %d", label, argc);
        return -1;
    }
    unsigned long long value = 0;
    if (parse_u64_strict(argv[1], &value) == 0) {
        snprintf(err, err_cap, "selftest %s is not a number: %ls", label, argv[1]);
        BT_TRACE_FAIL("jobwrap.parse_invocation", "selftest %s not a number", label);
        return -1;
    }
    if (value < minimum) {
        snprintf(err, err_cap, "selftest %s must be >= %llu", label, minimum);
        BT_TRACE_FAIL("jobwrap.parse_invocation", "selftest %s below %llu", label, minimum);
        return -1;
    }
    out->kind = kind;
    out->value = value;
    return 0;
}

int bt_jobwrap_parse_invocation(int argc, const wchar_t *const argv[], bt_jobwrap_invocation *out,
                                char *err, size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.parse_invocation");
    memset(out, 0, sizeof *out);
    int rc = -1;
    if (argc < 1) {
        snprintf(err, err_cap, "no arguments given");
        BT_TRACE_FAIL("jobwrap.parse_invocation", "no arguments");
        goto done;
    }
    if (wcscmp(argv[0], L"--selftest-alloc-mb") == 0) {
        if (parse_selftest_number(argc, argv, 1, "alloc mb", BT_JOBWRAP_INVOCATION_ALLOC_MB, out,
                                  err, err_cap) != 0) {
            goto done;
        }
        rc = 0;
        goto done;
    }
    if (wcscmp(argv[0], L"--selftest-hold-secs") == 0) {
        if (parse_selftest_number(argc, argv, 0, "hold secs", BT_JOBWRAP_INVOCATION_HOLD_SECS, out,
                                  err, err_cap) != 0) {
            goto done;
        }
        rc = 0;
        goto done;
    }
    int rest_at = 0;
    if (wcscmp(argv[0], L"--anchor") == 0) {
        if (argc < 2) {
            snprintf(err, err_cap, "--anchor needs a name");
            BT_TRACE_FAIL("jobwrap.parse_invocation", "anchor without name");
            goto done;
        }
        if (bt_jobwrap_is_anchor_name(argv[1]) == 0) {
            snprintf(err, err_cap, "--anchor name must be alnum dot underscore dash: %ls", argv[1]);
            BT_TRACE_FAIL("jobwrap.parse_invocation", "anchor name rejected");
            goto done;
        }
        out->anchor = argv[1];
        rest_at = 2;
    }
    if (rest_at >= argc || wcscmp(argv[rest_at], L"--") != 0) {
        snprintf(err, err_cap, "payload must follow --");
        BT_TRACE_FAIL("jobwrap.parse_invocation", "payload marker missing");
        goto done;
    }
    size_t payload_count = (size_t)(argc - rest_at - 1);
    if (payload_count == 0) {
        snprintf(err, err_cap, "payload command is empty");
        BT_TRACE_FAIL("jobwrap.parse_invocation", "payload empty");
        goto done;
    }
    out->payload = argv + rest_at + 1;
    out->payload_count = payload_count;
    out->kind = BT_JOBWRAP_INVOCATION_WRAP;
    rc = 0;
done:
    BT_TRACE_EXIT("jobwrap.parse_invocation");
    return rc;
}

int bt_jobwrap_quote_argument(const wchar_t *raw, wchar_t *out, size_t cap) {
    BT_TRACE_ENTER("jobwrap.quote_argument");
    size_t used = 0;
    int needs = raw[0] == L'\0';
    if (needs == 0) {
        for (const wchar_t *cursor = raw; *cursor != L'\0'; cursor++) {
            if (iswspace(*cursor) != 0 || *cursor == L'"') {
                needs = 1;
                break;
            }
        }
    }
    if (needs == 0) {
        if (wcslen(raw) + 1 > cap) {
            BT_TRACE_FAIL("jobwrap.quote_argument", "cap too small");
            BT_TRACE_EXIT("jobwrap.quote_argument");
            return -1;
        }
        wmemcpy(out, raw, wcslen(raw) + 1);
        BT_TRACE_EXIT("jobwrap.quote_argument");
        return 0;
    }
    if (cap < 3) {
        BT_TRACE_FAIL("jobwrap.quote_argument", "cap too small");
        BT_TRACE_EXIT("jobwrap.quote_argument");
        return -1;
    }
    out[used++] = L'"';
    size_t backslashes = 0;
    for (const wchar_t *cursor = raw; *cursor != L'\0'; cursor++) {
        if (*cursor == L'\\') {
            backslashes++;
            continue;
        }
        size_t emit = *cursor == L'"' ? backslashes * 2 + 1 : backslashes;
        if (used + emit + 2 >= cap) {
            BT_TRACE_FAIL("jobwrap.quote_argument", "cap too small");
            BT_TRACE_EXIT("jobwrap.quote_argument");
            return -1;
        }
        for (size_t at = 0; at < emit; at++) {
            out[used++] = L'\\';
        }
        backslashes = 0;
        out[used++] = *cursor;
    }
    if (used + backslashes * 2 + 2 >= cap) {
        BT_TRACE_FAIL("jobwrap.quote_argument", "cap too small");
        BT_TRACE_EXIT("jobwrap.quote_argument");
        return -1;
    }
    for (size_t at = 0; at < backslashes * 2; at++) {
        out[used++] = L'\\';
    }
    out[used++] = L'"';
    out[used] = L'\0';
    BT_TRACE_EXIT("jobwrap.quote_argument");
    return 0;
}

int bt_jobwrap_command_line(const wchar_t *const payload[], size_t count, wchar_t *out,
                            size_t cap) {
    BT_TRACE_ENTER("jobwrap.command_line");
    size_t used = 0;
    out[0] = L'\0';
    for (size_t at = 0; at < count; at++) {
        wchar_t quoted[BT_JOBWRAP_CMDLINE_MAX];
        if (bt_jobwrap_quote_argument(payload[at], quoted, sizeof quoted / sizeof quoted[0]) != 0) {
            BT_TRACE_FAIL("jobwrap.command_line", "argument %zu too long", at);
            BT_TRACE_EXIT("jobwrap.command_line");
            return -1;
        }
        size_t len = wcslen(quoted);
        size_t space = at > 0 ? 1 : 0;
        if (used + space + len + 1 > cap) {
            BT_TRACE_FAIL("jobwrap.command_line", "line too long at %zu", at);
            BT_TRACE_EXIT("jobwrap.command_line");
            return -1;
        }
        if (at > 0) {
            out[used++] = L' ';
        }
        wmemcpy(out + used, quoted, len);
        used += len;
        out[used] = L'\0';
    }
    BT_TRACE_EXIT("jobwrap.command_line");
    return 0;
}

void bt_jobwrap_workspace_root(char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("jobwrap.workspace_root");
    bt_paths_workspace_root(out);
    BT_TRACE_EXIT("jobwrap.workspace_root");
}

void bt_jobwrap_config_path_from(const char *env_value, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("jobwrap.config_path_from");
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    bt_paths_config_path_from(env_value, root, out);
    BT_TRACE_EXIT("jobwrap.config_path_from");
}

bt_json_value *bt_jobwrap_load_config(char *err, size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.load_config");
    char path[BT_PATH_MAX];
    bt_jobwrap_config_path_from(getenv("BT_CONFIG"), path);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, err_cap, "read %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("jobwrap.load_config", "cannot open %s", path);
        BT_TRACE_EXIT("jobwrap.load_config");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc(size > 0 ? (size_t)size + 1 : 1);
    if (text == NULL) {
        fclose(file);
        snprintf(err, err_cap, "read %s: out of memory", path);
        BT_TRACE_FAIL("jobwrap.load_config", "oom for %s", path);
        BT_TRACE_EXIT("jobwrap.load_config");
        return NULL;
    }
    size_t got = fread(text, 1, size > 0 ? (size_t)size : 0, file);
    int read_error = ferror(file);
    fclose(file);
    if (read_error != 0 || (size > 0 && got != (size_t)size)) {
        free(text);
        snprintf(err, err_cap, "read %s: short read", path);
        BT_TRACE_FAIL("jobwrap.load_config", "short read on %s", path);
        BT_TRACE_EXIT("jobwrap.load_config");
        return NULL;
    }
    text[got] = '\0';
    bt_json_value *config = NULL;
    bt_json_error json_err;
    if (bt_json_parse(text, got, &config, &json_err) != 0) {
        snprintf(err, err_cap, "parse %s: %s", path, json_err.message);
        BT_TRACE_FAIL("jobwrap.load_config", "parse failed on %s", path);
        free(text);
        BT_TRACE_EXIT("jobwrap.load_config");
        return NULL;
    }
    free(text);
    BT_TRACE_EXIT("jobwrap.load_config");
    return config;
}

int bt_jobwrap_per_process_bytes(const bt_json_value *config, unsigned long long *out, char *err,
                                 size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.per_process_bytes");
    const bt_json_value *value = bt_config_value(config, "job.per_process_bytes");
    if (value == NULL) {
        snprintf(err, err_cap, "config section missing: job.per_process_bytes");
        BT_TRACE_FAIL("jobwrap.per_process_bytes", "section missing");
        BT_TRACE_EXIT("jobwrap.per_process_bytes");
        return -1;
    }
    if (value->type != BT_JSON_NUMBER || bt_json_is_integer(value) == 0 ||
        bt_json_integer_or(value, -1) < 0) {
        snprintf(err, err_cap, "config value malformed: job.per_process_bytes");
        BT_TRACE_FAIL("jobwrap.per_process_bytes", "value malformed");
        BT_TRACE_EXIT("jobwrap.per_process_bytes");
        return -1;
    }
    long long exact = bt_json_integer_or(value, 0);
    if (exact == 0) {
        snprintf(err, err_cap, "config value invalid: job.per_process_bytes must be > 0");
        BT_TRACE_FAIL("jobwrap.per_process_bytes", "value zero");
        BT_TRACE_EXIT("jobwrap.per_process_bytes");
        return -1;
    }
    *out = (unsigned long long)exact;
    BT_TRACE_EXIT("jobwrap.per_process_bytes");
    return 0;
}

static int is_ws_codepoint(unsigned int codepoint) {
    return (codepoint >= 0x09 && codepoint <= 0x0D) || codepoint == 0x20 || codepoint == 0x85 ||
           codepoint == 0xA0 || codepoint == 0x1680 ||
           (codepoint >= 0x2000 && codepoint <= 0x200A) || codepoint == 0x2028 ||
           codepoint == 0x2029 || codepoint == 0x202F || codepoint == 0x205F || codepoint == 0x3000;
}

static unsigned int utf8_codepoint_at(const char *raw, size_t *at) {
    unsigned char lead = (unsigned char)raw[*at];
    if (lead < 0x80) {
        (*at)++;
        return lead;
    }
    size_t extra = 0;
    unsigned int codepoint = 0;
    if ((lead & 0xE0U) == 0xC0U) {
        extra = 1;
        codepoint = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
        extra = 2;
        codepoint = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
        extra = 3;
        codepoint = lead & 0x07U;
    } else {
        (*at)++;
        return 0xFFFD;
    }
    for (size_t step = 1; step <= extra; step++) {
        unsigned int cont = (unsigned char)raw[*at + step];
        if ((cont & 0xC0U) != 0x80U) {
            (*at)++;
            return 0xFFFD;
        }
        codepoint = (codepoint << 6U) | (cont & 0x3FU);
    }
    *at += extra + 1;
    return codepoint;
}

static int dir_text_is_blank(const char *raw) {
    if (raw[0] == '\0') {
        return 1;
    }
    size_t at = 0;
    while (raw[at] != '\0') {
        if (is_ws_codepoint(utf8_codepoint_at(raw, &at)) == 0) {
            return 0;
        }
    }
    return 1;
}

int bt_jobwrap_pid_dir_from(const bt_json_value *config, char out[BT_PATH_MAX], char *err,
                            size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.pid_dir_from");
    const bt_json_value *value = bt_config_value(config, "memguard.pid_dir");
    if (value == NULL || value->type != BT_JSON_STRING) {
        snprintf(err, err_cap, "config section missing: memguard.pid_dir");
        BT_TRACE_FAIL("jobwrap.pid_dir_from", "section missing");
        BT_TRACE_EXIT("jobwrap.pid_dir_from");
        return -1;
    }
    const char *raw = value->as.string;
    if (dir_text_is_blank(raw) != 0) {
        snprintf(err, err_cap, "config value invalid: memguard.pid_dir");
        BT_TRACE_FAIL("jobwrap.pid_dir_from", "value blank");
        BT_TRACE_EXIT("jobwrap.pid_dir_from");
        return -1;
    }
    int has_prefix = (raw[0] != '\0' && raw[1] == ':') || (raw[0] == '\\' && raw[1] == '\\');
    int has_root = raw[0] == '/' || raw[0] == '\\';
    if (has_prefix != 0) {
        snprintf(out, BT_PATH_MAX, "%s", raw);
    } else if (has_root != 0) {
        char root[BT_PATH_MAX];
        bt_paths_workspace_root(root);
        if (root[1] == ':') {
            snprintf(out, BT_PATH_MAX, "%2.2s%s", root, raw);
        } else {
            snprintf(out, BT_PATH_MAX, "%s", raw);
        }
    } else {
        char root[BT_PATH_MAX];
        bt_paths_workspace_root(root);
        snprintf(out, BT_PATH_MAX, "%s/%s", root, raw);
    }
    BT_TRACE_EXIT("jobwrap.pid_dir_from");
    return 0;
}

static int dir_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int make_one_dir(const char *path, char *err, size_t err_cap) {
    if (dir_exists(path) || (_mkdir(path) == 0) || dir_exists(path)) {
        return 0;
    }
    snprintf(err, err_cap, "create %s: %s", path, strerror(errno));
    return -1;
}

static int make_dirs(const char *path, char *err, size_t err_cap) {
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            *cursor = '\0';
            if (make_one_dir(copy, err, err_cap) != 0) {
                return -1;
            }
            *cursor = '/';
        }
    }
    return make_one_dir(copy, err, err_cap);
}

static int write_pid_file(const bt_json_value *config, const wchar_t *anchor,
                          char path_out[BT_PATH_MAX], char *err, size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.write_pid_file");
    char dir[BT_PATH_MAX];
    if (bt_jobwrap_pid_dir_from(config, dir, err, err_cap) != 0) {
        BT_TRACE_FAIL("jobwrap.write_pid_file", "pid dir rejected");
        BT_TRACE_EXIT("jobwrap.write_pid_file");
        return -1;
    }
    if (make_dirs(dir, err, err_cap) != 0) {
        BT_TRACE_FAIL("jobwrap.write_pid_file", "mkdir failed");
        BT_TRACE_EXIT("jobwrap.write_pid_file");
        return -1;
    }
    unsigned long long ticks = 0;
    unsigned long code = bt_jobwrap_dotnet_start_ticks(&ticks);
    if (code != 0) {
        snprintf(err, err_cap, "read own start time: win32 error %lu", code);
        BT_TRACE_FAIL("jobwrap.write_pid_file", "start ticks error %lu", code);
        BT_TRACE_EXIT("jobwrap.write_pid_file");
        return -1;
    }
    unsigned long pid = GetCurrentProcessId();
    snprintf(path_out, BT_PATH_MAX, "%s/%ls-%lu.pid", dir, anchor, pid);
    FILE *file = fopen(path_out, "wb");
    if (file == NULL) {
        snprintf(err, err_cap, "write %s: %s", path_out, strerror(errno));
        BT_TRACE_FAIL("jobwrap.write_pid_file", "cannot open %s", path_out);
        BT_TRACE_EXIT("jobwrap.write_pid_file");
        return -1;
    }
    fprintf(file, "%lu %llu", pid, ticks);
    int flush_error = ferror(file);
    int close_rc = fclose(file);
    if (flush_error != 0 || close_rc != 0) {
        snprintf(err, err_cap, "write %s: short write", path_out);
        BT_TRACE_FAIL("jobwrap.write_pid_file", "short write on %s", path_out);
        BT_TRACE_EXIT("jobwrap.write_pid_file");
        return -1;
    }
    fprintf(stderr, "bt-jobwrap: anchor pidfile %s holds pid %lu\n", path_out, pid);
    fflush(stderr);
    BT_TRACE_EXIT("jobwrap.write_pid_file");
    return 0;
}

static int execute(const wchar_t *line, unsigned long long ceiling, unsigned int *exit_code,
                   char *err, size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.execute");
    HANDLE job = NULL;
    unsigned long code = bt_jobwrap_create_ceiling_job(ceiling, &job);
    if (code != 0) {
        snprintf(err, err_cap, "job create failed: win32 error %lu", code);
        BT_TRACE_FAIL("jobwrap.execute", "job create error %lu", code);
        BT_TRACE_EXIT("jobwrap.execute");
        return -1;
    }
    bt_jobwrap_spawned spawned;
    code = bt_jobwrap_spawn_suspended(line, &spawned);
    if (code != 0) {
        snprintf(err, err_cap, "spawn failed: win32 error %lu", code);
        BT_TRACE_FAIL("jobwrap.execute", "spawn error %lu", code);
        bt_jobwrap_close_handle(job);
        BT_TRACE_EXIT("jobwrap.execute");
        return -1;
    }
    unsigned long assign_code = bt_jobwrap_assign_and_resume(job, &spawned);
    if (assign_code != 0) {
        snprintf(err, err_cap, "assign or resume failed for pid %lu: win32 error %lu",
                 spawned.process_id, assign_code);
        BT_TRACE_FAIL("jobwrap.execute", "assign or resume error %lu", assign_code);
        bt_jobwrap_close_handle(spawned.process);
        bt_jobwrap_close_handle(spawned.thread);
        bt_jobwrap_close_handle(job);
        BT_TRACE_EXIT("jobwrap.execute");
        return -1;
    }
    bt_jobwrap_close_handle(spawned.thread);
    fprintf(stderr, "bt-jobwrap: spawned pid %lu resumed under the job ceiling\n",
            spawned.process_id);
    fflush(stderr);
    unsigned long wait_code = bt_jobwrap_wait_exit_code(spawned.process, exit_code);
    if (wait_code != 0) {
        snprintf(err, err_cap, "wait failed for pid %lu: win32 error %lu", spawned.process_id,
                 wait_code);
        BT_TRACE_FAIL("jobwrap.execute", "wait error %lu", wait_code);
    }
    bt_jobwrap_close_handle(spawned.process);
    bt_jobwrap_close_handle(job);
    BT_TRACE_EXIT("jobwrap.execute");
    return wait_code != 0 ? -1 : 0;
}

int bt_jobwrap_run(const bt_jobwrap_invocation *invocation, unsigned int *exit_code, char *err,
                   size_t err_cap) {
    BT_TRACE_ENTER("jobwrap.run");
    if (invocation->kind != BT_JOBWRAP_INVOCATION_WRAP) {
        snprintf(err, err_cap, "run needs a wrap invocation");
        BT_TRACE_FAIL("jobwrap.run", "kind %d", invocation->kind);
        BT_TRACE_EXIT("jobwrap.run");
        return BT_JOBWRAP_RUN_USAGE;
    }
    bt_json_value *config = bt_jobwrap_load_config(err, err_cap);
    if (config == NULL) {
        BT_TRACE_FAIL("jobwrap.run", "config load failed");
        BT_TRACE_EXIT("jobwrap.run");
        return BT_JOBWRAP_RUN_FAILURE;
    }
    unsigned long long ceiling = 0;
    if (bt_jobwrap_per_process_bytes(config, &ceiling, err, err_cap) != 0) {
        bt_json_free(config);
        BT_TRACE_FAIL("jobwrap.run", "ceiling rejected");
        BT_TRACE_EXIT("jobwrap.run");
        return BT_JOBWRAP_RUN_FAILURE;
    }
    char pidfile[BT_PATH_MAX];
    pidfile[0] = '\0';
    if (invocation->anchor != NULL) {
        char inner[BT_JOBWRAP_ERR_MAX];
        if (write_pid_file(config, invocation->anchor, pidfile, inner, sizeof inner) != 0) {
            snprintf(err, err_cap, "anchor %ls: %s", invocation->anchor, inner);
            bt_json_free(config);
            BT_TRACE_FAIL("jobwrap.run", "anchor write failed");
            BT_TRACE_EXIT("jobwrap.run");
            return BT_JOBWRAP_RUN_FAILURE;
        }
    }
    bt_json_free(config);
    wchar_t line[BT_JOBWRAP_CMDLINE_MAX];
    if (bt_jobwrap_command_line(invocation->payload, invocation->payload_count, line,
                                BT_JOBWRAP_CMDLINE_MAX) != 0) {
        if (pidfile[0] != '\0') {
            remove(pidfile);
        }
        snprintf(err, err_cap, "payload command line too long");
        BT_TRACE_FAIL("jobwrap.run", "command line too long");
        BT_TRACE_EXIT("jobwrap.run");
        return BT_JOBWRAP_RUN_FAILURE;
    }
    const wchar_t *program = invocation->payload_count > 0 ? invocation->payload[0] : L"";
    fprintf(stderr, "bt-jobwrap: anchor=%ls ceiling=%llu payload=%ls\n",
            invocation->anchor != NULL ? invocation->anchor : L"none", ceiling, program);
    fflush(stderr);
    ULONGLONG started = GetTickCount64();
    char exec_err[BT_JOBWRAP_ERR_MAX];
    int exec_rc = execute(line, ceiling, exit_code, exec_err, sizeof exec_err);
    if (pidfile[0] != '\0') {
        remove(pidfile);
    }
    if (exec_rc == 0) {
        double elapsed = (double)(GetTickCount64() - started) / 1000.0;
        fprintf(stderr, "bt-jobwrap: payload %ls exited %u after %.2fs\n", program, *exit_code,
                elapsed);
        fflush(stderr);
        BT_TRACE_EXIT("jobwrap.run");
        return BT_JOBWRAP_RUN_OK;
    }
    snprintf(err, err_cap, "%s", exec_err);
    fprintf(stderr, "bt-jobwrap: %s\n", exec_err);
    fflush(stderr);
    BT_TRACE_FAIL("jobwrap.run", "execute failed");
    BT_TRACE_EXIT("jobwrap.run");
    return BT_JOBWRAP_RUN_FAILURE;
}
