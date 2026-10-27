#include "ga-run/lib.h"

#include "common/bt_trace.h"
#include "common/config.h"
#include "data/paths.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    bt_json_value *config = bt_data_load_config();
    long long ring_bytes = 0;
    long long max_threads = 0;
    long long line_max = 0;
    if (config != NULL && bt_config_int(config, "trace.ring_bytes", &ring_bytes) == 0 &&
        bt_config_int(config, "trace.max_threads", &max_threads) == 0 &&
        bt_config_int(config, "trace.line_max", &line_max) == 0 &&
        bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max) == 0) {
        bt_trace_install_crash_handler();
    }
    bt_gr_offenders offenders = {0};
    int failed = config == NULL || bt_gr_run_all(&offenders) != 0;
    if (config == NULL) {
        bt_gr_offenders_push(&offenders, "load config failed");
        failed = 1;
    }
    bt_json_free(config);
    if (failed) {
        for (size_t i = 0; i < offenders.count; i++) {
            fprintf(stderr, "ga: %s\n", offenders.items[i]);
        }
        bt_gr_offenders_free(&offenders);
        bt_trace_shutdown();
        return 1;
    }
    bt_gr_offenders_free(&offenders);
    bt_trace_shutdown();
    return 0;
}
