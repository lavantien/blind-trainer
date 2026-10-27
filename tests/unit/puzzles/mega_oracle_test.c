#include "puzzles/mega.h"
#include "puzzles/tier1.h"

#include "common/bt_harness.h"
#include "common/rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PINNED_TOKENS[16] = {
    "U", "U'", "U2", "U2'", "R", "R'", "R2", "R2'", "L", "L'", "F", "F'", "BR", "BR'", "y", "y'",
};

static const char *const REJECTED_TOKENS[] = {
    "D",   "D'", "D2",  "B",   "B'",  "BL", "BL'", "UL",  "UL'",  "UR",  "UR'", "DR",
    "DR'", "DL", "DL'", "L2",  "L2'", "F2", "F2'", "BR2", "BR2'", "y2",  "y2'", "x",
    "x'",  "z",  "u",   "u'",  "r",   "r'", "f",   "f'",  "br",   "br'", "l",   "U3",
    "R3",  "U0", "U''", "U'2", "2U",  "UU", "UR'", "Rw",  "Uw",   "M",   "E",   "S",
    "e2",  "-",  ",",   "R,",  "(R",  "U)", "U]",  "[(R", "*2",
};

static const size_t FACE_VERTEX[BT_MEGA_FACES] = {0, 4, 2, 3, 5, 1, 7, 11, 6, 10, 8, 9};

static const long long VERTICES[12][3][2] = {
    {{0, 0}, {1, 0}, {0, 1}},   {{0, 0}, {-1, 0}, {0, 1}},  {{1, 0}, {0, 1}, {0, 0}},
    {{-1, 0}, {0, 1}, {0, 0}},  {{0, 1}, {0, 0}, {1, 0}},   {{0, -1}, {0, 0}, {1, 0}},
    {{0, 0}, {1, 0}, {0, -1}},  {{0, 0}, {-1, 0}, {0, -1}}, {{1, 0}, {0, -1}, {0, 0}},
    {{-1, 0}, {0, -1}, {0, 0}}, {{0, 1}, {0, 0}, {-1, 0}},  {{0, -1}, {0, 0}, {-1, 0}},
};

static bool tier1_ok(const char *text) {
    bt_pz_engine eng = bt_mega_engine();
    bt_tier1_error err;
    return bt_verify_tier1(&eng, text, &err) == BT_TIER1_OK;
}

static double phi_value(void) {
    return (1.0 + sqrt(5.0)) / 2.0;
}

static char *split_space(char *str, char **save) {
    char *start = str != NULL ? str : *save;
    if (start == NULL) {
        return NULL;
    }
    while (*start == ' ') {
        start++;
    }
    if (*start == '\0') {
        *save = start;
        return NULL;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    if (*end != '\0') {
        *end = '\0';
        end++;
    }
    *save = end;
    return start;
}

static size_t face_of_vertex(size_t vertex) {
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        if (FACE_VERTEX[f] == vertex) {
            return f;
        }
    }
    BT_ABORT("vertex %zu has no face", vertex);
}

static double coord_value(long long a, long long b) {
    return (double)a + (double)b * phi_value();
}

static void vertex_xyz(size_t index, double out[3]) {
    for (size_t axis = 0; axis < 3; axis++) {
        out[axis] = coord_value(VERTICES[index][axis][0], VERTICES[index][axis][1]);
    }
}

static void square_pair(long long a, long long b, long long out[2]) {
    out[0] = a * a + b * b;
    out[1] = 2 * a * b + b * b;
}

static size_t exact_adjacency(size_t vertex, size_t out[5]) {
    size_t count = 0;
    for (size_t j = 0; j < 12; j++) {
        if (j == vertex) {
            continue;
        }
        long long r1 = 0;
        long long rphi = 0;
        for (size_t axis = 0; axis < 3; axis++) {
            long long sq[2];
            square_pair(VERTICES[vertex][axis][0] - VERTICES[j][axis][0],
                        VERTICES[vertex][axis][1] - VERTICES[j][axis][1], sq);
            r1 += sq[0];
            rphi += sq[1];
        }
        if (r1 == 4 && rphi == 0) {
            out[count] = j;
            count++;
        }
    }
    return count;
}

static void unit_vec(const double v[3], double out[3]) {
    double norm = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    out[0] = v[0] / norm;
    out[1] = v[1] / norm;
    out[2] = v[2] / norm;
}

static void rotate_cw(const double v[3], const double axis[3], double out[3]) {
    double rad = -72.0 * 3.14159265358979323846 / 180.0;
    double c = cos(rad);
    double s = sin(rad);
    double cross[3] = {
        axis[1] * v[2] - axis[2] * v[1],
        axis[2] * v[0] - axis[0] * v[2],
        axis[0] * v[1] - axis[1] * v[0],
    };
    double d = axis[0] * v[0] + axis[1] * v[1] + axis[2] * v[2];
    for (size_t i = 0; i < 3; i++) {
        out[i] = v[i] * c + cross[i] * s + axis[i] * d * (1.0 - c);
    }
}

