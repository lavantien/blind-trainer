#ifndef BT_APP_VIEWS_DRILL_H
#define BT_APP_VIEWS_DRILL_H

#include "app/logic.h"
#include "common/rng.h"
#include "core/alg.h"
#include "core/cube.h"
#include "data/corpus.h"
#include "data/fsrs.h"
#include "data/srs.h"
#include "scheme/pairs.h"

#include <stddef.h>
#include <stdint.h>

#define BT_DRILL_TEXT_MAX 256
#define BT_DRILL_LABEL_MAX 32
#define BT_DRILL_SPLITS_MAX 128

typedef struct {
    bt_case case_id;
    char position[BT_DRILL_TEXT_MAX];
    bt_alg alg;
    char alg_text[BT_DRILL_TEXT_MAX];
    char commutator[BT_DRILL_TEXT_MAX];
    char solver_label[BT_DRILL_LABEL_MAX];
    int verified;
    int vendor;
} bt_drill_side;

typedef struct {
    int is_a;
    float secs;
} bt_drill_split;

typedef struct {
    int has_side_a;
    int has_side_b;
    bt_drill_side side_a;
    bt_drill_side side_b;
    int has_pin;
    bt_case pin;
    unsigned int reps_done;
    unsigned int reps_target;
    int reps_side_a;
    float reps_clock;
    float last_split_at;
    bt_drill_split splits[BT_DRILL_SPLITS_MAX];
    size_t split_count;
    unsigned char rate_step;
    bt_cube prepared;
} bt_drill_session;

void bt_drill_side_init(bt_drill_side *side);
void bt_drill_side_free(bt_drill_side *side);
void bt_drill_side_letters(const bt_drill_side *side, char out[3]);

void bt_drill_session_init(bt_drill_session *session);
void bt_drill_session_free(bt_drill_session *session);
const bt_drill_side *bt_drill_session_sides(const bt_drill_session *session,
                                            const bt_drill_side **side_b);
const bt_drill_side *bt_drill_session_current_side(const bt_drill_session *session);

bt_sheet_class bt_view_sheet_of(bt_orbit orbit);
unsigned char bt_view_buffer_of(bt_orbit orbit);
void bt_view_case_position(const bt_case *case_id, char out[BT_DRILL_TEXT_MAX]);
bt_case bt_view_flip_case(const bt_case *case_id);
void bt_view_card_key(bt_sheet_class sheet, const char *position, char out[BT_DRILL_TEXT_MAX]);
unsigned int bt_view_even_reps(unsigned int reps);
int bt_view_has_commutator(const char *text);
int bt_view_expand_flat_line(const char *alg_text, char out[BT_DRILL_TEXT_MAX]);
int bt_view_expand_alg_line(const char *commutator, const bt_alg *alg, const char *alg_text,
                            const char *displayed, char out[BT_DRILL_TEXT_MAX]);

void bt_view_install_pair(bt_drill_session *session, const bt_corpus *corpus,
                          const bt_stores *stores, bt_case case_id);
int bt_view_pair_is_due(const bt_stores *stores, long long now_day, const bt_case *case_id);
int bt_view_pick_pair(const bt_corpus *corpus, const bt_stores *stores, long long now_day,
                      bt_rng *rng, bt_case *out);
void bt_view_write_review(bt_stores *stores, const bt_fsrs_config *fsrs, bt_sheet_class sheet,
                          const char *position, bt_grade rating, long long now_day);
int bt_view_rating_of(int key, bt_grade *out);

void bt_view_drill_net_set(bt_app_net_state *net, bt_cube cube, const bt_drill_side *side);
void bt_view_drill_net_clear(bt_app_net_state *net);
void bt_view_drill_net_sync(bt_app_net_state *net, const bt_drill_session *session);

#endif
