#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/json.h"
#include "data/paths.h"
#include "gendata/lib.h"

static void print_hash(const char *label, const char *text) {
    bt_json_error jerr;
    bt_json_value *config = NULL;
    if (bt_json_parse(text, strlen(text), &config, &jerr) != 0) {
        printf("%-12s parse failed\n", label);
        return;
    }
    char hash[65];
    char err[96];
    if (bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, hash, err, sizeof err) != 0) {
        printf("%-12s error %s\n", label, err);
    } else {
        printf("%-12s %s\n", label, hash);
    }
    bt_json_free(config);
}

int main(void) {
    bt_json_value *hub = bt_data_load_config();
    if (hub == NULL) {
        puts("hub config load failed");
        return 1;
    }
    char hash[65];
    char err[96];
    if (bt_gd_synth_params_sha256(hub, BT_GD_SYNTH_ALGORITHM, hash, err, sizeof err) == 0) {
        printf("%-12s %s\n", "golden", hash);
    } else {
        printf("%-12s error %s\n", "golden", err);
    }
    bt_json_free(hub);
    print_hash("1e16", "{\"synth\": {\"penalty_other\": 1e16}}");
    print_hash("0.0000001", "{\"synth\": {\"penalty_other\": 0.0000001}}");
    print_hash("1e15", "{\"synth\": {\"penalty_other\": 1e15}}");
    print_hash("0.00001", "{\"synth\": {\"penalty_other\": 0.00001}}");
    print_hash("2.0", "{\"synth\": {\"penalty_other\": 2.0}}");
    return 0;
}
