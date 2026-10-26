#include "common.h"

BT_TEST(qtm_oracle, qtm_and_htm_agree_with_simcheck) {
    static tc_simcheck_rows rows;
    tc_load_simcheck_rows(&rows);
    BT_CHECK_EQ(rows.count, (size_t)1077);
    for (size_t i = 0; i < rows.count; i++) {
        size_t rowid = (size_t)strtoul(rows.cols[i][2], NULL, 10);
        char alg_text[TC_TEXT_MAX];
        tc_sheet_alg(rows.cols[i][0], rows.cols[i][1], rowid, alg_text, sizeof alg_text);
        bt_alg alg;
        bt_alg_init(&alg);
        char err[64];
        if (!bt_alg_parse(alg_text, &alg, err, sizeof err)) {
            BT_ABORT("%s/%s row %zu: %s", rows.cols[i][0], rows.cols[i][1], rowid, err);
        }
        uint32_t qtm = (uint32_t)strtoul(rows.cols[i][5], NULL, 10);
        uint32_t moves = (uint32_t)strtoul(rows.cols[i][4], NULL, 10);
        uint32_t rotations = 0;
        for (size_t k = 0; k < alg.len; k++) {
            if (alg.items[k].kind == BT_MOVE_ROTATION) {
                rotations += 1;
            }
        }
        if (bt_alg_qtm(&alg) != qtm) {
            BT_ABORT("%s/%s row %zu %s: qtm %u want %u", rows.cols[i][0], rows.cols[i][1], rowid,
                     alg_text, bt_alg_qtm(&alg), qtm);
        }
        if ((uint32_t)alg.len != moves) {
            BT_ABORT("%s/%s row %zu %s: len %zu want %u", rows.cols[i][0], rows.cols[i][1], rowid,
                     alg_text, alg.len, moves);
        }
        if (bt_alg_htm(&alg) + rotations != moves) {
            BT_ABORT("%s/%s row %zu %s: htm+rot %u want %u", rows.cols[i][0], rows.cols[i][1],
                     rowid, alg_text, bt_alg_htm(&alg) + rotations, moves);
        }
        bt_alg_free(&alg);
    }
}
