#include "gendata/lib.h"

#include <stdio.h>
#include <string.h>

#include "common/json.h"

static const char *values[] = {
    "1e-7",  "1e-6",  "1e-5",  "1e-4",  "0.5",  "2.0",
    "1e15",  "1e16",  "1e100", "-0.0",  "5e-324",
    "0.30000000000000004",
};

int main(void) {
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        char text[160];
        snprintf(text, sizeof text, "{\"synth\": {\"penalty_other\": %s}}", values[i]);
        bt_json_error jerr;
        bt_json_value *config = NULL;
        if (bt_json_parse(text, strlen(text), &config, &jerr) != 0) {
            printf("%s PARSE-FAIL\n", values[i]);
            continue;
        }
        char hash[65];
        char err[96];
        if (bt_gd_synth_params_sha256(config, BT_GD_SYNTH_ALGORITHM, hash, err,
                                      sizeof err) != 0) {
            printf("%s HASH-FAIL %s\n", values[i], err);
        } else {
            printf("%s %s\n", values[i], hash);
        }
        bt_json_free(config);
    }
    return 0;
}