static double dist2(const double a[3], const double b[3]) {
    double d0 = a[0] - b[0];
    double d1 = a[1] - b[1];
    double d2 = a[2] - b[2];
    return d0 * d0 + d1 * d1 + d2 * d2;
}

static size_t nearest(const double target[3], const size_t *candidates, size_t count) {
    double best_dist = 0.0;
    size_t best = candidates[0];
    for (size_t i = 0; i < count; i++) {
        double cand[3];
        vertex_xyz(candidates[i], cand);
        double d = dist2(target, cand);
        if (i == 0 || d < best_dist) {
            best_dist = d;
            best = candidates[i];
        }
    }
    return best;
}

static void derived_ring(size_t face, size_t out[BT_MEGA_RING]) {
    size_t vertex = FACE_VERTEX[face];
    double vertex_axis[3];
    vertex_xyz(vertex, vertex_axis);
    double axis[3];
    unit_vec(vertex_axis, axis);
    size_t neighbors[5];
    size_t neighbor_count = exact_adjacency(vertex, neighbors);
    BT_CHECK(neighbor_count == 5);
    size_t start = neighbors[0];
    for (size_t i = 1; i < neighbor_count; i++) {
        if (face_of_vertex(neighbors[i]) < face_of_vertex(start)) {
            start = neighbors[i];
        }
    }
    size_t current = start;
    for (size_t step = 0; step < 5; step++) {
        out[step] = face_of_vertex(current);
        double from[3];
        double rotated[3];
        vertex_xyz(current, from);
        rotate_cw(from, axis, rotated);
        current = nearest(rotated, neighbors, neighbor_count);
    }
    BT_CHECK(current == start);
}

static void derived_y_perm(size_t out[BT_MEGA_FACES]) {
    double vertex_axis[3];
    vertex_xyz(FACE_VERTEX[BT_MEGA_U], vertex_axis);
    double axis[3];
    unit_vec(vertex_axis, axis);
    size_t all[12];
    for (size_t i = 0; i < 12; i++) {
        all[i] = i;
    }
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        double from[3];
        double rotated[3];
        vertex_xyz(FACE_VERTEX[f], from);
        rotate_cw(from, axis, rotated);
        out[f] = face_of_vertex(nearest(rotated, all, 12));
    }
}

static const char *face_token(uint8_t face) {
    if (face == BT_MEGA_U) {
        return "U";
    }
    if (face == BT_MEGA_R) {
        return "R";
    }
    if (face == BT_MEGA_L) {
        return "L";
    }
    if (face == BT_MEGA_F) {
        return "F";
    }
    return "BR";
}

static void colors_after(const char *text, bt_mega_state *out) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg(text, strlen(text), alg, &err)) {
        BT_ABORT("parse %s failed: %s", text, err.token);
    }
    bt_mega_solved_state(out);
    if (!bt_mega_apply_alg(out, alg, &err)) {
        BT_ABORT("apply %s failed: %s", text, err.token);
    }
    bt_pz_alg_destroy(alg);
}

static void all_slots(uint8_t out[BT_MEGA_PER_FACE]) {
    out[0] = BT_MEGA_CENTER;
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        out[1 + k] = bt_mega_corner_slot(k);
        out[BT_MEGA_RING + 1 + k] = bt_mega_edge_slot(k);
    }
}

static size_t moved_slots(const bt_mega_state *before, const bt_mega_state *after, uint8_t face,
                          uint8_t out[BT_MEGA_PER_FACE]) {
    uint8_t slots[BT_MEGA_PER_FACE];
    all_slots(slots);
    size_t count = 0;
    for (size_t i = 0; i < BT_MEGA_PER_FACE; i++) {
        if (bt_mega_color(before, face, slots[i]) != bt_mega_color(after, face, slots[i])) {
            out[count] = slots[i];
            count++;
        }
    }
    return count;
}

static bool ring_contains(const uint8_t *ring, uint8_t face) {
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        if (ring[k] == face) {
            return true;
        }
    }
    return false;
}

static size_t ring_position(const uint8_t *ring, uint8_t face) {
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        if (ring[k] == face) {
            return k;
        }
    }
    BT_ABORT("face %u not in ring", face);
}

BT_TEST(mega_oracle, pinned_tokens_parse_format_and_round_trip) {
    bt_pz_error err;
    for (size_t i = 0; i < 16; i++) {
        bt_mega_move move;
        BT_CHECK(bt_mega_parse_move(PINNED_TOKENS[i], strlen(PINNED_TOKENS[i]), &move, NULL));
        void *alg = bt_pz_alg_create();
        if (!bt_mega_parse_alg(PINNED_TOKENS[i], strlen(PINNED_TOKENS[i]), alg, &err)) {
            BT_ABORT("parse %s failed: %s", PINNED_TOKENS[i], err.token);
        }
        BT_CHECK_EQ(bt_pz_alg_len(alg), 1u);
        bt_pz_engine eng = bt_mega_engine();
        bt_mega_move parsed = *(const bt_mega_move *)bt_pz_alg_at(&eng, alg, 0);
        BT_CHECK(bt_mega_move_eq(parsed, move));
        bt_strbuf text;
        bt_strbuf_init(&text, 32);
        bt_mega_format_alg(alg, &text);
        BT_CHECK_EQ(text.data, PINNED_TOKENS[i]);
        bt_strbuf_free(&text);
        bt_pz_alg_destroy(alg);
        BT_CHECK(tier1_ok(PINNED_TOKENS[i]));
    }
}

