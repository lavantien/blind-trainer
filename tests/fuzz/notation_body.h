#ifndef BT_FUZZ_NOTATION_BODY_H
#define BT_FUZZ_NOTATION_BODY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/bt_harness.h"
#include "common/bt_trace.h"
#include "core/alg.h"
#include "core/cube.h"
#include "fuzz/fuzz_body.h"

int bt_fuzz_notation_once(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return bt_fuzz_notation_once(data, size);
}

int bt_fuzz_notation_once(const uint8_t *data, size_t size) {
    BT_TRACE_ENTER("bt_fuzz_notation_once");
    char text[4096];
    size_t text_len = fuzz_lossy_utf8(data, size, text, sizeof text);
    (void)text_len;
    bool has_replacement = strstr(text, "\xEF\xBF\xBD") != NULL;
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    bool parsed = bt_alg_parse(text, &alg, err, sizeof err);
    if (has_replacement && parsed) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("replacement char must not parse: %s", text);
    }
    if (!parsed) {
        bt_alg_free(&alg);
        BT_TRACE_EXIT("bt_fuzz_notation_once");
        return 0;
    }
    bt_strbuf formatted;
    if (bt_strbuf_init(&formatted, 4096) != 0) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("strbuf init failed in notation body");
    }
    bt_alg_format(&alg, &formatted);
    bt_alg reparsed;
    bt_alg_init(&reparsed);
    char err2[64];
    if (!bt_alg_parse(formatted.data, &reparsed, err2, sizeof err2)) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("formatted alg must reparse: %s", formatted.data);
    }
    if (!bt_alg_eq(&reparsed, &alg)) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("round trip changed alg: %s -> %s", text, formatted.data);
    }
    if (bt_alg_qtm(&reparsed) != bt_alg_qtm(&alg) || bt_alg_htm(&reparsed) != bt_alg_htm(&alg)) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("metric drift for %s", text);
    }
    bt_cube cube = bt_cube_solved();
    bt_cube_apply_alg(&cube, &alg);
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_cube_apply_alg(&cube, &inverse);
    if (!bt_cube_is_solved(&cube)) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("inverse failed: %s", text);
    }
    if (!bt_cube_centers_fixed(&cube)) {
        fuzz_save_crash("notation", data, size);
        BT_ABORT("centers moved: %s", text);
    }
    bt_strbuf_free(&formatted);
    bt_alg_free(&alg);
    bt_alg_free(&reparsed);
    bt_alg_free(&inverse);
    BT_TRACE_EXIT("bt_fuzz_notation_once");
    return 0;
}

#endif
