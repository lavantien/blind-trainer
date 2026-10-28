#include "e2e/lib.h"
#include "e2e/proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void run_leg(const char *label, const char *binary, const char *out_dir,
                    const char *const *pairs, size_t count) {
    bt_e2e_smoke_run run;
    char error[BT_E2E_ERROR_MAX];
    unsigned long long deadline = bt_e2e_now_ms() + 30000ULL;
    int code = bt_e2e_run_smoke(binary, out_dir, pairs, count, deadline, &run, error,
                                sizeof error);
    printf("%s verdict=%d has_code=%d code=%ld\n", label, code, run.has_code, run.code);
    printf("%s stdout:\n%.*s\n", label, (int)run.stdout_len, run.stdout_bytes);
    bt_e2e_smoke_run_free(&run);
}

int main(int argc, char **argv) {
    const char *binary = argv[1];
    char out_dir[BT_E2E_PATH_WMAX * 4];
    GetCurrentDirectoryA(sizeof out_dir, out_dir);
    strcat(out_dir, "\\playground\\adv21a");

    _putenv("APPDATA=C:/adv21a-sandbox-user");
    _putenv("BT_CONFIG=C:/adv21a-original-config");
    const char *neg_pair[] = {"BT_CONFIG=C:/adv21a-neg-config"};
    run_leg("inherit", binary, out_dir, NULL, 0);
    run_leg("override", binary, out_dir, neg_pair, 1);

    char filler[3000];
    for (int i = 0; i < 16; i++) {
        snprintf(filler, sizeof filler,
                 "ADV21A_PAD_%02d=%s", i, "x");
        memset(filler + 14, 'x', 2900);
        filler[14 + 2900] = '\0';
        _putenv(filler);
    }
    run_leg("flooded", binary, out_dir, neg_pair, 1);
    return 0;
}