BT_TEST(mega_oracle, rejects_tokens_outside_the_pinned_grammar) {
    for (size_t i = 0; i < sizeof REJECTED_TOKENS / sizeof REJECTED_TOKENS[0]; i++) {
        bt_mega_move move;
        BT_CHECK(!bt_mega_parse_move(REJECTED_TOKENS[i], strlen(REJECTED_TOKENS[i]), &move, NULL));
    }
}

BT_TEST(mega_oracle, rejects_rogers_twelve_face_vocabulary_in_algs) {
    const char *texts[] = {
        "D", "R D R'", "B L B'", "BL U BL'", "UR2 U'", "y DR", "x U x'", "U2 D2",
    };
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        BT_CHECK(!bt_mega_parse_alg(texts[i], strlen(texts[i]), alg, &err));
        BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_PARSE);
    }
    bt_pz_alg_destroy(alg);
}

BT_TEST(mega_oracle, row_4lll_33_and_raw_grouping_are_wave_b_normalizer_scope) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    BT_CHECK(!bt_mega_parse_alg("[(R U R' U) (R' U' R2 U') (R' U R' U R) U] *2",
                                strlen("[(R U R' U) (R' U' R2 U') (R' U R' U R) U] *2"), alg,
                                &err));
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_PARSE);
    BT_CHECK_EQ(err.token, "[(R");
    BT_CHECK(!bt_mega_parse_alg("F (R U R' U') F'", strlen("F (R U R' U') F'"), alg, &err));
    BT_CHECK_EQ(err.kind, (int)BT_PZ_ERR_PARSE);
    BT_CHECK_EQ(err.token, "(R");
    bt_pz_alg_destroy(alg);
}

BT_TEST(mega_oracle, adjacency_closes_symmetrically) {
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[f];
        uint8_t sorted[BT_MEGA_RING];
        memcpy(sorted, ring, BT_MEGA_RING);
        for (size_t i = 1; i < BT_MEGA_RING; i++) {
            uint8_t key = sorted[i];
            size_t j = i;
            while (j > 0 && sorted[j - 1] > key) {
                sorted[j] = sorted[j - 1];
                j--;
            }
            sorted[j] = key;
        }
        for (size_t i = 1; i < BT_MEGA_RING; i++) {
            BT_CHECK(sorted[i - 1] != sorted[i]);
        }
        BT_CHECK(!ring_contains(ring, (uint8_t)f));
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            BT_CHECK(ring_contains(BT_MEGA_NEIGHBOR_CW[ring[k]], (uint8_t)f));
        }
    }
}

BT_TEST(mega_oracle, adjacency_closure_holds_for_any_face) {
    for (size_t face = 0; face < BT_MEGA_FACES; face++) {
        const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[face];
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            uint8_t n = ring[k];
            BT_CHECK(n != face);
            BT_CHECK(ring_contains(BT_MEGA_NEIGHBOR_CW[n], (uint8_t)face));
            BT_CHECK(n < BT_MEGA_FACES);
        }
    }
}

BT_TEST(mega_oracle, adjacency_matches_the_golden_icosahedron_exactly) {
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        size_t neighbors[5];
        size_t neighbor_count = exact_adjacency(FACE_VERTEX[f], neighbors);
        BT_CHECK(neighbor_count == 5);
        size_t derived[5];
        for (size_t i = 0; i < 5; i++) {
            derived[i] = face_of_vertex(neighbors[i]);
        }
        size_t pinned[5];
        for (size_t i = 0; i < 5; i++) {
            pinned[i] = BT_MEGA_NEIGHBOR_CW[f][i];
        }
        for (size_t i = 1; i < 5; i++) {
            size_t key = derived[i];
            size_t j = i;
            while (j > 0 && derived[j - 1] > key) {
                derived[j] = derived[j - 1];
                j--;
            }
            derived[j] = key;
            key = pinned[i];
            j = i;
            while (j > 0 && pinned[j - 1] > key) {
                pinned[j] = pinned[j - 1];
                j--;
            }
            pinned[j] = key;
        }
        for (size_t i = 0; i < 5; i++) {
            BT_CHECK(derived[i] == pinned[i]);
        }
    }
}

