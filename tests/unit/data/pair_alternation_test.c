#include "common.h"
#include "common/bt_harness.h"
#include "core/cube.h"
#include "data/corpus.h"

#include <stdlib.h>
#include <string.h>

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    td_ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[512];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("corpus load failed: %s", text);
    }
}

static void flip_position(const char *position, char *out, size_t out_size) {
    char parts[8][64];
    size_t count = 0;
    const char *cursor = position;
    while (*cursor != '\0' && count < 8) {
        const char *dash = strchr(cursor, '-');
        size_t len = dash == NULL ? strlen(cursor) : (size_t)(dash - cursor);
        snprintf(parts[count], sizeof parts[count], "%.*s", (int)len, cursor);
        count += 1;
        if (dash == NULL) {
            break;
        }
        cursor = dash + 1;
    }
    char swap[64];
    snprintf(swap, sizeof swap, "%s", parts[count - 1]);
    snprintf(parts[count - 1], sizeof parts[count - 1], "%s", parts[count - 2]);
    snprintf(parts[count - 2], sizeof parts[count - 2], "%s", swap);
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        used += (size_t)snprintf(out + used, out_size - used, "%s%s", i == 0 ? "" : "-", parts[i]);
    }
}

BT_TEST(pair_alternation, first_row_alg_then_flipped_first_row_alg_returns_to_solved) {
    bt_corpus corpus;
    loaded(&corpus);
    size_t covered = 0;
    bt_sheet_class sheets[2] = {BT_SHEET_EDGE, BT_SHEET_CORNER};
    for (int s = 0; s < 2; s++) {
        char positions[1200][64];
        size_t position_count = 0;
        for (size_t i = 0; i < corpus.count; i++) {
            const bt_corpus_row *row = &corpus.rows[i];
            if (row->sheet != sheets[s]) {
                continue;
            }
            bool seen = false;
            for (size_t p = 0; p < position_count; p++) {
                if (strcmp(positions[p], row->position) == 0) {
                    seen = true;
                    break;
                }
            }
            if (!seen && position_count < 1200) {
                snprintf(positions[position_count], sizeof positions[position_count], "%s",
                         row->position);
                position_count += 1;
            }
        }
        for (size_t p = 0; p < position_count; p++) {
            for (size_t q = p + 1; q < position_count; q++) {
                if (strcmp(positions[p], positions[q]) > 0) {
                    char swap[64];
                    snprintf(swap, sizeof swap, "%s", positions[p]);
                    snprintf(positions[p], sizeof positions[p], "%s", positions[q]);
                    snprintf(positions[q], sizeof positions[q], "%s", swap);
                }
            }
        }
        for (size_t p = 0; p < position_count; p++) {
            char flipped[64];
            flip_position(positions[p], flipped, sizeof flipped);
            size_t here[BT_CORPUS_INDEX_MAX];
            size_t there[BT_CORPUS_INDEX_MAX];
            size_t here_len = bt_corpus_rows_for_position(&corpus, sheets[s], positions[p], here,
                                                          BT_CORPUS_INDEX_MAX);
            size_t there_len = bt_corpus_rows_for_position(&corpus, sheets[s], flipped, there,
                                                           BT_CORPUS_INDEX_MAX);
            if (here_len == 0 || there_len == 0) {
                continue;
            }
            covered += 1;
            bt_cube cube = bt_cube_solved();
            bt_cube_apply_alg(&cube, &corpus.rows[here[0]].alg);
            bt_cube_apply_alg(&cube, &corpus.rows[there[0]].alg);
            if (!bt_cube_is_solved(&cube)) {
                BT_ABORT("sheet %d %s then %s not solved", s, positions[p], flipped);
            }
        }
    }
    BT_CHECK(covered == 584);
    bt_corpus_free(&corpus);
}
