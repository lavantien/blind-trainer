#include <stdio.h>
#include <string.h>

#include "common/strbuf.h"
#include "puzzles/fto.h"
#include "puzzles/geometry.h"
#include "puzzles/mega.h"
#include "puzzles/nxn.h"
#include "puzzles/pyra.h"
#include "puzzles/skewb.h"
#include "puzzles/sq1.h"

static void probe(const bt_pz_engine *eng, const char *label, const char *text) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bool parsed = eng->parse_alg(eng, text, strlen(text), alg, &err);
    if (!parsed) {
        printf("%-6s [%s] parse failed: kind=%d token=[%s]\n", label, text, (int)err.kind,
               err.token);
        bt_pz_alg_destroy(alg);
        return;
    }
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    eng->format_alg(eng, alg, &sb);
    void *again = bt_pz_alg_create();
    bt_pz_error err2;
    bool reparsed = eng->parse_alg(eng, sb.data, sb.len, again, &err2);
    printf("%-6s [%s] -> [%s] reparse=%d", label, text, sb.data, reparsed ? 1 : 0);
    if (!reparsed) {
        printf(" err kind=%d token=[%s]", (int)err2.kind, err2.token);
    } else if (bt_pz_alg_len(again) != bt_pz_alg_len(alg)) {
        printf(" len %zu vs %zu", bt_pz_alg_len(again), bt_pz_alg_len(alg));
    } else {
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            if (memcmp(bt_pz_alg_at(eng, alg, i), bt_pz_alg_at(eng, again, i), eng->move_size) !=
                0) {
                printf(" move %zu differs", i);
                break;
            }
        }
    }
    printf("\n");
    bt_pz_alg_destroy(again);
    bt_strbuf_free(&sb);
    bt_pz_alg_destroy(alg);
}

int main(void) {
    for (size_t n = 2; n <= 7; n++) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, n);
        bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
        probe(&eng, "nxn", "R U R' F2");
        if (n >= 4) {
            char wide[32];
            snprintf(wide, sizeof wide, "nxn%zu", n);
            probe(&eng, wide, "2R 3L' 2F2");
        }
    }
    bt_pz_engine sq1 = bt_pz_sq1_engine();
    probe(&sq1, "sq1", "(1,0)");
    probe(&sq1, "sq1", "/");
    probe(&sq1, "sq1", "(5,-3)/(6,6)");
    bt_pz_engine mega = bt_mega_engine();
    probe(&mega, "mega", "R2' BR'");
    probe(&mega, "mega", "U2 L y'");
    bt_pz_engine pyra = bt_pyra_engine();
    probe(&pyra, "pyra", "U L' R B");
    probe(&pyra, "pyra", "u b'");
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine seng = bt_skewb_engine(&skewb);
    probe(&seng, "skewb", "U L R' B");
    bt_fto fto = bt_fto_new();
    bt_pz_engine feng = bt_fto_engine(&fto);
    probe(&feng, "fto", "U BR U BR' F U F'");
    return 0;
}
