#include <stdio.h>
#include <string.h>

#include "puzzles/nxn.h"
#include "verify/ev_bigcube.h"

void verify_section_init(verify_section_result *section) {
    (void)section;
}
void verify_section_push_row(verify_section_result *section, const verify_row_outcome *row) {
    (void)section;
    (void)row;
}
void verify_section_push_honesty(verify_section_result *section, const char *text) {
    (void)section;
    (void)text;
}
verify_tier2 verify_tier2_skipped(const char *reason) {
    verify_tier2 out = {0};
    out.skip = (char *)reason;
    return out;
}
verify_tier2 verify_tier2_pass(void) {
    verify_tier2 out = {0};
    return out;
}
verify_tier2 verify_tier2_fail(const char *reason) {
    verify_tier2 out = {0};
    snprintf(out.fail, sizeof out.fail, "%s", reason);
    return out;
}

static void probe_alg(const char *alg_text, size_t n) {
    bt_pz_nxn engine;
    if (!bt_pz_nxn_init(&engine, n)) {
        printf("n=%zu init fail\n", n);
        return;
    }
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    if (!eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &err)) {
        printf("n=%zu parse fail %s\n", n, alg_text);
        bt_pz_alg_destroy(alg);
        return;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    bt_pz_nxn_apply_alg(&engine, &state, inverse, &err);
    char reduction_err[512];
    int lines =
        verify_bigcube_reduction_state(&engine, &state, reduction_err, sizeof reduction_err);
    printf("n=%zu lines=%d %s %s\n", n, lines, lines < 0 ? reduction_err : "clean",
           lines < 0 ? "" : alg_text);
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
}

static void probe_audit(const char *alg_text, size_t n) {
    bt_pz_nxn engine;
    if (!bt_pz_nxn_init(&engine, n)) {
        return;
    }
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    bt_pz_engine eng = bt_pz_nxn_engine(&engine);
    if (!eng.parse_alg(&eng, alg_text, strlen(alg_text), alg, &err)) {
        printf("n=%zu parse fail\n", n);
        bt_pz_alg_destroy(alg);
        return;
    }
    void *inverse = bt_pz_alg_create();
    eng.invert_alg(&eng, alg, inverse);
    bt_pz_nxn_state state;
    bt_pz_nxn_solved_state(&engine, &state);
    bt_pz_nxn_apply_alg(&engine, &state, inverse, &err);
    bt_pz_grid grid;
    bt_pz_nxn_facelets(&engine, &state, &grid);
    printf("n=%zu %s\n", n, alg_text);
    for (size_t face = 0; face < 6; face++) {
        verify_bigcube_face_audit audit;
        verify_bigcube_audit_face(&grid, face, n, &audit);
        printf("  face %zu color=%u centers=%d corners=%d midges=%d disturbed=%zu\n", face,
               audit.center_color, audit.centers_uniform, audit.corners_solved, audit.midges_solved,
               audit.disturbed_slots);
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
}

static const char *const ALPHA[] = {
    "Rw", "Rw'", "Rw2", "Uw", "Uw'", "Uw2", "Fw", "Fw'", "Fw2",
    "2R", "2R'", "2R2", "2U", "2U'", "2U2", "2F", "2F'", "2F2",
};
#define ALPHA_COUNT (sizeof ALPHA / sizeof ALPHA[0])
#define SEARCH_MAX 6

static bool audit_clean_lines(const bt_pz_nxn *engine, const bt_pz_nxn_state *state, int *lines) {
    char err[512];
    int got = verify_bigcube_reduction_state(engine, state, err, sizeof err);
    if (got < 0) {
        return false;
    }
    *lines = got;
    return true;
}

static size_t hits = 0;

static void search(size_t n, size_t depth, char *text, size_t used) {
    if (hits >= 24) {
        return;
    }
    if (depth == SEARCH_MAX) {
        return;
    }
    for (size_t at = 0; at < ALPHA_COUNT; at++) {
        size_t len = strlen(ALPHA[at]);
        if (depth > 0) {
            text[used] = ' ';
            memcpy(text + used + 1, ALPHA[at], len + 1);
        } else {
            memcpy(text, ALPHA[at], len + 1);
        }
        size_t now = depth == 0 ? len : used + 1 + len;
        bt_pz_nxn engine;
        bt_pz_nxn_init(&engine, n);
        bt_pz_error err;
        void *alg = bt_pz_alg_create();
        bt_pz_engine eng = bt_pz_nxn_engine(&engine);
        if (eng.parse_alg(&eng, text, strlen(text), alg, &err)) {
            bt_pz_nxn_state state;
            bt_pz_nxn_solved_state(&engine, &state);
            bt_pz_nxn_apply_alg(&engine, &state, alg, &err);
            int lines = 0;
            if (audit_clean_lines(&engine, &state, &lines) && lines >= 1 && lines <= 2) {
                printf("HIT n=%zu lines=%d %s\n", n, lines, text);
                hits += 1;
            }
        }
        bt_pz_alg_destroy(alg);
        search(n, depth + 1, text, now);
        if (hits >= 24) {
            return;
        }
    }
}

int main(void) {
    const char *witness = "Rw2 2U2 2F2 2U2 Rw2";
    probe_alg(witness, 4);
    probe_alg(witness, 5);
    probe_alg(witness, 6);
    probe_alg(witness, 7);
    probe_audit(witness, 6);
    probe_alg("x", 6);
    probe_alg("x", 7);
    return 0;
}
