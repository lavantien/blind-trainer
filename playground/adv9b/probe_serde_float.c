#include <stdio.h>
#include <string.h>

#include "common/json.h"
#include "gendata/lib.h"

static void probe(const char *text) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &config, &error) != 0) {
        printf("%s => parse failed\n", text);
        return;
    }
    char hash[65];
    char err[128];
    if (bt_gd_synth_params_sha256(config, "first-setup-plateau-1", hash, err, sizeof err) != 0) {
        printf("%s => hash failed: %s\n", text, err);
    } else {
        printf("%s => %s\n", text, hash);
    }
    bt_json_free(config);
}

int main(void) {
    probe("{\"synth\":{\"penalty_other\":1e16}}");
    probe("{\"synth\":{\"penalty_other\":0.0000001}}");
    probe("{\"synth\":{\"penalty_other\":1e15}}");
    probe("{\"synth\":{\"penalty_other\":0.00001}}");
    probe("{\"synth\":{\"penalty_other\":2.0}}");
    return 0;
}
