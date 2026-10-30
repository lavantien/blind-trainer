#include "e2e/lib.h"

#include "common/bt_trace.h"
#include "e2e/png.h"
#include "e2e/proc.h"

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>

static int value_is_u64(const bt_json_value *value, unsigned long long *out) {
    if (value == NULL || value->type != BT_JSON_NUMBER || !value->is_integer ||
        value->integer < 0) {
        return 0;
    }
    *out = (unsigned long long)value->integer;
    return 1;
}

static int wide_of(const char *path, wchar_t *out, size_t cap) {
    if (bt_e2e_utf8_to_wide(path, out, cap) != 0) {
        return 1;
    }
    return 0;
}

static void mkdir_p_w(const wchar_t *path) {
    wchar_t buf[BT_E2E_PATH_WMAX];
    size_t len = wcslen(path);
    if (len >= BT_E2E_PATH_WMAX) {
        return;
    }
    wcscpy(buf, path);
    for (wchar_t *p = buf + (buf[0] != L'\0' && buf[1] == L':' ? 3 : 1); *p; p++) {
        if (*p == L'/' || *p == L'\\') {
            wchar_t save = *p;
            *p = L'\0';
            _wmkdir(buf);
            *p = save;
        }
    }
    _wmkdir(buf);
}

int bt_e2e_parse_args(size_t argc, const char *const *argv, bt_e2e_args *out) {
    BT_TRACE_ENTER("e2e.parse_args");
    memset(out, 0, sizeof *out);
    if (argc > 2) {
        BT_TRACE_FAIL("e2e.parse_args", "usage: more than two arguments");
        BT_TRACE_EXIT("e2e.parse_args");
        return BT_E2E_PARSE_USAGE;
    }
    if (argc >= 1) {
        snprintf(out->binary, BT_PATH_MAX, "%s", argv[0]);
        out->has_binary = 1;
    }
    if (argc >= 2) {
        snprintf(out->out_dir, BT_PATH_MAX, "%s", argv[1]);
        out->has_out_dir = 1;
    }
    BT_TRACE_EXIT("e2e.parse_args");
    return 0;
}

void bt_e2e_free_settings(bt_e2e_settings *settings) {
    BT_TRACE_ENTER("e2e.free_settings");
    for (size_t i = 0; i < settings->shots_count; i++) {
        free(settings->shots[i]);
    }
    free(settings->shots);
    settings->shots = NULL;
    settings->shots_count = 0;
    BT_TRACE_EXIT("e2e.free_settings");
}

