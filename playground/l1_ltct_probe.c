#include <stdio.h>

#include "analysis_sheets_test.c"

int main(void) {
    const char *note =
        "named e=UF/UR c=UBR/LUB[FUR] | edges: UR->UF UF->UR | corners: UBL->BUR "
        "[UBL>BUR LUB>RUB BUL>UBR] tw=-1 UBR->LUB [UBR>LUB RUB>UBL BUR>BUL] tw=-1 "
        "UFR->RUF [UFR>RUF FUR>UFR RUF>FUR] tw=-1";
    ltct_note parsed;
    parse_ltct_note(note, &parsed);
    printf("edges %zu corners %zu\n", parsed.edges_len, parsed.corners_len);
    for (size_t e = 0; e < parsed.edges_len; e++) {
        printf("edge %zu: %s -> %s\n", e, parsed.edges[e].from, parsed.edges[e].to);
    }
    for (size_t c = 0; c < parsed.corners_len; c++) {
        printf("corner %zu: %s -> %s tw=%d moves=%zu\n", c, parsed.corners[c].piece,
               parsed.corners[c].slot, parsed.corners[c].dir, parsed.corners[c].moves_len);
        for (size_t m = 0; m < parsed.corners[c].moves_len; m++) {
            printf("    %s > %s\n", parsed.corners[c].moves[m].from, parsed.corners[c].moves[m].to);
        }
    }
    return 0;
}
