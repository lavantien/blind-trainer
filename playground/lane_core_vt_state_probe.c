#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/strbuf.h"
#include "puzzles/fto.h"
#include "puzzles/geometry.h"
#include "puzzles/mega.h"
#include "puzzles/nxn.h"
#include "puzzles/pyra.h"
#include "puzzles/skewb.h"
#include "puzzles/sq1.h"

static void dump_move(const bt_pz_engine *eng, const char *label, const void *alg, size_t at) {
    const unsigned char *p = bt_pz_alg_at(eng, alg, at);
    printf("%s move %zu:", label, at);
    for (size_t b = 0; b < eng->move_size; b++) {
        printf(" %02x", p[b]);
    }
    printf("\n");
}

static void state_diff(const bt_pz_engine *eng, const char *text) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!eng->parse_alg(eng, text, strlen(text), alg, &err)) {
        printf("state_diff [%s] parse failed token=[%s]\n", text, err.token);
        bt_pz_alg_destroy(alg);
        return;
    }
    bt_strbuf sb;
    bt_strbuf_init(&sb, 64);
    eng->format_alg(eng, alg, &sb);
    void *again = bt_pz_alg_create();
    bt_pz_error err2;
    eng->parse_alg(eng, sb.data, sb.len, again, &err2);
    unsigned char *a = malloc(eng->state_size);
    unsigned char *b = malloc(eng->state_size);
    memset(a, 0xAA, eng->state_size);
    memset(b, 0xAA, eng->state_size);
    eng->solved_state(eng, a);
    eng->solved_state(eng, b);
    eng->apply_alg(eng, a, alg, &err);
    eng->apply_alg(eng, b, again, &err2);
    size_t diff = eng->state_size;
    for (size_t i = 0; i < eng->state_size; i++) {
        if (a[i] != b[i]) {
            diff = i;
            break;
        }
    }
    printf("state_diff [%s] -> [%s] %s first diff at %zu of %zu\n", text, sb.data,
           diff == eng->state_size ? "IDENTICAL" : "DIFFERS", diff, eng->state_size);
    if (diff != eng->state_size) {
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            dump_move(eng, "orig", alg, i);
        }
        for (size_t i = 0; i < bt_pz_alg_len(again); i++) {
            dump_move(eng, "refmt", again, i);
        }
    }
    free(a);
    free(b);
    bt_pz_alg_destroy(again);
    bt_strbuf_free(&sb);
    bt_pz_alg_destroy(alg);
}

int main(int argc, char **argv) {
    int skewb_only = argc > 1 && strcmp(argv[1], "skewb") == 0;
    if (skewb_only) {
        bt_skewb skewb = bt_skewb_new();
        bt_pz_engine eng = bt_skewb_engine(&skewb);
        state_diff(&eng, "U L R' B");
        return 0;
    }
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
    state_diff(&eng, "R U R' F2");
    bt_pz_sq1_engine();
    bt_pz_engine mega = bt_mega_engine();
    state_diff(&mega, "R2' BR'");
    bt_pz_engine pyra = bt_pyra_engine();
    state_diff(&pyra, "U L' R B");
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine seng = bt_skewb_engine(&skewb);
    state_diff(&seng, "U L R' B");
    bt_fto fto = bt_fto_new();
    bt_pz_engine feng = bt_fto_engine(&fto);
    state_diff(&feng, "U BR U BR' F U F'");
    return 0;
}