static size_t antipode_of(size_t source) {
    long long dist[BT_MEGA_FACES];
    for (size_t i = 0; i < BT_MEGA_FACES; i++) {
        dist[i] = -1;
    }
    dist[source] = 0;
    size_t queue[BT_MEGA_FACES];
    size_t head = 0;
    size_t tail = 0;
    queue[tail] = source;
    tail++;
    while (head < tail) {
        size_t v = queue[head];
        head++;
        const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[v];
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            size_t n = ring[k];
            if (dist[n] == -1) {
                dist[n] = dist[v] + 1;
                queue[tail] = n;
                tail++;
            }
        }
    }
    size_t counts[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < BT_MEGA_FACES; i++) {
        counts[dist[i]]++;
    }
    BT_CHECK(counts[0] == 1 && counts[1] == 5 && counts[2] == 5 && counts[3] == 1);
    for (size_t i = 0; i < BT_MEGA_FACES; i++) {
        if (dist[i] == 3) {
            return i;
        }
    }
    BT_ABORT("no antipode for %zu", source);
}

BT_TEST(mega_oracle, graph_distances_pin_the_antipodes) {
    const size_t pairs[6][2] = {
        {BT_MEGA_U, BT_MEGA_D},  {BT_MEGA_F, BT_MEGA_B},   {BT_MEGA_R, BT_MEGA_UL},
        {BT_MEGA_L, BT_MEGA_UR}, {BT_MEGA_BR, BT_MEGA_DL}, {BT_MEGA_BL, BT_MEGA_DR},
    };
    for (size_t source = 0; source < BT_MEGA_FACES; source++) {
        size_t far = antipode_of(source);
        BT_CHECK(far != source);
        BT_CHECK(antipode_of(far) == source);
    }
    for (size_t i = 0; i < 6; i++) {
        BT_CHECK(antipode_of(pairs[i][0]) == pairs[i][1]);
    }
}

BT_TEST(mega_oracle, ring_orders_form_a_valid_rotation_system) {
    for (int direction = 0; direction < 2; direction++) {
        bool visited[BT_MEGA_FACES * BT_MEGA_FACES] = {false};
        size_t orbit_count = 0;
        for (size_t g = 0; g < BT_MEGA_FACES; g++) {
            for (size_t k = 0; k < BT_MEGA_RING; k++) {
                size_t h = BT_MEGA_NEIGHBOR_CW[g][k];
                size_t dart = g * BT_MEGA_FACES + h;
                if (visited[dart]) {
                    continue;
                }
                size_t orbit[3][2];
                size_t orbit_len = 0;
                while (!visited[dart]) {
                    visited[dart] = true;
                    size_t from = dart / BT_MEGA_FACES;
                    size_t to = dart % BT_MEGA_FACES;
                    if (orbit_len < 3) {
                        orbit[orbit_len][0] = from;
                        orbit[orbit_len][1] = to;
                    }
                    orbit_len++;
                    const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[to];
                    size_t pos = ring_position(ring, (uint8_t)from);
                    size_t next =
                        direction ? ring[(pos + 1) % BT_MEGA_RING] : ring[(pos + 4) % BT_MEGA_RING];
                    dart = to * BT_MEGA_FACES + next;
                }
                BT_CHECK(orbit_len == 3);
                size_t a = orbit[0][0];
                size_t b = orbit[0][1];
                size_t c = orbit[1][1];
                BT_CHECK(c != a);
                BT_CHECK(c != b);
                BT_CHECK(ring_contains(BT_MEGA_NEIGHBOR_CW[a], (uint8_t)b));
                BT_CHECK(ring_contains(BT_MEGA_NEIGHBOR_CW[a], (uint8_t)c));
                BT_CHECK(ring_contains(BT_MEGA_NEIGHBOR_CW[b], (uint8_t)c));
                orbit_count++;
            }
        }
        BT_CHECK_EQ((long long)orbit_count, 20);
    }
}

BT_TEST(mega_oracle, ring_orders_match_independent_rotation_derivation) {
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        size_t derived[BT_MEGA_RING];
        derived_ring(f, derived);
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            BT_CHECK(derived[k] == BT_MEGA_NEIGHBOR_CW[f][k]);
        }
    }
    const uint8_t u_ring[BT_MEGA_RING] = {BT_MEGA_F, BT_MEGA_L, BT_MEGA_BL, BT_MEGA_BR, BT_MEGA_R};
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        BT_CHECK_EQ(BT_MEGA_NEIGHBOR_CW[BT_MEGA_U][k], u_ring[k]);
    }
}

BT_TEST(mega_oracle, y_perm_matches_independent_derivation_and_structure) {
    size_t derived[BT_MEGA_FACES];
    derived_y_perm(derived);
    for (size_t f = 0; f < BT_MEGA_FACES; f++) {
        BT_CHECK(derived[f] == BT_MEGA_Y_PERM[f]);
    }
    BT_CHECK_EQ(BT_MEGA_Y_PERM[BT_MEGA_U], BT_MEGA_U);
    BT_CHECK_EQ(BT_MEGA_Y_PERM[BT_MEGA_D], BT_MEGA_D);
    const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[BT_MEGA_U];
    for (size_t k = 0; k < BT_MEGA_RING; k++) {
        BT_CHECK_EQ(BT_MEGA_Y_PERM[ring[k]], ring[(k + 1) % BT_MEGA_RING]);
    }
    for (size_t g = 0; g < BT_MEGA_FACES; g++) {
        uint8_t image[BT_MEGA_RING];
        const uint8_t *target = BT_MEGA_NEIGHBOR_CW[BT_MEGA_Y_PERM[g]];
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            image[k] = BT_MEGA_Y_PERM[BT_MEGA_NEIGHBOR_CW[g][k]];
        }
        bool shifted = false;
        for (size_t t = 0; t < BT_MEGA_RING; t++) {
            bool matches = true;
            for (size_t k = 0; k < BT_MEGA_RING; k++) {
                matches = matches && target[(k + t) % BT_MEGA_RING] == image[k];
            }
            shifted = shifted || matches;
        }
        BT_CHECK(shifted);
    }
}

