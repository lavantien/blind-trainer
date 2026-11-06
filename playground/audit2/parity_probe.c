#include <stdio.h>
#include <string.h>

#include "data/verify.h"

static void run(const char *alg_text, const char *position, bt_sheet_class sheet,
                const char *label) {
    bt_alg alg;
    bt_alg_init(&alg);
    char err[64];
    if (!bt_alg_parse(alg_text, &alg, err, sizeof err)) {
        printf("%s: parse failed: %s\n", label, err);
        return;
    }
    bt_verdict verdict;
    bt_verdict_init(&verdict);
    bt_verify(&alg, position, sheet, &verdict);
    char text[1024];
    bt_verdict_string(&verdict, text, sizeof text);
    printf("%s\nposition: %s\nok=%d problems=%zu notes=%zu\n%s\n\n", label, position,
           bt_verdict_ok(&verdict) ? 1 : 0, verdict.problem_count, verdict.note_count, text);
    bt_verdict_free(&verdict);
    bt_alg_free(&alg);
}

int main(void) {
    static const char PARITY[] = "U' R U R' F' R U R' U' R' F R2 U' R'";
    static const char FLIP[] = "L E2 L2 E L U L' E' L2 E2 L' U'";
    static const char TWIST[] = "R' D' R D R' D' R U' R' D R D' R' D R U";
    char combined[512];
    run(PARITY, "UF-UL, UFR-UFL", BT_SHEET_PARITY, "pure parity row");
    run(FLIP, "UF & UL", BT_SHEET_FLIPS, "pure flip row");
    run(TWIST, "UFR & UFL", BT_SHEET_TWISTS, "pure twist row");
    snprintf(combined, sizeof combined, "%s %s", PARITY, FLIP);
    run(combined, "UF-UL, UFR-UFL", BT_SHEET_PARITY, "parity alg plus flip pair on named edges");
    snprintf(combined, sizeof combined, "%s %s", PARITY, TWIST);
    run(combined, "UF-UL, UFR-UFL", BT_SHEET_PARITY, "parity alg plus twist pair on named corners");
    return 0;
}
