#ifndef BT_FUZZ_EXPAND_BODY_H
#define BT_FUZZ_EXPAND_BODY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "core/alg.h"
#include "core/cube.h"
#include "core/expand.h"
#include "fuzz/fuzz_body.h"

int bt_fuzz_expand_once(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return bt_fuzz_expand_once(data, size);
}

int bt_fuzz_expand_once(const uint8_t *data, size_t size) {
    BT_TRACE_ENTER("bt_fuzz_expand_once");
    char text[4096];
    fuzz_lossy_utf8(data, size, text, sizeof text);
    bt_alg first;
    bt_alg_init(&first);
    bool ok = bt_expand(text, &first);
    bt_alg second;
    bt_alg_init(&second);
    bool ok_again = bt_expand(text, &second);
    if (ok != ok_again || (ok && !bt_alg_eq(&first, &second))) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expand must be deterministic: %s", text);
    }
    if (!ok) {
        bt_alg_free(&first);
        bt_alg_free(&second);
        BT_TRACE_EXIT("bt_fuzz_expand_once");
        return 0;
    }
    if (bt_alg_is_empty(&first)) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expansion never collapses to identity: %s", text);
    }
    bt_strbuf formatted;
    bt_strbuf_init(&formatted, 4096);
    bt_alg_format(&first, &formatted);
    bt_alg again;
    bt_alg_init(&again);
    if (!bt_expand(formatted.data, &again)) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expanded form reparses as flat alg: %s", text);
    }
    if (!bt_alg_eq(&again, &first)) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expansion must reach a fixed point: %s", text);
    }
    bt_alg cancelled;
    bt_alg_init(&cancelled);
    bt_expand_cancel(&first, &cancelled);
    bt_strbuf cancelled_text;
    bt_strbuf_init(&cancelled_text, 4096);
    bt_alg_format(&cancelled, &cancelled_text);
    if (strcmp(cancelled_text.data, formatted.data) != 0) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expanded form is already cancelled: %s", text);
    }
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &first);
    bt_alg inverse = bt_alg_inverse(&first);
    bt_cube_apply_alg(&cube, &inverse);
    if (!bt_cube_is_solved(&cube)) {
        fuzz_save_crash("expand", data, size);
        BT_ABORT("expansion not invertible: %s", text);
    }
    bt_strbuf_free(&formatted);
    bt_strbuf_free(&cancelled_text);
    bt_alg_free(&first);
    bt_alg_free(&second);
    bt_alg_free(&again);
    bt_alg_free(&cancelled);
    bt_alg_free(&inverse);
    BT_TRACE_EXIT("bt_fuzz_expand_once");
    return 0;
}

#endif