BT_TEST(mega_oracle, quarter_turn_anatomy_moves_the_layer_strips) {
    const uint8_t faces[5] = {BT_MEGA_U, BT_MEGA_R, BT_MEGA_L, BT_MEGA_F, BT_MEGA_BR};
    for (size_t index = 0; index < 5; index++) {
        uint8_t face = faces[index];
        const char *text = face_token(face);
        bt_mega_state before;
        bt_mega_state after;
        bt_mega_solved_state(&before);
        colors_after(text, &after);
        BT_CHECK(!bt_mega_is_solved(&after));
        size_t total = 0;
        for (uint8_t probe = 0; probe < BT_MEGA_FACES; probe++) {
            uint8_t moved[BT_MEGA_PER_FACE];
            size_t moved_count = moved_slots(&before, &after, probe, moved);
            total += moved_count;
            if (probe == face) {
                BT_CHECK(moved_count == 0);
                BT_CHECK_EQ(bt_mega_color(&after, face, BT_MEGA_CENTER), face);
            } else if (ring_contains(BT_MEGA_NEIGHBOR_CW[face], probe)) {
                BT_CHECK_EQ((long long)moved_count, 3);
                const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[probe];
                size_t pos = ring_position(ring, face);
                bool edge_found = false;
                bool corner_found = false;
                bool corner_prev = false;
                for (size_t i = 0; i < moved_count; i++) {
                    edge_found = edge_found || moved[i] == bt_mega_edge_slot((pos + 4) % 5);
                    corner_found = corner_found || moved[i] == bt_mega_corner_slot(pos);
                    corner_prev = corner_prev || moved[i] == bt_mega_corner_slot((pos + 4) % 5);
                }
                BT_CHECK(edge_found);
                BT_CHECK(corner_found);
                BT_CHECK(corner_prev);
            } else {
                BT_CHECK(moved_count == 0);
            }
        }
        BT_CHECK_EQ((long long)total, 15);
        bt_pz_error err;
        void *alg = bt_pz_alg_create();
        if (!bt_mega_parse_alg(text, strlen(text), alg, &err)) {
            BT_ABORT("parse failed: %s", err.token);
        }
        bt_mega_state five;
        bt_mega_solved_state(&five);
        for (size_t i = 0; i < 5; i++) {
            if (!bt_mega_apply_alg(&five, alg, &err)) {
                BT_ABORT("apply failed: %s", err.token);
            }
        }
        BT_CHECK(bt_mega_is_solved(&five));
        bt_pz_alg_destroy(alg);
    }
}

BT_TEST(mega_oracle, strip_donors_follow_the_cw_ring) {
    const uint8_t faces[5] = {BT_MEGA_U, BT_MEGA_R, BT_MEGA_L, BT_MEGA_F, BT_MEGA_BR};
    for (size_t index = 0; index < 5; index++) {
        uint8_t face = faces[index];
        bt_mega_state before;
        bt_mega_state after;
        bt_mega_solved_state(&before);
        colors_after(face_token(face), &after);
        const uint8_t *ring = BT_MEGA_NEIGHBOR_CW[face];
        for (size_t k = 0; k < BT_MEGA_RING; k++) {
            uint8_t target = ring[(k + 1) % BT_MEGA_RING];
            uint8_t donor = ring[k];
            uint8_t moved[BT_MEGA_PER_FACE];
            size_t moved_count = moved_slots(&before, &after, target, moved);
            BT_CHECK_EQ((long long)moved_count, 3);
            for (size_t i = 0; i < moved_count; i++) {
                BT_CHECK_EQ(bt_mega_color(&after, target, moved[i]), donor);
            }
        }
    }
}

BT_TEST(mega_oracle, conjugation_by_y_moves_turns_around_the_u_ring) {
    bt_mega_state a;
    bt_mega_state b;
    colors_after("y R y'", &a);
    colors_after("BR", &b);
    BT_CHECK(bt_mega_state_eq(&a, &b));
    colors_after("y' R y", &a);
    colors_after("F", &b);
    BT_CHECK(bt_mega_state_eq(&a, &b));
    colors_after("y' BR y", &a);
    colors_after("R", &b);
    BT_CHECK(bt_mega_state_eq(&a, &b));
}

