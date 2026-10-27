#ifndef BT_JOBWRAP_LIB_H
#define BT_JOBWRAP_LIB_H

#include "common/json.h"
#include "common/paths.h"

#include <stddef.h>
#include <wchar.h>

#define BT_JOBWRAP_ERR_MAX 192
#define BT_JOBWRAP_CMDLINE_MAX 32767

extern const char BT_JOBWRAP_USAGE[];
extern const unsigned long long BT_JOBWRAP_DOTNET_TICKS_AT_FILETIME_ZERO;

enum {
    BT_JOBWRAP_INVOCATION_WRAP = 0,
    BT_JOBWRAP_INVOCATION_ALLOC_MB = 1,
    BT_JOBWRAP_INVOCATION_HOLD_SECS = 2,
};

enum {
    BT_JOBWRAP_RUN_OK = 0,
    BT_JOBWRAP_RUN_USAGE = 1,
    BT_JOBWRAP_RUN_FAILURE = 2,
};

typedef struct {
    int kind;
    const wchar_t *anchor;
    const wchar_t *const *payload;
    size_t payload_count;
    unsigned long long value;
} bt_jobwrap_invocation;

unsigned long long bt_jobwrap_dotnet_ticks(unsigned long long filetime);
int bt_jobwrap_is_anchor_name(const wchar_t *raw);
int bt_jobwrap_parse_invocation(int argc, const wchar_t *const argv[], bt_jobwrap_invocation *out,
                                char *err, size_t err_cap);
int bt_jobwrap_quote_argument(const wchar_t *raw, wchar_t *out, size_t cap);
int bt_jobwrap_command_line(const wchar_t *const payload[], size_t count, wchar_t *out, size_t cap);
void bt_jobwrap_workspace_root(char out[BT_PATH_MAX]);
void bt_jobwrap_config_path_from(const char *env_value, char out[BT_PATH_MAX]);
bt_json_value *bt_jobwrap_load_config(char *err, size_t err_cap);
int bt_jobwrap_per_process_bytes(const bt_json_value *config, unsigned long long *out, char *err,
                                 size_t err_cap);
int bt_jobwrap_pid_dir_from(const bt_json_value *config, char out[BT_PATH_MAX], char *err,
                            size_t err_cap);
int bt_jobwrap_run(const bt_jobwrap_invocation *invocation, unsigned int *exit_code, char *err,
                   size_t err_cap);

#endif
