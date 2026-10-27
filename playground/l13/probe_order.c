#include "ga-run/lib.h"
#include "common/bt_harness.h"
#include "common/paths.h"
#include "data/paths.h"
#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *temp = getenv("TEMP");
    char scratch[BT_PATH_MAX];
    snprintf(scratch, sizeof scratch, "%s/bt-gr-orderprobe-%d", temp, _getpid());
    _mkdir(scratch);
    char ga_dir[BT_PATH_MAX];
    char vendor_dir[BT_PATH_MAX];
    snprintf(ga_dir, sizeof ga_dir, "%s/ga", scratch);
    snprintf(vendor_dir, sizeof vendor_dir, "%s/vendor", scratch);
    _mkdir(ga_dir);
    _mkdir(vendor_dir);
    FILE *f = fopen(strcat(ga_dir, "/freq.json"), "wb");
    fwrite("{ not json", 1, 11, f);
    fclose(f);
    f = fopen(strcat(vendor_dir, "/cases.json"), "wb");
    fwrite("{\"cases\": []}", 1, 13, f);
    fclose(f);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config/config.json", root);
    f = fopen(config_path, "rb");
    char config_text[8192];
    size_t config_len = fread(config_text, 1, sizeof config_text - 1, f);
    fclose(f);
    config_text[config_len] = 0;
    char ga_clean[BT_PATH_MAX];
    char vendor_clean[BT_PATH_MAX];
    char ga_dir2[BT_PATH_MAX];
    char vendor_dir2[BT_PATH_MAX];
    snprintf(ga_dir2, sizeof ga_dir2, "%s/ga", scratch);
    snprintf(vendor_dir2, sizeof vendor_dir2, "%s/vendor", scratch);
    snprintf(ga_clean, sizeof ga_clean, "%s", ga_dir2);
    snprintf(vendor_clean, sizeof vendor_clean, "%s", vendor_dir2);
    for (char *cursor = ga_clean; *cursor != 0; cursor++) {
        if (*cursor == 92) *cursor = '/';
    }
    for (char *cursor = vendor_clean; *cursor != 0; cursor++) {
        if (*cursor == 92) *cursor = '/';
    }
    char *at = strstr(config_text, "\"data_ga\": \"data/ga\"");
    fprintf(stderr, "data_ga anchor %s\n", at != NULL ? "found" : "MISSING");
    if (at != NULL) {
        memcpy(at + 12, ga_clean, strlen(ga_clean));
    }
    at = strstr(config_text, "\"data_vendor\": \"data/vendor\"");
    fprintf(stderr, "data_vendor anchor %s\n", at != NULL ? "found" : "MISSING");
    if (at != NULL) {
        memcpy(at + 16, vendor_clean, strlen(vendor_clean));
    }
    char scratch_config[BT_PATH_MAX];
    snprintf(scratch_config, sizeof scratch_config, "%s/config.json", scratch);
    f = fopen(scratch_config, "wb");
    fwrite(config_text, 1, config_len, f);
    fclose(f);
    _putenv_s("BT_CONFIG", scratch_config);
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    int rc = bt_gr_load_run_inputs(&inputs, &offenders);
    fprintf(stderr, "rc=%d count=%zu\n", rc, offenders.count);
    for (size_t i = 0; i < offenders.count; i++) {
        fprintf(stderr, "offender %zu: %s\n", i, offenders.items[i]);
    }
    return 0;
}