BT_TEST(mega_oracle, y_shares_the_u_axis) {
    bt_mega_state a;
    bt_mega_state b;
    colors_after("y U y'", &a);
    colors_after("U", &b);
    BT_CHECK(bt_mega_state_eq(&a, &b));
    colors_after("y U", &a);
    colors_after("U y", &b);
    BT_CHECK(bt_mega_state_eq(&a, &b));
    bt_mega_state state;
    colors_after("y", &state);
    BT_CHECK(!bt_mega_is_solved(&state));
    uint8_t slots[BT_MEGA_PER_FACE];
    all_slots(slots);
    for (uint8_t face = 0; face < BT_MEGA_FACES; face++) {
        uint8_t head = bt_mega_color(&state, face, BT_MEGA_CENTER);
        for (size_t i = 0; i < BT_MEGA_PER_FACE; i++) {
            BT_CHECK_EQ(bt_mega_color(&state, face, slots[i]), head);
        }
    }
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg("y", strlen("y"), alg, &err)) {
        BT_ABORT("parse y failed: %s", err.token);
    }
    for (size_t i = 0; i < 4; i++) {
        if (!bt_mega_apply_alg(&state, alg, &err)) {
            BT_ABORT("apply y failed: %s", err.token);
        }
    }
    BT_CHECK(bt_mega_is_solved(&state));
    bt_pz_alg_destroy(alg);
}

BT_TEST(mega_oracle, random_algs_pass_tier1_and_round_trips) {
    bt_rng rng;
    bt_rng_seed(&rng, 20261012);
    bt_pz_error err;
    bt_pz_engine eng = bt_mega_engine();
    for (size_t iteration = 0; iteration < 256; iteration++) {
        size_t length = (size_t)bt_rng_below(&rng, 12);
        char text[512];
        size_t at = 0;
        for (size_t i = 0; i < length; i++) {
            size_t pick = (size_t)bt_rng_below(&rng, 16);
            at += (size_t)snprintf(text + at, sizeof text - at, "%s ", PINNED_TOKENS[pick]);
        }
        if (at > 0) {
            text[at - 1] = '\0';
        } else {
            text[0] = '\0';
        }
        if (!tier1_ok(text)) {
            BT_ABORT("tier1 failed for %s", text);
        }
        void *alg = bt_pz_alg_create();
        if (!bt_mega_parse_alg(text, strlen(text), alg, &err)) {
            BT_ABORT("parse %s failed: %s", text, err.token);
        }
        bt_strbuf formatted;
        bt_strbuf_init(&formatted, 512);
        bt_mega_format_alg(alg, &formatted);
        BT_CHECK_EQ(formatted.data, text);
        void *inverse = bt_pz_alg_create();
        void *twice = bt_pz_alg_create();
        bt_mega_invert_alg(alg, inverse);
        bt_mega_invert_alg(inverse, twice);
        BT_CHECK_EQ(bt_pz_alg_len(twice), bt_pz_alg_len(alg));
        for (size_t i = 0; i < bt_pz_alg_len(alg); i++) {
            bt_mega_move a = *(const bt_mega_move *)bt_pz_alg_at(&eng, alg, i);
            bt_mega_move b = *(const bt_mega_move *)bt_pz_alg_at(&eng, twice, i);
            BT_CHECK(bt_mega_move_eq(a, b));
        }
        bt_mega_state state;
        bt_mega_solved_state(&state);
        if (!bt_mega_apply_alg(&state, alg, &err)) {
            BT_ABORT("apply %s failed: %s", text, err.token);
        }
        size_t counts[BT_MEGA_FACES] = {0};
        uint8_t slots[BT_MEGA_PER_FACE];
        all_slots(slots);
        for (uint8_t face = 0; face < BT_MEGA_FACES; face++) {
            for (size_t i = 0; i < BT_MEGA_PER_FACE; i++) {
                counts[bt_mega_color(&state, face, slots[i])]++;
            }
        }
        for (size_t f = 0; f < BT_MEGA_FACES; f++) {
            BT_CHECK_EQ((long long)counts[f], 11);
        }
        if (!bt_mega_apply_alg(&state, inverse, &err) || !bt_mega_is_solved(&state)) {
            BT_ABORT("forward round trip failed for %s", text);
        }
        bt_mega_state reverse;
        bt_mega_solved_state(&reverse);
        if (!bt_mega_apply_alg(&reverse, inverse, &err) ||
            !bt_mega_apply_alg(&reverse, alg, &err) || !bt_mega_is_solved(&reverse)) {
            BT_ABORT("reverse round trip failed for %s", text);
        }
        bt_pz_alg_destroy(twice);
        bt_pz_alg_destroy(inverse);
        bt_strbuf_free(&formatted);
        bt_pz_alg_destroy(alg);
    }
}