int bt_e2e_read_config(const char *path, bt_json_value **out, char *error, size_t error_cap) {
    BT_TRACE_ENTER("e2e.read_config");
    *out = NULL;
    wchar_t path_w[BT_E2E_PATH_WMAX];
    if (wide_of(path, path_w, BT_E2E_PATH_WMAX) != 0) {
        snprintf(error, error_cap, "cannot read config %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("e2e.read_config", "utf8 conversion failed");
        BT_TRACE_EXIT("e2e.read_config");
        return 1;
    }
    FILE *file = _wfopen(path_w, L"rb");
    if (file == NULL) {
        snprintf(error, error_cap, "cannot read config %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("e2e.read_config", "cannot open %s", path);
        BT_TRACE_EXIT("e2e.read_config");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc(size > 0 ? (size_t)size + 1 : 1);
    if (text == NULL) {
        fclose(file);
        snprintf(error, error_cap, "cannot read config %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("e2e.read_config", "allocation failed");
        BT_TRACE_EXIT("e2e.read_config");
        return 1;
    }
    size_t read_len = fread(text, 1, (size_t)size, file);
    if (read_len != (size_t)size) {
        free(text);
        snprintf(error, error_cap, "cannot read config %s: %s", path, strerror(errno));
        BT_TRACE_FAIL("e2e.read_config", "short read on %s", path);
        BT_TRACE_EXIT("e2e.read_config");
        return 1;
    }
    fclose(file);
    text[read_len] = '\0';
    bt_json_error json_error;
    if (bt_json_parse(text, read_len, out, &json_error) != 0) {
        free(text);
        snprintf(error, error_cap, "cannot parse config %s", path);
        BT_TRACE_FAIL("e2e.read_config", "parse failed at %zu:%zu", json_error.line,
                      json_error.column);
        BT_TRACE_EXIT("e2e.read_config");
        return 1;
    }
    free(text);
    BT_TRACE_EXIT("e2e.read_config");
    return 0;
}

int bt_e2e_neg_config_exit_from(const bt_json_value *value, long *out, char *error,
                                size_t error_cap) {
    BT_TRACE_ENTER("e2e.neg_config_exit_from");
    const bt_json_value *code =
        value != NULL ? bt_json_get(bt_json_get(value, "release"), "neg_config_exit") : NULL;
    if (code == NULL || code->type != BT_JSON_NUMBER || !code->is_integer ||
        code->integer < -2147483648LL || code->integer > 2147483647LL) {
        snprintf(error, error_cap, "config release.neg_config_exit must be an integer");
        BT_TRACE_FAIL("e2e.neg_config_exit_from", "%s", error);
        BT_TRACE_EXIT("e2e.neg_config_exit_from");
        return 1;
    }
    *out = (long)code->integer;
    BT_TRACE_EXIT("e2e.neg_config_exit_from");
    return 0;
}

int bt_e2e_window_dims_from(const bt_json_value *value, bt_e2e_window_dims *out, char *error,
                            size_t error_cap) {
    BT_TRACE_ENTER("e2e.window_dims_from");
    const bt_json_value *section =
        value != NULL ? bt_json_get(bt_json_get(value, "app"), "window") : NULL;
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_cap, "config app.window missing");
        BT_TRACE_FAIL("e2e.window_dims_from", "%s", error);
        BT_TRACE_EXIT("e2e.window_dims_from");
        return 1;
    }
    unsigned long long width = 0;
    unsigned long long height = 0;
    if (!value_is_u64(bt_json_get(section, "width"), &width) || width > 4294967295ULL) {
        snprintf(error, error_cap, "config app.window.width must be an unsigned integer");
        BT_TRACE_FAIL("e2e.window_dims_from", "%s", error);
        BT_TRACE_EXIT("e2e.window_dims_from");
        return 1;
    }
    if (!value_is_u64(bt_json_get(section, "height"), &height) || height > 4294967295ULL) {
        snprintf(error, error_cap, "config app.window.height must be an unsigned integer");
        BT_TRACE_FAIL("e2e.window_dims_from", "%s", error);
        BT_TRACE_EXIT("e2e.window_dims_from");
        return 1;
    }
    out->width = (unsigned int)width;
    out->height = (unsigned int)height;
    BT_TRACE_EXIT("e2e.window_dims_from");
    return 0;
}

int bt_e2e_settings_from(const bt_json_value *value, bt_e2e_settings *out, char *error,
                         size_t error_cap) {
    BT_TRACE_ENTER("e2e.settings_from");
    memset(out, 0, sizeof *out);
    const bt_json_value *section = value != NULL ? bt_json_get(value, "e2e") : NULL;
    if (section == NULL || section->type != BT_JSON_OBJECT) {
        snprintf(error, error_cap, "config e2e missing");
        BT_TRACE_FAIL("e2e.settings_from", "%s", error);
        BT_TRACE_EXIT("e2e.settings_from");
        return 1;
    }
    const bt_json_value *shots = bt_json_get(section, "shots");
    if (shots == NULL || shots->type != BT_JSON_ARRAY) {
        snprintf(error, error_cap, "config e2e.shots must be an array of strings");
        BT_TRACE_FAIL("e2e.settings_from", "%s", error);
        BT_TRACE_EXIT("e2e.settings_from");
        return 1;
    }
    for (size_t i = 0; i < shots->as.array.count; i++) {
        const bt_json_value *item = bt_json_at(shots, i);
        if (item == NULL || item->type != BT_JSON_STRING) {
            snprintf(error, error_cap, "config e2e.shots must be an array of strings");
            BT_TRACE_FAIL("e2e.settings_from", "%s", error);
            BT_TRACE_EXIT("e2e.settings_from");
            return 1;
        }
    }
    out->shots = malloc(shots->as.array.count * sizeof(char *));
    if (out->shots == NULL && shots->as.array.count > 0) {
        snprintf(error, error_cap, "config e2e.shots must be an array of strings");
        BT_TRACE_FAIL("e2e.settings_from", "allocation failed");
        BT_TRACE_EXIT("e2e.settings_from");
        return 1;
    }
    for (size_t i = 0; i < shots->as.array.count; i++) {
        const char *slug = bt_json_at(shots, i)->as.string;
        out->shots[i] = malloc(strlen(slug) + 1);
        if (out->shots[i] == NULL) {
            out->shots_count = i;
            bt_e2e_free_settings(out);
            snprintf(error, error_cap, "config e2e.shots must be an array of strings");
            BT_TRACE_FAIL("e2e.settings_from", "allocation failed");
            BT_TRACE_EXIT("e2e.settings_from");
            return 1;
        }
        strcpy(out->shots[i], slug);
    }
    out->shots_count = shots->as.array.count;
    if (!value_is_u64(bt_json_get(section, "timeout_secs"), &out->timeout_secs)) {
        bt_e2e_free_settings(out);
        snprintf(error, error_cap, "config e2e.timeout_secs must be an unsigned integer");
        BT_TRACE_FAIL("e2e.settings_from", "%s", error);
        BT_TRACE_EXIT("e2e.settings_from");
        return 1;
    }
    BT_TRACE_EXIT("e2e.settings_from");
    return 0;
}

void bt_e2e_config_path_from(const char *env_value, const char *root, char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("e2e.config_path_from");
    bt_paths_config_path_from(env_value != NULL && env_value[0] != '\0' ? env_value : NULL, root,
                              out);
    BT_TRACE_EXIT("e2e.config_path_from");
}

void bt_e2e_out_dir_from(const char *env_value, const bt_json_value *value, const char *root,
                         char out[BT_PATH_MAX]) {
    BT_TRACE_ENTER("e2e.out_dir_from");
    const char *relative = "artifacts";
    char env_copy[BT_PATH_MAX];
    if (env_value != NULL && env_value[0] != '\0') {
        snprintf(env_copy, BT_PATH_MAX, "%s", env_value);
        relative = env_copy;
    } else {
        const bt_json_value *artifacts =
            value != NULL ? bt_json_get(bt_json_get(value, "paths"), "artifacts") : NULL;
        if (artifacts != NULL && artifacts->type == BT_JSON_STRING) {
            relative = artifacts->as.string;
        }
    }
    char base[BT_PATH_MAX];
    if (bt_paths_is_absolute(relative)) {
        snprintf(base, BT_PATH_MAX, "%s", relative);
    } else {
        bt_paths_join(root, relative, base);
    }
    bt_paths_join(base, "e2e", out);
    BT_TRACE_EXIT("e2e.out_dir_from");
}

int bt_e2e_clean_pngs(const char *dir, char *error, size_t error_cap) {
    BT_TRACE_ENTER("e2e.clean_pngs");
    wchar_t dir_w[BT_E2E_PATH_WMAX];
    if (wide_of(dir, dir_w, BT_E2E_PATH_WMAX) != 0) {
        snprintf(error, error_cap, "cannot create %s", dir);
        BT_TRACE_FAIL("e2e.clean_pngs", "utf8 conversion failed");
        BT_TRACE_EXIT("e2e.clean_pngs");
        return 1;
    }
    mkdir_p_w(dir_w);
    struct _stat64 info;
    if (_wstat64(dir_w, &info) != 0 || (info.st_mode & _S_IFDIR) == 0) {
        snprintf(error, error_cap, "not a directory: %s", dir);
        BT_TRACE_FAIL("e2e.clean_pngs", "%s", error);
        BT_TRACE_EXIT("e2e.clean_pngs");
        return 1;
    }
    wchar_t pattern[BT_E2E_PATH_WMAX];
    _snwprintf(pattern, BT_E2E_PATH_WMAX, L"%ls/*", dir_w);
    struct _wfinddata64_t entry;
    intptr_t handle = _wfindfirst64(pattern, &entry);
    if (handle == -1) {
        BT_TRACE_EXIT("e2e.clean_pngs");
        return 0;
    }
    do {
        if ((entry.attrib & _A_SUBDIR) != 0) {
            continue;
        }
        wchar_t *dot = wcsrchr(entry.name, L'.');
        if (dot != NULL && _wcsicmp(dot, L".png") == 0) {
            wchar_t child[BT_E2E_PATH_WMAX];
            _snwprintf(child, BT_E2E_PATH_WMAX, L"%ls/%ls", dir_w, entry.name);
            if (_wremove(child) != 0) {
                char child_utf8[BT_PATH_MAX];
                bt_e2e_wide_to_utf8(child, child_utf8, BT_PATH_MAX);
                snprintf(error, error_cap, "cannot remove %s", child_utf8);
                _findclose(handle);
                BT_TRACE_FAIL("e2e.clean_pngs", "%s", error);
                BT_TRACE_EXIT("e2e.clean_pngs");
                return 1;
            }
        }
    } while (_wfindnext64(handle, &entry) == 0);
    _findclose(handle);
    BT_TRACE_EXIT("e2e.clean_pngs");
    return 0;
}

static int name_cmp(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

int bt_e2e_png_names(const char *dir, char ***out_names, size_t *out_count, char *error,
                     size_t error_cap) {
    BT_TRACE_ENTER("e2e.png_names");
    *out_names = NULL;
    *out_count = 0;
    wchar_t dir_w[BT_E2E_PATH_WMAX];
    if (wide_of(dir, dir_w, BT_E2E_PATH_WMAX) != 0) {
        snprintf(error, error_cap, "cannot list %s", dir);
        BT_TRACE_FAIL("e2e.png_names", "utf8 conversion failed");
        BT_TRACE_EXIT("e2e.png_names");
        return 1;
    }
    wchar_t pattern[BT_E2E_PATH_WMAX];
    _snwprintf(pattern, BT_E2E_PATH_WMAX, L"%ls/*", dir_w);
    struct _wfinddata64_t entry;
    intptr_t handle = _wfindfirst64(pattern, &entry);
    if (handle == -1) {
        snprintf(error, error_cap, "cannot list %s", dir);
        BT_TRACE_FAIL("e2e.png_names", "%s", error);
        BT_TRACE_EXIT("e2e.png_names");
        return 1;
    }
    char **names = NULL;
    size_t count = 0;
    do {
        if ((entry.attrib & _A_SUBDIR) != 0) {
            continue;
        }
        wchar_t *dot = wcsrchr(entry.name, L'.');
        if (dot == NULL || _wcsicmp(dot, L".png") != 0) {
            continue;
        }
        char *name_utf8 = NULL;
        if (bt_e2e_wide_to_utf8_alloc(entry.name, &name_utf8) != 0) {
            continue;
        }
        char **grown = realloc(names, (count + 1) * sizeof(char *));
        if (grown == NULL) {
            free(name_utf8);
            break;
        }
        names = grown;
        names[count] = name_utf8;
        count += 1;
    } while (_wfindnext64(handle, &entry) == 0);
    _findclose(handle);
    qsort(names, count, sizeof(char *), name_cmp);
    *out_names = names;
    *out_count = count;
    BT_TRACE_EXIT("e2e.png_names");
    return 0;
}

void bt_e2e_free_names(char **names, size_t count) {
    BT_TRACE_ENTER("e2e.free_names");
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
    BT_TRACE_EXIT("e2e.free_names");
}

static int push_name_copy(char ***rows, size_t *count, const char *name) {
    char **grown = realloc(*rows, (*count + 1) * sizeof(char *));
    if (grown == NULL) {
        return -1;
    }
    *rows = grown;
    char *copy = malloc(strlen(name) + 1);
    if (copy == NULL) {
        return -1;
    }
    strcpy(copy, name);
    (*rows)[*count] = copy;
    *count += 1;
    return 0;
}

int bt_e2e_expected_names(const char *const *shots, size_t shots_count, char ***out_names,
                          size_t *out_count) {
    BT_TRACE_ENTER("e2e.expected_names");
    *out_names = NULL;
    *out_count = 0;
    char **names = NULL;
    size_t count = 0;
    for (size_t i = 0; i < shots_count; i++) {
        char *name = malloc(strlen(shots[i]) + 5);
        if (name == NULL) {
            bt_e2e_free_names(names, count);
            BT_TRACE_FAIL("e2e.expected_names", "allocation failed");
            BT_TRACE_EXIT("e2e.expected_names");
            return 1;
        }
        strcpy(name, shots[i]);
        strcat(name, ".png");
        char **grown = realloc(names, (count + 1) * sizeof(char *));
        if (grown == NULL) {
            free(name);
            bt_e2e_free_names(names, count);
            BT_TRACE_FAIL("e2e.expected_names", "allocation failed");
            BT_TRACE_EXIT("e2e.expected_names");
            return 1;
        }
        names = grown;
        names[count] = name;
        count += 1;
    }
    qsort(names, count, sizeof(char *), name_cmp);
    size_t unique = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0 && strcmp(names[i], names[i - 1]) == 0) {
            continue;
        }
        names[unique] = names[i];
        unique += 1;
    }
    for (size_t i = unique; i < count; i++) {
        names[i] = NULL;
    }
    *out_names = names;
    *out_count = unique;
    BT_TRACE_EXIT("e2e.expected_names");
    return 0;
}

void bt_e2e_classify(const char *const *found, size_t found_count, const char *const *expected,
                     size_t expected_count, char ***out_missing, size_t *missing_count,
                     char ***out_unexpected, size_t *unexpected_count) {
    BT_TRACE_ENTER("e2e.classify");
    *out_missing = NULL;
    *missing_count = 0;
    *out_unexpected = NULL;
    *unexpected_count = 0;
    for (size_t i = 0; i < expected_count; i++) {
        int duplicate = 0;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(expected[j], expected[i]) == 0) {
                duplicate = 1;
            }
        }
        if (duplicate) {
            continue;
        }
        int present = 0;
        for (size_t j = 0; j < found_count; j++) {
            if (strcmp(found[j], expected[i]) == 0) {
                present = 1;
            }
        }
        if (!present) {
            push_name_copy(out_missing, missing_count, expected[i]);
        }
    }
    for (size_t i = 0; i < found_count; i++) {
        int duplicate = 0;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(found[j], found[i]) == 0) {
                duplicate = 1;
            }
        }
        if (duplicate) {
            continue;
        }
        int known = 0;
        for (size_t j = 0; j < expected_count; j++) {
            if (strcmp(expected[j], found[i]) == 0) {
                known = 1;
            }
        }
        if (!known) {
            push_name_copy(out_unexpected, unexpected_count, found[i]);
        }
    }
    BT_TRACE_EXIT("e2e.classify");
}

