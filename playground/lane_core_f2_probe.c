#include <stdio.h>

#include "common/strbuf.h"
#include "core/expand.h"

static void show(const char *text) {
    bt_alg alg;
    bt_alg_init(&alg);
    bool ok = bt_expand(text, &alg);
    bt_strbuf out;
    bt_strbuf_init(&out, 0);
    bt_alg_format(&alg, &out);
    printf("input:  [%s]\nresult: ok=%d len=%zu text=[%s]\n\n", text, ok ? 1 : 0, alg.len,
           out.data);
    bt_strbuf_free(&out);
    bt_alg_free(&alg);
}

int main(void) {
    show("R:[R,U] : D");
    show("R:[R,U]:D");
    show("U:U:U:U:U");
    show("R2 U':[S,R2] + U");
    show("R2 U':[S,R2] U");
    return 0;
}
