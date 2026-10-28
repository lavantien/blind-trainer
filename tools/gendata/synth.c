#include "gendata/synth.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/strbuf.h"
#include "core/cube.h"
#include "core/moves.h"
#include "data/corpus.h"
#include "data/verify.h"
#include "scheme/memo.h"
#include "scheme/pairs.h"
#include "scheme/scheme.h"

enum { GD_SYNTH_ALPHABET_MAX = 27 };

typedef struct {
    bt_move *items;
    size_t len;
    size_t cap;
} bt_gd_move_vec;

static int vec_push(bt_gd_move_vec *vec, bt_move mv) {
    if (vec->len == vec->cap) {
        size_t cap = vec->cap == 0 ? 8 : vec->cap * 2;
        bt_move *items = realloc(vec->items, cap * sizeof(bt_move));
        if (items == NULL) {
            return -1;
        }
        vec->items = items;
        vec->cap = cap;
    }
    vec->items[vec->len++] = mv;
    return 0;
}

static void vec_free(bt_gd_move_vec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static void vec_clone(const bt_gd_move_vec *src, bt_gd_move_vec *dst) {
    dst->cap = dst->len = src->len;
    dst->items = src->len > 0 ? malloc(src->len * sizeof(bt_move)) : NULL;
    if (dst->items != NULL && src->len > 0) {
        memcpy(dst->items, src->items, src->len * sizeof(bt_move));
    }
}

static void solved_occ(uint8_t occ[54]) {
    for (size_t i = 0; i < 54; i++) {
        occ[i] = (uint8_t)i;
    }
}

static void apply_move_occ(uint8_t occ[54], bt_move mv) {
    uint8_t table[54];
    bt_move_table(mv, table);
    uint8_t next[54];
    for (size_t i = 0; i < 54; i++) {
        next[table[i]] = occ[i];
    }
    memcpy(occ, next, sizeof next);
}

static void apply_seq_occ(uint8_t occ[54], const bt_move *moves, size_t count) {
    for (size_t i = 0; i < count; i++) {
        apply_move_occ(occ, moves[i]);
    }
}

static bool occ_equals(const uint8_t left[54], const uint8_t right[54]) {
    return memcmp(left, right, 54) == 0;
}

static size_t face_turns(bt_move out[18]) {
    static const uint8_t faces[6] = {
        BT_FACE_U, BT_FACE_L, BT_FACE_F, BT_FACE_R, BT_FACE_B, BT_FACE_D,
    };
    static const uint8_t quarters[3] = {1, 2, 3};
    size_t at = 0;
    for (size_t f = 0; f < 6; f++) {
        for (size_t q = 0; q < 3; q++) {
            out[at++] = bt_move_of_face(faces[f], quarters[q]);
        }
    }
    return at;
}

static size_t single_turns(bt_move out[GD_SYNTH_ALPHABET_MAX]) {
    size_t count = face_turns(out);
    static const uint8_t slices[3] = {BT_SLICE_M, BT_SLICE_E, BT_SLICE_S};
    static const uint8_t quarters[3] = {1, 2, 3};
    for (size_t s = 0; s < 3; s++) {
        for (size_t q = 0; q < 3; q++) {
            out[count++] = bt_move_of_slice(slices[s], quarters[q]);
        }
    }
    return count;
}

static uint8_t layer_of(bt_move mv) {
    switch ((bt_move_kind)mv.kind) {
    case BT_MOVE_FACE:
    case BT_MOVE_WIDE:
    case BT_MOVE_INNER:
        return mv.face;
    case BT_MOVE_SLICE:
        return 6 + mv.slice;
    default:
        return 9;
    }
}

static bool canonical_extension(const bt_gd_move_vec *prefix, bt_move mv) {
    if (prefix->len == 0) {
        return true;
    }
    return layer_of(prefix->items[prefix->len - 1]) != layer_of(mv);
}

static int setups_by_depth(const bt_move *alphabet, size_t alphabet_len, size_t max_setup,
                           bt_gd_move_vec **out, size_t *out_count) {
    bt_gd_move_vec *all = NULL;
    size_t all_count = 0;
    bt_gd_move_vec *frontier = malloc(sizeof(bt_gd_move_vec));
    size_t frontier_count = 1;
    if (frontier == NULL) {
        return -1;
    }
    frontier[0].items = NULL;
    frontier[0].len = 0;
    frontier[0].cap = 0;
    all = malloc(sizeof(bt_gd_move_vec));
    if (all == NULL) {
        free(frontier);
        return -1;
    }
    vec_clone(&frontier[0], &all[0]);
    all_count = 1;
    for (size_t depth = 0; depth < max_setup; depth++) {
        bt_gd_move_vec *next = NULL;
        size_t next_count = 0;
        for (size_t p = 0; p < frontier_count; p++) {
            for (size_t a = 0; a < alphabet_len; a++) {
                if (!canonical_extension(&frontier[p], alphabet[a])) {
                    continue;
                }
                bt_gd_move_vec *grown = realloc(next, (next_count + 1) * sizeof(bt_gd_move_vec));
                if (grown == NULL) {
                    goto fail;
                }
                next = grown;
                vec_clone(&frontier[p], &next[next_count]);
                if (vec_push(&next[next_count], alphabet[a]) != 0) {
                    vec_free(&next[next_count]);
                    goto fail;
                }
                next_count++;
            }
        }
        bt_gd_move_vec *grown_all = realloc(all, (all_count + next_count) * sizeof(bt_gd_move_vec));
        if (grown_all == NULL) {
            goto fail;
        }
        all = grown_all;
        for (size_t n = 0; n < next_count; n++) {
            vec_clone(&next[n], &all[all_count + n]);
        }
        all_count += next_count;
        for (size_t p = 0; p < frontier_count; p++) {
            vec_free(&frontier[p]);
        }
        free(frontier);
        frontier = next;
        frontier_count = next_count;
        continue;
    fail:
        for (size_t n = 0; n < next_count; n++) {
            vec_free(&next[n]);
        }
        free(next);
        for (size_t p = 0; p < frontier_count; p++) {
            vec_free(&frontier[p]);
        }
        free(frontier);
        for (size_t a = 0; a < all_count; a++) {
            vec_free(&all[a]);
        }
        free(all);
        return -1;
    }
    for (size_t p = 0; p < frontier_count; p++) {
        vec_free(&frontier[p]);
    }
    free(frontier);
    *out = all;
    *out_count = all_count;
    return 0;
}

static bool cycle_slots(const uint8_t cube_s[54], const uint8_t t_s[54], size_t buffer_slot,
                        size_t slots[3]) {
    slots[0] = buffer_slot;
    slots[1] = 0;
    slots[2] = 0;
    size_t current = buffer_slot;
    for (size_t index = 1; index < 3; index++) {
        uint8_t sticker = t_s[current];
        size_t from = 54;
        for (size_t i = 0; i < 54; i++) {
            if (cube_s[i] == sticker) {
                from = i;
                break;
            }
        }
        if (from == 54 || from == buffer_slot) {
            return false;
        }
        for (size_t i = 0; i < index; i++) {
            if (slots[i] == from) {
                return false;
            }
        }
        slots[index] = from;
        current = from;
    }
    uint8_t sticker = t_s[current];
    size_t back = 54;
    for (size_t i = 0; i < 54; i++) {
        if (cube_s[i] == sticker) {
            back = i;
            break;
        }
    }
    return back == buffer_slot;
}

typedef struct {
    bt_gd_move_vec *items;
    size_t count;
    size_t cap;
} bt_gd_insertions;

static int insertions_push(bt_gd_insertions *sink, const bt_gd_move_vec *stack) {
    if (sink->count == sink->cap) {
        size_t cap = sink->cap == 0 ? 8 : sink->cap * 2;
        bt_gd_move_vec *items = realloc(sink->items, cap * sizeof(bt_gd_move_vec));
        if (items == NULL) {
            return -1;
        }
        sink->items = items;
        sink->cap = cap;
    }
    vec_clone(stack, &sink->items[sink->count]);
    sink->count++;
    return 0;
}

typedef struct {
    size_t left;
} bt_gd_budget;

static bool reaches_target(const uint8_t state[54], const size_t targets[2], uint8_t buffer) {
    for (size_t t = 0; t < 2; t++) {
        if (bt_scheme_same_piece(state[targets[t]], buffer)) {
            return true;
        }
    }
    return false;
}

static int recurse_insertions(uint8_t state[54], bt_gd_move_vec *stack, const bt_move *alphabet,
                              size_t alphabet_len, size_t max_insert, const size_t targets[2],
                              uint8_t buffer, bt_gd_budget *budget, bt_gd_insertions *sink) {
    if (stack->len == max_insert || budget->left == 0) {
        return 0;
    }
    for (size_t a = 0; a < alphabet_len; a++) {
        if (!canonical_extension(stack, alphabet[a])) {
            continue;
        }
        apply_move_occ(state, alphabet[a]);
        if (vec_push(stack, alphabet[a]) != 0) {
            apply_move_occ(state, bt_move_invert(alphabet[a]));
            return -1;
        }
        budget->left -= 1;
        if (reaches_target(state, targets, buffer) && insertions_push(sink, stack) != 0) {
            return -1;
        }
        int rc = recurse_insertions(state, stack, alphabet, alphabet_len, max_insert, targets,
                                    buffer, budget, sink);
        stack->len -= 1;
        apply_move_occ(state, bt_move_invert(alphabet[a]));
        if (rc != 0 || budget->left == 0) {
            return rc;
        }
    }
    return 0;
}

typedef struct {
    size_t len;
    size_t index;
} bt_gd_len_key;

static int compare_len_keys(const void *left, const void *right) {
    const bt_gd_len_key *a = left;
    const bt_gd_len_key *b = right;
    if (a->len != b->len) {
        return a->len < b->len ? -1 : 1;
    }
    return a->index < b->index ? -1 : 1;
}

static int insertions_reaching(const uint8_t cube_s[54], const bt_move *alphabet,
                               size_t alphabet_len, size_t max_insert, const size_t targets[2],
                               uint8_t buffer, bt_gd_budget *budget, bt_gd_insertions *sink) {
    bt_gd_move_vec stack = {0};
    uint8_t state[54];
    memcpy(state, cube_s, sizeof state);
    int rc = recurse_insertions(state, &stack, alphabet, alphabet_len, max_insert, targets, buffer,
                                budget, sink);
    vec_free(&stack);
    if (rc != 0) {
        return -1;
    }
    bt_gd_len_key *keys = malloc(sink->count * sizeof(bt_gd_len_key));
    bt_gd_move_vec *sorted = malloc(sink->count * sizeof(bt_gd_move_vec));
    if (keys == NULL || sorted == NULL) {
        free(keys);
        free(sorted);
        return -1;
    }
    for (size_t i = 0; i < sink->count; i++) {
        keys[i].len = sink->items[i].len;
        keys[i].index = i;
    }
    qsort(keys, sink->count, sizeof(bt_gd_len_key), compare_len_keys);
    for (size_t i = 0; i < sink->count; i++) {
        sorted[i] = sink->items[keys[i].index];
    }
    free(keys);
    free(sink->items);
    sink->items = sorted;
    return 0;
}

static void invert_seq(const bt_move *seq, size_t len, bt_gd_move_vec *out) {
    for (size_t i = len; i > 0; i--) {
        vec_push(out, bt_move_invert(seq[i - 1]));
    }
}

static void alg_from_vec(const bt_gd_move_vec *vec, bt_alg *out) {
    bt_alg_init(out);
    for (size_t i = 0; i < vec->len; i++) {
        bt_alg_push(out, vec->items[i]);
    }
}

static void commutator_of(const bt_gd_move_vec *setup, const bt_alg *first, const bt_alg *second,
                          bt_alg *out, char *text, size_t text_len) {
    bt_gd_move_vec moves = {0};
    for (size_t i = 0; i < setup->len; i++) {
        vec_push(&moves, setup->items[i]);
    }
    for (size_t i = 0; i < first->len; i++) {
        vec_push(&moves, first->items[i]);
    }
    for (size_t i = 0; i < second->len; i++) {
        vec_push(&moves, second->items[i]);
    }
    bt_alg first_inv = bt_alg_inverse(first);
    bt_alg second_inv = bt_alg_inverse(second);
    for (size_t i = 0; i < first_inv.len; i++) {
        vec_push(&moves, first_inv.items[i]);
    }
    for (size_t i = 0; i < second_inv.len; i++) {
        vec_push(&moves, second_inv.items[i]);
    }
    bt_gd_move_vec setup_inv = {0};
    invert_seq(setup->items, setup->len, &setup_inv);
    for (size_t i = 0; i < setup_inv.len; i++) {
        vec_push(&moves, setup_inv.items[i]);
    }
    alg_from_vec(&moves, out);
    bt_strbuf first_text;
    bt_strbuf second_text;
    bt_strbuf_init(&first_text, 0);
    bt_strbuf_init(&second_text, 0);
    bt_alg_format(first, &first_text);
    bt_alg_format(second, &second_text);
    if (setup->len == 0) {
        snprintf(text, text_len, "[%s,%s]", first_text.data, second_text.data);
    } else {
        bt_strbuf setup_text;
        bt_strbuf_init(&setup_text, 0);
        bt_alg setup_alg;
        alg_from_vec(setup, &setup_alg);
        bt_alg_format(&setup_alg, &setup_text);
        snprintf(text, text_len, "%s:[%s,%s]", setup_text.data, first_text.data, second_text.data);
        bt_strbuf_free(&setup_text);
        bt_alg_free(&setup_alg);
    }
    bt_strbuf_free(&first_text);
    bt_strbuf_free(&second_text);
    bt_alg_free(&first_inv);
    bt_alg_free(&second_inv);
    vec_free(&setup_inv);
    vec_free(&moves);
}

static void probe_moves(const bt_gd_move_vec *setup, const bt_gd_move_vec *setup_inv,
                        const bt_move *first, size_t first_len, const bt_move *first_inv,
                        size_t first_inv_len, bt_move interchange, bool interchange_first,
                        bt_gd_move_vec *out) {
    for (size_t i = 0; i < setup->len; i++) {
        vec_push(out, setup->items[i]);
    }
    if (interchange_first) {
        vec_push(out, interchange);
        for (size_t i = 0; i < first_len; i++) {
            vec_push(out, first[i]);
        }
        vec_push(out, bt_move_invert(interchange));
        for (size_t i = 0; i < first_inv_len; i++) {
            vec_push(out, first_inv[i]);
        }
    } else {
        for (size_t i = 0; i < first_len; i++) {
            vec_push(out, first[i]);
        }
        vec_push(out, interchange);
        for (size_t i = 0; i < first_inv_len; i++) {
            vec_push(out, first_inv[i]);
        }
        vec_push(out, bt_move_invert(interchange));
    }
    for (size_t i = 0; i < setup_inv->len; i++) {
        vec_push(out, setup_inv->items[i]);
    }
}

static int hits_push(bt_gd_synth_hits *hits, const bt_alg *alg, const char *commutator) {
    if (hits->count == hits->cap_alloc) {
        size_t cap = hits->cap_alloc == 0 ? 8 : hits->cap_alloc * 2;
        bt_gd_synth_hit *items = realloc(hits->items, cap * sizeof(bt_gd_synth_hit));
        if (items == NULL) {
            return -1;
        }
        hits->items = items;
        hits->cap_alloc = cap;
    }
    bt_gd_synth_hit *hit = &hits->items[hits->count];
    bt_alg_init(&hit->alg);
    for (size_t i = 0; i < alg->len; i++) {
        bt_alg_push(&hit->alg, alg->items[i]);
    }
    hit->commutator = _strdup(commutator);
    hits->count++;
    return 0;
}

static int search_with(const uint8_t target[54], uint8_t buffer, const bt_gd_synth_limits *limits,
                       bool wide, bt_gd_synth_hits *hits) {
    bt_move setup_alphabet[GD_SYNTH_ALPHABET_MAX];
    size_t setup_alphabet_len = wide ? single_turns(setup_alphabet) : face_turns(setup_alphabet);
    bt_move insert_alphabet[GD_SYNTH_ALPHABET_MAX];
    size_t insert_alphabet_len = wide ? single_turns(insert_alphabet) : face_turns(insert_alphabet);
    bt_move interchanges[GD_SYNTH_ALPHABET_MAX];
    size_t interchanges_len = single_turns(interchanges);
    bt_gd_budget budget = {.left = limits->node_budget};
    bt_gd_move_vec *setups = NULL;
    size_t setup_count = 0;
    if (setups_by_depth(setup_alphabet, setup_alphabet_len, limits->max_setup, &setups,
                        &setup_count) != 0) {
        return -1;
    }
    bool plateau_set = false;
    uint32_t plateau_qtm = 0;
    int rc = 0;
    for (size_t s = 0; s < setup_count; s++) {
        if (budget.left == 0) {
            break;
        }
        const bt_gd_move_vec *setup = &setups[s];
        uint8_t cube_s[54];
        solved_occ(cube_s);
        apply_seq_occ(cube_s, setup->items, setup->len);
        uint8_t t_s[54];
        memcpy(t_s, target, sizeof t_s);
        apply_seq_occ(t_s, setup->items, setup->len);
        size_t buffer_slot = 54;
        for (size_t i = 0; i < 54; i++) {
            if (cube_s[i] == buffer) {
                buffer_slot = i;
                break;
            }
        }
        if (buffer_slot == 54) {
            continue;
        }
        size_t slots[3];
        if (!cycle_slots(cube_s, t_s, buffer_slot, slots)) {
            continue;
        }
        size_t targets[2] = {slots[1], slots[2]};
        bt_move usable[GD_SYNTH_ALPHABET_MAX];
        size_t usable_len = 0;
        for (size_t i = 0; i < interchanges_len; i++) {
            uint8_t table[54];
            bt_move_table(interchanges[i], table);
            if (table[targets[0]] != (uint8_t)targets[0] &&
                table[targets[1]] != (uint8_t)targets[1]) {
                usable[usable_len++] = interchanges[i];
            }
        }
        if (usable_len == 0) {
            continue;
        }
        bt_gd_move_vec setup_inv = {0};
        invert_seq(setup->items, setup->len, &setup_inv);
        bt_gd_insertions found = {0};
        if (insertions_reaching(cube_s, insert_alphabet, insert_alphabet_len, limits->max_insert,
                                targets, buffer, &budget, &found) != 0) {
            vec_free(&setup_inv);
            rc = -1;
            break;
        }
        for (size_t f = 0; f < found.count && rc == 0; f++) {
            bt_gd_move_vec variants[2] = {{0}, {0}};
            variants[0] = found.items[f];
            invert_seq(found.items[f].items, found.items[f].len, &variants[1]);
            for (size_t v = 0; v < 2 && rc == 0; v++) {
                bt_gd_move_vec first_inv = {0};
                invert_seq(variants[v].items, variants[v].len, &first_inv);
                for (size_t u = 0; u < usable_len && rc == 0; u++) {
                    for (int order = 0; order < 2 && rc == 0; order++) {
                        bt_gd_move_vec probe = {0};
                        probe_moves(setup, &setup_inv, variants[v].items, variants[v].len,
                                    first_inv.items, first_inv.len, usable[u], order == 1, &probe);
                        uint8_t occ[54];
                        solved_occ(occ);
                        apply_seq_occ(occ, probe.items, probe.len);
                        if (occ_equals(occ, target)) {
                            bt_alg first_alg;
                            alg_from_vec(&variants[v], &first_alg);
                            bt_alg second_alg;
                            bt_alg_init(&second_alg);
                            bt_alg_push(&second_alg, usable[u]);
                            bt_alg alg;
                            char commutator[512];
                            commutator_of(setup, order == 1 ? &second_alg : &first_alg,
                                          order == 1 ? &first_alg : &second_alg, &alg, commutator,
                                          sizeof commutator);
                            uint32_t qtm = bt_alg_qtm(&alg);
                            bool push = true;
                            if (!plateau_set) {
                                plateau_set = true;
                                plateau_qtm = qtm;
                            } else if (qtm > plateau_qtm) {
                                push = false;
                            }
                            if (push) {
                                rc = hits_push(hits, &alg, commutator);
                            }
                            bt_alg_free(&alg);
                            bt_alg_free(&second_alg);
                            bt_alg_free(&first_alg);
                        }
                        vec_free(&probe);
                    }
                }
                vec_free(&first_inv);
            }
            vec_free(&variants[1]);
        }
        for (size_t f = 0; f < found.count; f++) {
            vec_free(&found.items[f]);
        }
        free(found.items);
        vec_free(&setup_inv);
        if (rc != 0) {
            break;
        }
        if (plateau_set) {
            break;
        }
    }
    for (size_t s = 0; s < setup_count; s++) {
        vec_free(&setups[s]);
    }
    free(setups);
    return rc;
}

void bt_gd_synth_hit_free(bt_gd_synth_hit *hit) {
    bt_alg_free(&hit->alg);
    free(hit->commutator);
    hit->commutator = NULL;
}

void bt_gd_synth_hits_free(bt_gd_synth_hits *hits) {
    for (size_t i = 0; i < hits->count; i++) {
        bt_gd_synth_hit_free(&hits->items[i]);
    }
    free(hits->items);
    hits->items = NULL;
    hits->count = 0;
    hits->cap_alloc = 0;
}

int bt_gd_synth_candidates(bt_orbit orbit, uint8_t t1, uint8_t t2, const bt_gd_synth_limits *limits,
                           bt_gd_synth_hits *out) {
    BT_TRACE_ENTER("gd.synth.candidates");
    memset(out, 0, sizeof *out);
    uint8_t buffer =
        orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER : BT_SCHEME_CORNER_BUFFER_STICKER;
    uint8_t target[54];
    solved_occ(target);
    bt_scheme_cycles cycles;
    bt_scheme_ideal_cycle_at(orbit, buffer, t1, t2, &cycles);
    bt_scheme_apply_sticker_cycles(target, &cycles);
    int rc = search_with(target, buffer, limits, false, out);
    if (rc == 0 && out->count == 0) {
        rc = search_with(target, buffer, limits, true, out);
    }
    if (rc != 0) {
        BT_TRACE_FAIL("gd.synth.candidates", "search failed");
    } else {
        BT_TRACE_EXIT("gd.synth.candidates");
    }
    return rc;
}

bool bt_gd_synth_case(bt_orbit orbit, uint8_t t1, uint8_t t2, const bt_gd_synth_limits *limits,
                      const bt_gd_scoring_cfg *scoring, bt_gd_synth_hit *out) {
    BT_TRACE_ENTER("gd.synth.case");
    bt_gd_synth_hits candidates = {0};
    if (bt_gd_synth_candidates(orbit, t1, t2, limits, &candidates) != 0 || candidates.count == 0) {
        bt_gd_synth_hits_free(&candidates);
        BT_TRACE_FAIL("gd.synth.case", "no candidates");
        return false;
    }
    size_t best = 0;
    for (size_t i = 1; i < candidates.count; i++) {
        if (bt_gd_rank_cmp(candidates.items[i].alg.items, candidates.items[i].alg.len,
                           candidates.items[best].alg.items, candidates.items[best].alg.len, orbit,
                           scoring) < 0) {
            best = i;
        }
    }
    *out = candidates.items[best];
    for (size_t i = 0; i < candidates.count; i++) {
        if (i != best) {
            bt_gd_synth_hit_free(&candidates.items[i]);
        }
    }
    free(candidates.items);
    BT_TRACE_EXIT("gd.synth.case");
    return true;
}

static void note_text(const bt_note *note, char *out, size_t out_len) {
    if (note->kind == BT_NOTE_FLIP) {
        snprintf(out, out_len, "flip:%s", note->piece);
    } else {
        snprintf(out, out_len, "twist:%s:%+d", note->piece, (int)note->twist);
    }
}

static int vendor_alg_from_verdict(const char *solver, size_t rowid, const char *position,
                                   const bt_alg *alg, const char *alg_text,
                                   const bt_verdict *verdict, const char *commutator,
                                   bt_gd_vendor_alg *out) {
    memset(out, 0, sizeof *out);
    out->solver = _strdup(solver);
    out->rowid = rowid;
    out->position = _strdup(position);
    out->alg = _strdup(alg_text);
    out->qtm = bt_alg_qtm(alg);
    out->htm = bt_alg_htm(alg);
    out->ok = bt_verdict_ok(verdict);
    out->problem_count = verdict->problem_count;
    out->problems = out->problem_count > 0 ? malloc(out->problem_count * sizeof(char *)) : NULL;
    if (out->problems == NULL && out->problem_count > 0) {
        bt_gd_vendor_alg_free(out);
        return -1;
    }
    for (size_t i = 0; i < verdict->problem_count; i++) {
        char text[BT_VERIFY_DETAIL_MAX + 32];
        bt_problem_string(&verdict->problems[i], text, sizeof text);
        out->problems[i] = _strdup(text);
    }
    out->note_count = verdict->note_count;
    out->notes = out->note_count > 0 ? malloc(out->note_count * sizeof(char *)) : NULL;
    if (out->notes == NULL && out->note_count > 0) {
        bt_gd_vendor_alg_free(out);
        return -1;
    }
    for (size_t i = 0; i < verdict->note_count; i++) {
        char text[64];
        note_text(&verdict->notes[i], text, sizeof text);
        out->notes[i] = _strdup(text);
    }
    out->commutator = _strdup(commutator);
    out->recon_link = _strdup("");
    out->youtube_link = _strdup("");
    return 0;
}

void bt_gd_synthesize(bt_gd_cases_file *cases, const bt_gd_synth_limits *limits,
                      const bt_gd_scoring_cfg *scoring, size_t *edge_added, size_t *corner_added) {
    BT_TRACE_ENTER("gd.synth.synthesize");
    *edge_added = 0;
    *corner_added = 0;
    for (size_t i = 0; i < cases->count; i++) {
        bt_gd_case_entry *entry = &cases->cases[i];
        if (entry->vendor_count != 0) {
            continue;
        }
        bt_orbit orbit = strcmp(entry->orbit, "edge") == 0 ? BT_ORBIT_EDGE : BT_ORBIT_CORNER;
        bt_gd_synth_hit hit;
        if (!bt_gd_synth_case(orbit, entry->t1, entry->t2, limits, scoring, &hit)) {
            continue;
        }
        uint8_t buffer = orbit == BT_ORBIT_EDGE ? BT_SCHEME_EDGE_BUFFER_STICKER
                                                : BT_SCHEME_CORNER_BUFFER_STICKER;
        bt_sheet_class sheet = orbit == BT_ORBIT_EDGE ? BT_SHEET_EDGE : BT_SHEET_CORNER;
        char position[96];
        snprintf(position, sizeof position, "%s-%s-%s", bt_scheme_name_of(buffer),
                 bt_scheme_name_of(entry->t1), bt_scheme_name_of(entry->t2));
        bt_verdict verdict;
        bt_verdict_init(&verdict);
        bt_verify(&hit.alg, position, sheet, &verdict);
        bt_strbuf alg_text;
        bt_strbuf_init(&alg_text, 0);
        bt_alg_format(&hit.alg, &alg_text);
        bt_gd_vendor_alg generated;
        if (vendor_alg_from_verdict("generated", 0, position, &hit.alg, alg_text.data, &verdict,
                                    hit.commutator, &generated) == 0) {
            bt_gd_vendor_alg *grown =
                realloc(entry->vendor_algs, (entry->vendor_count + 1) * sizeof(bt_gd_vendor_alg));
            if (grown != NULL) {
                entry->vendor_algs = grown;
                entry->vendor_algs[entry->vendor_count++] = generated;
                entry->covered = true;
                if (orbit == BT_ORBIT_EDGE) {
                    (*edge_added)++;
                } else {
                    (*corner_added)++;
                }
            } else {
                bt_gd_vendor_alg_free(&generated);
            }
        }
        bt_strbuf_free(&alg_text);
        bt_verdict_free(&verdict);
        bt_gd_synth_hit_free(&hit);
    }
    BT_TRACE_EXIT("gd.synth.synthesize");
}
