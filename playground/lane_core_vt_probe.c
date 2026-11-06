#include <stdio.h>
#include <string.h>

#include "common/strbuf.h"
#include "puzzles/nxn.h"

int main(void) {
    bt_pz_nxn nxn;
    bt_pz_nxn_init(&nxn, 3);
    bt_pz_engine eng = bt_pz_nxn_engine(&nxn);
    bt_pz_error err;
    for (int round = 0; round < 3; round++) {
        void *alg = bt_pz_alg_create();
        bool via_public = bt_pz_nxn_parse_alg(&nxn, "R U R' F2", 10, alg, &err);
        printf("round %d public: %d token=[%s]\n", round, via_public, err.token);
        bt_pz_alg_destroy(alg);
        alg = bt_pz_alg_create();
        bool via_vt = eng.parse_alg(&eng, "R U R' F2", 10, alg, &err);
        printf("round %d vt: %d token=[%s]\n", round, via_vt, err.token);
        bt_pz_alg_destroy(alg);
    }
    return 0;
}