static char *read_pack(void) {
    const char *root = getenv("BT_REF_GROUND");
    if (root == NULL || root[0] == '\0') {
        root = "ref_ground";
    }
    char path[512];
    snprintf(path, sizeof path, "%s/packs/megaminx_4lll.tsv", root);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        BT_ABORT("pack unreadable: %s", path);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL || fread(text, 1, (size_t)size, file) != (size_t)size) {
        BT_ABORT("pack unreadable: %s", path);
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

static size_t parse_rows(char *text, char keys[64][32], char algs[64][512]) {
    size_t rows = 0;
    char *cursor = text;
    bool first = true;
    while (*cursor != '\0') {
        char *line_end = strchr(cursor, '\n');
        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
        }
        char *line = cursor;
        bool last = *line_end == '\0';
        if (line_end > line && line_end[-1] == '\r') {
            line_end[-1] = '\0';
        }
        *line_end = '\0';
        cursor = last ? line_end : line_end + 1;
        if (line[0] == '\0') {
            continue;
        }
        if (first) {
            first = false;
            continue;
        }
        char *fields[8];
        size_t field_count = 0;
        char *field = line;
        for (char *scan = line;; scan++) {
            if (*scan == '\t' || *scan == '\0') {
                if (field_count < 8) {
                    fields[field_count] = field;
                }
                field_count++;
                bool done = *scan == '\0';
                *scan = '\0';
                if (done) {
                    break;
                }
                field = scan + 1;
            }
        }
        BT_CHECK(field_count == 5);
        snprintf(keys[rows], 32, "%s", fields[0]);
        size_t alg_at = 0;
        for (char *scan = fields[1]; *scan != '\0'; scan++) {
            if (*scan != '(' && *scan != ')' && alg_at + 1 < 512) {
                algs[rows][alg_at] = *scan;
                alg_at++;
            }
        }
        algs[rows][alg_at] = '\0';
        rows++;
    }
    return rows;
}

BT_TEST(mega_oracle, tsv_rows_all_pass_tier1_except_the_wave_b_row) {
    char *text = read_pack();
    char keys[64][32];
    char algs[64][512];
    size_t rows = parse_rows(text, keys, algs);
    free(text);
    BT_CHECK_EQ((long long)rows, 39);
    size_t failing_count = 0;
    char failing[64][32];
    for (size_t row = 0; row < rows; row++) {
        bt_mega_move move;
        char walker[512];
        snprintf(walker, sizeof walker, "%s", algs[row]);
        bool bad = false;
        char *save = NULL;
        for (char *token = split_space(walker, &save); token != NULL;
             token = split_space(NULL, &save)) {
            if (!bt_mega_parse_move(token, strlen(token), &move, NULL)) {
                bad = true;
            }
        }
        if (bad) {
            snprintf(failing[failing_count], 32, "%s", keys[row]);
            failing_count++;
        }
    }
    BT_CHECK_EQ((long long)failing_count, 1);
    BT_CHECK_EQ(failing[0], "4lll.33");
    for (size_t row = 0; row < rows; row++) {
        if (strcmp(keys[row], "4lll.33") == 0) {
            continue;
        }
        BT_CHECK(tier1_ok(algs[row]));
    }
}

BT_TEST(mega_oracle, tsv_row_33_bracket_tokens_are_pinned) {
    char *text = read_pack();
    char keys[64][32];
    char algs[64][512];
    size_t rows = parse_rows(text, keys, algs);
    free(text);
    bool found = false;
    char bad[8][32];
    size_t bad_count = 0;
    for (size_t row = 0; row < rows; row++) {
        if (strcmp(keys[row], "4lll.33") != 0) {
            continue;
        }
        found = true;
        bt_mega_move move;
        char walker[512];
        snprintf(walker, sizeof walker, "%s", algs[row]);
        char *save = NULL;
        for (char *token = split_space(walker, &save); token != NULL;
             token = split_space(NULL, &save)) {
            if (!bt_mega_parse_move(token, strlen(token), &move, NULL) && bad_count < 8) {
                snprintf(bad[bad_count], 32, "%s", token);
                bad_count++;
            }
        }
    }
    BT_CHECK(found);
    BT_CHECK_EQ((long long)bad_count, 3);
    BT_CHECK_EQ(bad[0], "[R");
    BT_CHECK_EQ(bad[1], "U]");
    BT_CHECK_EQ(bad[2], "*2");
}

BT_TEST(mega_oracle, facelet_grid_layout_pins_the_pentagon) {
    bt_mega_state solved;
    bt_mega_solved_state(&solved);
    bt_pz_grid grid;
    bt_mega_facelets(&solved, &grid);
    BT_CHECK_EQ((long long)grid.face_count, 12);
    const size_t widths[5] = {1, 2, 3, 3, 2};
    for (size_t face = 0; face < grid.face_count; face++) {
        BT_CHECK_EQ(grid.faces[face].row_count, 5u);
        for (size_t row = 0; row < grid.faces[face].row_count; row++) {
            BT_CHECK_EQ(grid.faces[face].rows[row].cols, widths[row]);
            for (size_t col = 0; col < grid.faces[face].rows[row].cols; col++) {
                BT_CHECK_EQ(grid.faces[face].rows[row].cells[col], (uint8_t)face);
            }
        }
    }
    bt_mega_state after;
    colors_after("R", &after);
    bt_pz_grid after_grid;
    bt_mega_facelets(&after, &after_grid);
    const bt_pz_face *top = &after_grid.faces[BT_MEGA_U];
    BT_CHECK_EQ(top->rows[0].cells[0], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[1].cells[0], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[1].cells[1], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[2].cells[0], (uint8_t)BT_MEGA_F);
    BT_CHECK_EQ(top->rows[2].cells[1], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[2].cells[2], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[3].cells[0], (uint8_t)BT_MEGA_F);
    BT_CHECK_EQ(top->rows[3].cells[1], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[3].cells[2], (uint8_t)BT_MEGA_U);
    BT_CHECK_EQ(top->rows[4].cells[0], (uint8_t)BT_MEGA_F);
    BT_CHECK_EQ(top->rows[4].cells[1], (uint8_t)BT_MEGA_U);
}