bt_e2e_shot_verdict bt_e2e_check_shot(const char *path, bt_e2e_window_dims dims, char *reason,
                                      size_t reason_cap) {
    BT_TRACE_ENTER("e2e.check_shot");
    wchar_t path_w[BT_E2E_PATH_WMAX];
    if (wide_of(path, path_w, BT_E2E_PATH_WMAX) != 0) {
        snprintf(reason, reason_cap, "not a png: %s", path);
        BT_TRACE_FAIL("e2e.check_shot", "%s", reason);
        BT_TRACE_EXIT("e2e.check_shot");
        return BT_E2E_SHOT_FAIL;
    }
    struct _stat64 info;
    if (_wstat64(path_w, &info) != 0) {
        snprintf(reason, reason_cap, "missing");
        BT_TRACE_FAIL("e2e.check_shot", "%s", reason);
        BT_TRACE_EXIT("e2e.check_shot");
        return BT_E2E_SHOT_FAIL;
    }
    if ((info.st_mode & _S_IFREG) == 0 || info.st_size == 0) {
        snprintf(reason, reason_cap, "empty or not a file");
        BT_TRACE_FAIL("e2e.check_shot", "%s", reason);
        BT_TRACE_EXIT("e2e.check_shot");
        return BT_E2E_SHOT_FAIL;
    }
    unsigned int width = 0;
    unsigned int height = 0;
    if (bt_png_dims(path, &width, &height) != 0) {
        snprintf(reason, reason_cap, "not a png");
        BT_TRACE_FAIL("e2e.check_shot", "%s", reason);
        BT_TRACE_EXIT("e2e.check_shot");
        return BT_E2E_SHOT_FAIL;
    }
    if (width != dims.width || height != dims.height) {
        snprintf(reason, reason_cap, "dimensions %ux%u, expected %ux%u", width, height, dims.width,
                 dims.height);
        BT_TRACE_FAIL("e2e.check_shot", "%s", reason);
        BT_TRACE_EXIT("e2e.check_shot");
        return BT_E2E_SHOT_FAIL;
    }
    BT_TRACE_EXIT("e2e.check_shot");
    return BT_E2E_SHOT_OK;
}

