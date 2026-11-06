#include <stdio.h>
#include <string.h>

#include "puzzles/sq1.h"

typedef struct {
    int top;
    int bottom;
    int slash;
} probe_move;

static void build_text(const probe_move *moves, size_t count, char *out, size_t cap) {
    size_t used = 0;
    for (size_t at = 0; at < count; at++) {
        if (moves[at].slash) {
            used += (size_t)snprintf(out + used, cap - used, "%s/", at > 0 ? " " : "");
        } else {
            used += (size_t)snprintf(out + used, cap - used, "%s(%d,%d)", at > 0 ? " " : "",
                                     moves[at].top, moves[at].bottom);
        }
    }
}

static int run_one(const bt_pz_engine *eng, const char *text, int *inverse_failed,
                   int *replay_failed, int *replay_solved) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (alg == NULL || !eng->parse_alg(eng, text, strlen(text), alg, &err)) {
        bt_pz_alg_destroy(alg);
        return 0;
    }
    void *inverse = bt_pz_alg_create();
    eng->invert_alg(eng, alg, inverse);
    bt_pz_sq1_state state;
    bt_pz_sq1_solved_state(&state);
    *inverse_failed = eng->apply_alg(eng, &state, inverse, &err) ? 0 : 1;
    *replay_failed = 0;
    *replay_solved = 0;
    if (!*inverse_failed) {
        if (!eng->apply_alg(eng, &state, alg, &err)) {
            *replay_failed = 1;
        } else {
            *replay_solved = bt_pz_sq1_is_solved(&state) ? 1 : 0;
        }
    }
    bt_pz_alg_destroy(alg);
    bt_pz_alg_destroy(inverse);
    return 1;
}

int main(void) {
    bt_pz_engine eng = bt_pz_sq1_engine();
    static const int values[] = {-2, -1, 0, 1, 2};
    probe_move moves[4];
    char text[128];
    int inverse_failed = 0;
    int replay_failed = 0;
    int replay_solved = 0;
    size_t inverse_hits = 0;
    size_t replay_hits = 0;
    size_t unsolved_hits = 0;
    for (size_t len = 1; len <= 4; len++) {
        size_t combos = 1;
        for (size_t at = 0; at < len; at++) {
            combos *= 26;
        }
        for (size_t code = 0; code < combos; code++) {
            size_t rest = code;
            for (size_t at = 0; at < len; at++) {
                size_t pick = rest % 26;
                rest /= 26;
                if (pick == 25) {
                    moves[at] = (probe_move){0, 0, 1};
                } else {
                    moves[at] = (probe_move){values[pick / 5], values[pick % 5], 0};
                }
            }
            build_text(moves, len, text, sizeof text);
            if (!run_one(&eng, text, &inverse_failed, &replay_failed, &replay_solved)) {
                continue;
            }
            if (inverse_failed && inverse_hits < 8) {
                printf("inverse-trip [%zu] %s\n", len, text);
                inverse_hits++;
            }
            if (!inverse_failed && replay_failed && replay_hits < 8) {
                printf("replay-trip [%zu] %s\n", len, text);
                replay_hits++;
            }
            if (!inverse_failed && !replay_failed && !replay_solved && unsolved_hits < 8) {
                printf("unsolved    [%zu] %s\n", len, text);
                unsolved_hits++;
            }
        }
        if (inverse_hits > 0 && replay_hits > 0) {
            break;
        }
    }
    printf("totals inverse=%zu shown replay=%zu shown unsolved=%zu shown\n", inverse_hits,
           replay_hits, unsolved_hits);
    return 0;
}
