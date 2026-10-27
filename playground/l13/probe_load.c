#include "ga-run/lib.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    bt_gr_offenders offenders = {0};
    bt_gr_run_inputs inputs;
    fprintf(stderr, "calling load\n");
    int rc = bt_gr_load_run_inputs(&inputs, &offenders);
    fprintf(stderr, "rc=%d offenders=%zu\n", rc, offenders.count);
    for (size_t i = 0; i < offenders.count; i++) {
        fprintf(stderr, "offender: %s\n", offenders.items[i]);
    }
    if (rc == 0) {
        fprintf(stderr, "pool=%zu cooc=%zu adj=%zu\n", inputs.pool_count,
                inputs.freq.cooccurring_count, inputs.freq.adjacent_count);
        bt_gr_run_inputs_free(&inputs);
    }
    return 0;
}