int bt_e2e_deadline_from(unsigned long long timeout_secs, unsigned long long now_ms,
                         unsigned long long *out) {
    BT_TRACE_ENTER("e2e.deadline_from");
    if (timeout_secs > (18446744073709551615ULL - now_ms) / 1000ULL) {
        BT_TRACE_FAIL("e2e.deadline_from", "timeout %llu overflows the deadline", timeout_secs);
        BT_TRACE_EXIT("e2e.deadline_from");
        return 1;
    }
    *out = now_ms + timeout_secs * 1000ULL;
    BT_TRACE_EXIT("e2e.deadline_from");
    return 0;
}

char *bt_e2e_tail(const unsigned char *bytes, size_t len) {
    BT_TRACE_ENTER("e2e.tail");
    const char *text = (const char *)bytes;
    size_t *line_starts = NULL;
    size_t *line_lens = NULL;
    size_t line_count = 0;
    size_t line_cap = 0;
    size_t at = 0;
    while (at <= len) {
        size_t start = at;
        while (at < len && text[at] != '\n') {
            at += 1;
        }
        size_t line_len = at - start;
        if (line_len > 0 && text[start + line_len - 1] == '\r') {
            line_len -= 1;
        }
        if (line_len > 0 || at < len) {
            if (line_count == line_cap) {
                size_t grown_cap = line_cap < 64 ? 64 : line_cap * 2;
                size_t *grown_starts = realloc(line_starts, grown_cap * sizeof *grown_starts);
                size_t *grown_lens = realloc(line_lens, grown_cap * sizeof *grown_lens);
                if (grown_starts == NULL || grown_lens == NULL) {
                    free(grown_starts);
                    free(grown_lens);
                    free(line_starts);
                    free(line_lens);
                    BT_TRACE_FAIL("e2e.tail", "allocation failed");
                    BT_TRACE_EXIT("e2e.tail");
                    return NULL;
                }
                line_starts = grown_starts;
                line_lens = grown_lens;
                line_cap = grown_cap;
            }
            line_starts[line_count] = start;
            line_lens[line_count] = line_len;
            line_count += 1;
        }
        if (at >= len) {
            break;
        }
        at += 1;
    }
    size_t keep = line_count < 20 ? line_count : 20;
    size_t first = line_count - keep;
    size_t total = 0;
    for (size_t i = first; i < line_count; i++) {
        total += line_lens[i] + 1;
    }
    char *out = malloc(total > 0 ? total : 1);
    if (out == NULL) {
        free(line_starts);
        free(line_lens);
        BT_TRACE_FAIL("e2e.tail", "allocation failed");
        BT_TRACE_EXIT("e2e.tail");
        return NULL;
    }
    size_t used = 0;
    for (size_t i = first; i < line_count; i++) {
        if (i > first) {
            out[used] = '\n';
            used += 1;
        }
        memcpy(out + used, text + line_starts[i], line_lens[i]);
        used += line_lens[i];
    }
    out[used] = '\0';
    free(line_starts);
    free(line_lens);
    BT_TRACE_EXIT("e2e.tail");
    return out;
}
