#include "common.h"

BT_TEST(notation_totality, all_vendor_algs_parse) {
    static tc_vendor_rows rows;
    tc_load_vendor_rows(&rows);
    BT_CHECK_EQ(rows.count, (size_t)1077);
    for (size_t i = 0; i < rows.count; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (!bt_alg_parse(rows.text[i], &alg, err, sizeof err)) {
            BT_ABORT("%s alg %s: %s", rows.rel[i], rows.text[i], err);
        }
        bt_alg_free(&alg);
    }
}

BT_TEST(notation_totality, no_sheet_uses_tokens_outside_the_dialect) {
    static tc_vendor_rows rows;
    tc_load_vendor_rows(&rows);
    for (size_t i = 0; i < rows.count; i++) {
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        BT_CHECK(bt_alg_parse(rows.text[i], &alg, err, sizeof err));
        bt_strbuf canon;
        bt_strbuf_init(&canon, 128);
        bt_alg_format(&alg, &canon);
        if (strstr(canon.data, "2'") != NULL) {
            BT_ABORT("%s canonical form regressed: %s from %s", rows.rel[i], canon.data,
                     rows.text[i]);
        }
        bt_strbuf_free(&canon);
        bt_alg_free(&alg);
    }
}
