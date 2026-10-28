#ifndef BT_E2E_PROC_H
#define BT_E2E_PROC_H

#include <stddef.h>
#include <wchar.h>

#define BT_E2E_PATH_WMAX 2048
#define BT_E2E_ENV_PAIR_MAX 4096

typedef struct {
    int has_code;
    long code;
    unsigned char *stdout_bytes;
    size_t stdout_len;
    unsigned char *stderr_bytes;
    size_t stderr_len;
} bt_e2e_smoke_run;

int bt_e2e_utf8_to_wide(const char *text, wchar_t *out, size_t cap);
int bt_e2e_wide_to_utf8(const wchar_t *text, char *out, size_t cap);
int bt_e2e_file_exists(const char *path);
unsigned long long bt_e2e_now_ms(void);
int bt_e2e_run_smoke(const char *binary, const char *out_dir, const char *const *env_pairs,
                     size_t env_count, unsigned long long deadline_ms, bt_e2e_smoke_run *out,
                     char *error, size_t error_cap);
void bt_e2e_smoke_run_free(bt_e2e_smoke_run *run);

#endif
