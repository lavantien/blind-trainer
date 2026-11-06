#include <stdio.h>

#include "common/alloc.h"
#include "core/alg.h"
#include "core/expand.h"

static void probe_text(const char *text) {
    printf("text %s\n", text);
    for (size_t k = 0; k < 24; k++) {
        bt_alg out;
        bt_alg_init(&out);
        bt_alloc_fail_arm(k);
        bool ok = bt_expand(text, &out);
        size_t failures = bt_alloc_fail_count();
        bt_alloc_fail_disarm();
        size_t len = out.items == NULL ? 0 : out.len;
        bt_alg_free(&out);
        printf("  arm %zu ok=%d failures=%zu len=%zu\n", k, ok, failures, len);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    probe_text("U U U U:[[R,U],S]");
    probe_text("U U:R");
    return 0;
}
