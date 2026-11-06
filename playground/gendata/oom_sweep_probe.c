#include <stdio.h>
#include <string.h>

#include "common/alloc.h"
#include "common/json.h"
#include "gendata/lib.h"

int main(void) {
    const char *text = "{\"synth\": {\"deep\": {\"list\": [1, 2, \"x\"], \"name\": \"guarded\"}}}";
    for (size_t armed = 0; armed < 64; armed++) {
        bt_json_error jerr;
        bt_json_value *config = NULL;
        if (bt_json_parse(text, strlen(text), &config, &jerr) != 0) {
            printf("armed %zu parse failed\n", armed);
            return 1;
        }
        char hash[65];
        char err[96];
        err[0] = '\0';
        printf("armed %zu start\n", armed);
        fflush(stdout);
        bt_alloc_fail_arm(armed);
        int rc = bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, hash, err, sizeof err);
        bt_alloc_fail_disarm();
        printf("armed %zu rc %d err %s\n", armed, rc, err);
        fflush(stdout);
        bt_json_free(config);
        if (rc == 0) {
            printf("success at armed %zu\n", armed);
            return 0;
        }
    }
    printf("no success within 64\n");
    return 1;
}