BT_TEST(mega_oracle, empty_alg_is_the_identity) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg("", 0, alg, &err)) {
        BT_ABORT("parse empty failed: %s", err.token);
    }
    BT_CHECK(bt_pz_alg_is_empty(alg));
    bt_strbuf text;
    bt_strbuf_init(&text, 8);
    bt_mega_format_alg(alg, &text);
    BT_CHECK_EQ(text.data, "");
    bt_strbuf_free(&text);
    bt_pz_alg_destroy(alg);
    BT_CHECK(tier1_ok(""));
}

BT_TEST(mega_oracle, move_type_reports_face_and_steps) {
    bt_mega_move move;
    BT_CHECK(bt_mega_parse_move("BR'", strlen("BR'"), &move, NULL));
    BT_CHECK(bt_mega_move_eq(move, (bt_mega_move){0, BT_MEGA_BR, 4}));
    BT_CHECK(bt_mega_parse_move("y'", strlen("y'"), &move, NULL));
    BT_CHECK(bt_mega_move_eq(move, (bt_mega_move){1, 0, 4}));
    BT_CHECK(bt_mega_parse_move("U2'", strlen("U2'"), &move, NULL));
    BT_CHECK(bt_mega_move_eq(move, (bt_mega_move){0, BT_MEGA_U, 3}));
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg("U2' y BR", strlen("U2' y BR"), alg, &err)) {
        BT_ABORT("parse failed: %s", err.token);
    }
    void *inverse = bt_pz_alg_create();
    bt_mega_invert_alg(alg, inverse);
    bt_strbuf text;
    bt_strbuf_init(&text, 32);
    bt_mega_format_alg(inverse, &text);
    BT_CHECK_EQ(text.data, "BR' y' U2");
    bt_strbuf_free(&text);
    bt_pz_alg_destroy(inverse);
    bt_pz_alg_destroy(alg);
}

BT_TEST(mega_oracle, only_pinned_moves_are_legal_constructions) {
    for (size_t i = 0; i < 16; i++) {
        bt_mega_move move;
        BT_CHECK(bt_mega_parse_move(PINNED_TOKENS[i], strlen(PINNED_TOKENS[i]), &move, NULL));
        BT_CHECK(bt_mega_is_pinned_move(move));
    }
    const bt_mega_move illegal[7] = {
        {0, BT_MEGA_D, 1}, {0, BT_MEGA_UL, 2}, {0, 12, 1}, {0, BT_MEGA_U, 0},
        {0, BT_MEGA_U, 5}, {1, 0, 0},          {1, 0, 5},
    };
    for (size_t i = 0; i < 7; i++) {
        BT_CHECK(!bt_mega_is_pinned_move(illegal[i]));
    }
}

BT_TEST(mega_oracle, quarter_one_direction_is_pinned_by_neighbor_colors) {
    bt_mega_state state;
    colors_after("U", &state);
    const uint8_t faces[4] = {1, 2, 3, 4};
    const uint8_t values[4] = {2, 3, 4, 5};
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_EQ(bt_mega_color(&state, faces[i], 1), values[i]);
        BT_CHECK_EQ(bt_mega_color(&state, faces[i], 5), values[i]);
    }
}

BT_TEST(mega_oracle, is_empty_reflects_the_parsed_move_count) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg("U", strlen("U"), alg, &err)) {
        BT_ABORT("parse U failed: %s", err.token);
    }
    BT_CHECK(!bt_pz_alg_is_empty(alg));
    bt_pz_alg_destroy(alg);
    alg = bt_pz_alg_create();
    if (!bt_mega_parse_alg("", 0, alg, &err)) {
        BT_ABORT("parse empty failed: %s", err.token);
    }
    BT_CHECK(bt_pz_alg_is_empty(alg));
    bt_pz_alg_destroy(alg);
}

BT_TEST(mega_oracle, quarter_one_own_ring_cycle_is_pinned_after_a_neighbor_turn) {
    bt_mega_state state;
    colors_after("F U", &state);
    const uint8_t slots[4] = {2, 5, 6, 10};
    const uint8_t values[4] = {5, 0, 5, 0};
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_EQ(bt_mega_color(&state, 0, slots[i]), values[i]);
    }
}
