#include "app/views/case_drill.h"

#include "app/views/drill.h"

#include "common/bt_trace.h"
#include "data/db.h"
#include "data/notation.h"
#include "data/paths.h"
#include "puzzles/fto.h"
#include "puzzles/mega.h"
#include "puzzles/nxn.h"
#include "puzzles/pyra.h"
#include "puzzles/skewb.h"
#include "puzzles/sq1.h"

#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const CUBE_FACES[6] = {"U", "L", "F", "R", "B", "D"};
static const char *const SKEWB_FACES[6] = {"U", "F", "R", "L", "B", "D"};
static const char *const FTO_FACES[8] = {"U", "F", "R", "BR", "B", "BL", "L", "D"};
static const char *const FTO_DISPLAY[8] = {"U", "L", "F", "R", "B", "BR", "BL", "D"};
static const char *const PYRA_FACES[4] = {"F", "L", "R", "D"};
static const char *const MEGA_FACES[12] = {
    "U", "F", "R", "BR", "BL", "L", "D", "B", "UR", "DR", "DL", "UL",
};
static const char *const SQ1_FACES[2] = {"U", "D"};
static const char *const FACE_COLOR_NAMES[12] = {
    "U", "L", "F", "R", "B", "D", "BR", "BL", "UR", "DR", "DL", "UL",
};

typedef struct {
    const char *const *faces;
    size_t count;
} face_list;

static face_list engine_faces(const char *event) {
    face_list list = {NULL, 0};
    if (strcmp(event, "2x2") == 0 || strcmp(event, "3x3") == 0 || strcmp(event, "oh") == 0 ||
        strcmp(event, "bigcube") == 0) {
        list.faces = CUBE_FACES;
        list.count = 6;
    } else if (strcmp(event, "skewb") == 0) {
        list.faces = SKEWB_FACES;
        list.count = 6;
    } else if (strcmp(event, "fto") == 0) {
        list.faces = FTO_FACES;
        list.count = 8;
    } else if (strcmp(event, "pyraminx") == 0) {
        list.faces = PYRA_FACES;
        list.count = 4;
    } else if (strcmp(event, "megaminx") == 0) {
        list.faces = MEGA_FACES;
        list.count = 12;
    } else if (strcmp(event, "sq1") == 0) {
        list.faces = SQ1_FACES;
        list.count = 2;
    }
    return list;
}

static face_list display_faces(const char *event) {
    if (strcmp(event, "skewb") == 0) {
        face_list list = {CUBE_FACES, 6};
        return list;
    }
    if (strcmp(event, "fto") == 0) {
        face_list list = {FTO_DISPLAY, 8};
        return list;
    }
    return engine_faces(event);
}

size_t bt_case_drill_face_color_index(const char *face) {
    BT_TRACE_ENTER("app.case_drill_face_color_index");
    size_t index = 0;
    for (size_t i = 0; i < 12; i++) {
        if (strcmp(FACE_COLOR_NAMES[i], face) == 0) {
            index = i;
            break;
        }
    }
    BT_TRACE_EXIT("app.case_drill_face_color_index");
    return index;
}

size_t bt_case_drill_face_permutation(const char *event, size_t out[BT_PZ_MAX_FACES]) {
    BT_TRACE_ENTER("app.case_drill_face_permutation");
    face_list engine = engine_faces(event);
    face_list display = display_faces(event);
    if (engine.count == 0 || display.count == 0) {
        BT_TRACE_EXIT("app.case_drill_face_permutation");
        return 0;
    }
    for (size_t i = 0; i < display.count; i++) {
        size_t at = engine.count;
        for (size_t candidate = 0; candidate < engine.count; candidate++) {
            if (strcmp(engine.faces[candidate], display.faces[i]) == 0) {
                at = candidate;
                break;
            }
        }
        if (at == engine.count) {
            BT_TRACE_EXIT("app.case_drill_face_permutation");
            return 0;
        }
        out[i] = at;
    }
    BT_TRACE_EXIT("app.case_drill_face_permutation");
    return display.count;
}

size_t bt_case_drill_color_remap(const char *event, size_t out[16]) {
    BT_TRACE_ENTER("app.case_drill_color_remap");
    if (strcmp(event, "sq1") == 0) {
        for (size_t i = 0; i < 8; i++) {
            out[i] = 0;
            out[8 + i] = 5;
        }
        BT_TRACE_EXIT("app.case_drill_color_remap");
        return 16;
    }
    face_list engine = engine_faces(event);
    if (engine.count == 0) {
        BT_TRACE_EXIT("app.case_drill_color_remap");
        return 0;
    }
    for (size_t i = 0; i < engine.count; i++) {
        out[i] = bt_case_drill_face_color_index(engine.faces[i]);
    }
    BT_TRACE_EXIT("app.case_drill_color_remap");
    return engine.count;
}

int bt_case_drill_themed_grid(const bt_pz_grid *grid, const char *event, bt_pz_grid *out) {
    BT_TRACE_ENTER("app.case_drill_themed_grid");
    size_t remap[16];
    size_t remap_count = bt_case_drill_color_remap(event, remap);
    size_t order[BT_PZ_MAX_FACES];
    size_t order_count = bt_case_drill_face_permutation(event, order);
    if (remap_count == 0 || order_count == 0) {
        BT_TRACE_EXIT("app.case_drill_themed_grid");
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->face_count = order_count;
    for (size_t slot = 0; slot < order_count; slot++) {
        size_t engine_index = order[slot];
        if (engine_index >= grid->face_count) {
            BT_TRACE_EXIT("app.case_drill_themed_grid");
            return 0;
        }
        out->faces[slot].row_count = grid->faces[engine_index].row_count;
        for (size_t row = 0; row < grid->faces[engine_index].row_count; row++) {
            size_t cols = grid->faces[engine_index].rows[row].cols;
            out->faces[slot].rows[row].cols = cols;
            for (size_t col = 0; col < cols; col++) {
                unsigned char color = grid->faces[engine_index].rows[row].cells[col];
                out->faces[slot].rows[row].cells[col] =
                    color < remap_count ? (unsigned char)remap[color] : color;
            }
        }
    }
    BT_TRACE_EXIT("app.case_drill_themed_grid");
    return 1;
}

static int geometry_grid(bt_pz_engine engine, const char *text, const char *event, bt_pz_grid *out,
                         char *error, size_t error_len) {
    bt_pz_error err;
    void *alg = bt_pz_alg_create();
    if (!engine.parse_alg(&engine, text, strlen(text), alg, &err)) {
        bt_strbuf message;
        bt_strbuf_init(&message, 256);
        bt_pz_error_message(&err, &message);
        snprintf(error, error_len, "%s", message.data);
        free(message.data);
        bt_pz_alg_destroy(alg);
        return 0;
    }
    void *inverse = bt_pz_alg_create();
    engine.invert_alg(&engine, alg, inverse);
    bt_pz_alg_destroy(alg);
    char *state = malloc(engine.state_size);
    if (state == NULL) {
        bt_pz_alg_destroy(inverse);
        snprintf(error, error_len, "%s", "out of memory");
        return 0;
    }
    engine.solved_state(&engine, state);
    if (!engine.apply_alg(&engine, state, inverse, &err)) {
        bt_strbuf message;
        bt_strbuf_init(&message, 256);
        bt_pz_error_message(&err, &message);
        snprintf(error, error_len, "%s", message.data);
        free(message.data);
        free(state);
        bt_pz_alg_destroy(inverse);
        return 0;
    }
    bt_pz_alg_destroy(inverse);
    bt_pz_grid raw;
    engine.facelets(&engine, state, &raw);
    free(state);
    if (!bt_case_drill_themed_grid(&raw, event, out)) {
        snprintf(error, error_len, "no face layout for event %s", event);
        return 0;
    }
    return 1;
}

static int nxn_grid(size_t n, const char *text, bt_pz_grid *out, char *error, size_t error_len) {
    bt_pz_nxn nxn;
    if (!bt_pz_nxn_init(&nxn, n)) {
        snprintf(error, error_len, "no case geometry for a %zux%zu cube", n, n);
        return 0;
    }
    return geometry_grid(bt_pz_nxn_engine(&nxn), text, "bigcube", out, error, error_len);
}

int bt_case_drill_facelets(const char *event, int has_size, long long size, const char *alg,
                           bt_pz_grid *out, char *error, size_t error_len) {
    BT_TRACE_ENTER("app.case_drill_facelets");
    int rendered = 0;
    if (strcmp(event, "2x2") == 0) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, 2);
        rendered = geometry_grid(bt_pz_nxn_engine(&nxn), alg, event, out, error, error_len);
    } else if (strcmp(event, "3x3") == 0 || strcmp(event, "oh") == 0) {
        bt_pz_nxn nxn;
        bt_pz_nxn_init(&nxn, 3);
        rendered = geometry_grid(bt_pz_nxn_engine(&nxn), alg, event, out, error, error_len);
    } else if (strcmp(event, "bigcube") == 0) {
        long long n = has_size ? size : 7;
        if (n < 2) {
            n = 2;
        }
        if (n > 7) {
            n = 7;
        }
        rendered = nxn_grid((size_t)n, alg, out, error, error_len);
    } else if (strcmp(event, "skewb") == 0) {
        char stripped[BT_CASE_DRILL_TEXT_MAX];
        bt_notation_strip_rotations(alg, stripped, sizeof stripped);
        if (stripped[0] == '\0') {
            snprintf(error, error_len, "%s", "alg is empty after rotation stripping");
        } else {
            bt_skewb skewb = bt_skewb_new();
            rendered =
                geometry_grid(bt_skewb_engine(&skewb), stripped, event, out, error, error_len);
        }
    } else if (strcmp(event, "pyraminx") == 0) {
        rendered = geometry_grid(bt_pyra_engine(), alg, event, out, error, error_len);
    } else if (strcmp(event, "megaminx") == 0) {
        rendered = geometry_grid(bt_mega_engine(), alg, event, out, error, error_len);
    } else if (strcmp(event, "fto") == 0) {
        bt_fto fto = bt_fto_new();
        rendered = geometry_grid(bt_fto_engine(&fto), alg, event, out, error, error_len);
    } else if (strcmp(event, "sq1") == 0) {
        rendered = geometry_grid(bt_pz_sq1_engine(), alg, event, out, error, error_len);
    } else {
        snprintf(error, error_len, "no case geometry for event %s", event);
    }
    BT_TRACE_EXIT("app.case_drill_facelets");
    return rendered;
}

static long long size_label_value(const char *label) {
    char digits[BT_REPO_LABEL_MAX];
    size_t at = 0;
    while (label[at] != '\0' && label[at] != 'x' && at + 1 < sizeof digits) {
        digits[at] = label[at];
        at++;
    }
    digits[at] = '\0';
    return strtoll(digits, NULL, 10);
}

static size_t collect_node(const bt_repo_node *node, const char *event, const char *method,
                           const char *stage, int has_size, long long size,
                           const bt_overrides *store, bt_drill_case *out, size_t cap) {
    size_t count = 0;
    if (node->kind == BT_REPO_LETTERS) {
        return 0;
    }
    if (node->kind == BT_REPO_EVENT || node->kind == BT_REPO_METHOD ||
        node->kind == BT_REPO_STAGE) {
        const char *node_method = node->kind == BT_REPO_METHOD ? node->label : method;
        const char *node_stage = node->kind == BT_REPO_STAGE ? node->label : stage;
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, node_method, node_stage, has_size,
                                  size, store, out + count, cap - count);
        }
        return count;
    }
    if (node->kind == BT_REPO_SIZE) {
        for (size_t i = 0; i < node->children_count; i++) {
            count += collect_node(&node->children[i], event, method, stage, 1,
                                  size_label_value(node->label), store, out + count, cap - count);
        }
        return count;
    }
    if (method != NULL && stage != NULL && node->has_case) {
        if (cap == 0) {
            return 0;
        }
        bt_case_ref_of(&out->case_ref, event, method, stage, node->label);
        out->has_size = has_size;
        out->size = size;
        snprintf(out->seed_alg, BT_CASE_DRILL_TEXT_MAX, "%s", node->case_alg);
        bt_effective_alg effective = bt_view_effective_alg(&out->case_ref, node->case_alg, store);
        snprintf(out->alg, BT_CASE_DRILL_TEXT_MAX, "%s", effective.alg);
        return 1;
    }
    return 0;
}

size_t bt_case_drill_collect_cases(const bt_repo_browser *browser, const bt_overrides *store,
                                   bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX]) {
    BT_TRACE_ENTER("app.case_drill_collect_cases");
    if (browser->arm >= browser->arm_count) {
        BT_TRACE_EXIT("app.case_drill_collect_cases");
        return 0;
    }
    const bt_repo_node *arm = &browser->arms[browser->arm];
    if (arm->kind == BT_REPO_LETTERS || strcmp(arm->label, BT_DB_EVENT_3BLD) == 0) {
        BT_TRACE_EXIT("app.case_drill_collect_cases");
        return 0;
    }
    const bt_repo_node *node = arm;
    const char *method = NULL;
    const char *stage = NULL;
    int has_size = 0;
    long long size = 0;
    for (size_t step = 0; step < browser->depth; step++) {
        node = &node->children[browser->path[step]];
        if (node->kind == BT_REPO_SIZE) {
            has_size = 1;
            size = size_label_value(node->label);
        } else if (node->kind == BT_REPO_METHOD) {
            method = node->label;
        } else if (node->kind == BT_REPO_STAGE) {
            stage = node->label;
        }
    }
    size_t count = collect_node(node, arm->label, method, stage, has_size, size, store, out,
                                BT_CASE_DRILL_QUEUE_MAX);
    BT_TRACE_EXIT("app.case_drill_collect_cases");
    return count;
}

size_t bt_case_drill_due_session(const bt_drill_case *cases, size_t count,
                                 const bt_case_cards *cards, long long now_day,
                                 bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX]) {
    BT_TRACE_ENTER("app.case_drill_due_session");
    bt_drill_case due[BT_CASE_DRILL_QUEUE_MAX];
    bt_drill_case seen[BT_CASE_DRILL_QUEUE_MAX];
    size_t due_count = 0;
    size_t seen_count = 0;
    for (size_t i = 0; i < count; i++) {
        const bt_card *card = bt_case_cards_get(cards, &cases[i].case_ref);
        if (card != NULL && !bt_card_is_due(card, now_day)) {
            seen[seen_count++] = cases[i];
        } else {
            due[due_count++] = cases[i];
        }
    }
    const bt_drill_case *source = due;
    size_t source_count = due_count;
    if (due_count == 0) {
        source = seen;
        source_count = seen_count;
    }
    memcpy(out, source, source_count * sizeof *source);
    BT_TRACE_EXIT("app.case_drill_due_session");
    return source_count;
}

int bt_case_drill_request(const bt_repo_browser *browser, const bt_overrides *store,
                          const bt_case_card_store *cards, long long now_day,
                          bt_drill_case out[BT_CASE_DRILL_QUEUE_MAX], size_t *out_count) {
    BT_TRACE_ENTER("app.case_drill_request");
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node == NULL) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    size_t all = bt_case_drill_collect_cases(browser, store, out);
    if (all == 0) {
        BT_TRACE_EXIT("app.case_drill_request");
        return 0;
    }
    if (node->kind == BT_REPO_CASE) {
        *out_count = all;
        BT_TRACE_EXIT("app.case_drill_request");
        return 1;
    }
    *out_count = bt_case_drill_due_session(out, all, &cards->cards, now_day, out);
    BT_TRACE_EXIT("app.case_drill_request");
    return 1;
}

void bt_case_drill_install(bt_case_drill_session *session, const bt_drill_case *queue,
                           size_t count) {
    BT_TRACE_ENTER("app.case_drill_install");
    session->total = count;
    session->done = 0;
    if (count > BT_CASE_DRILL_QUEUE_MAX) {
        count = BT_CASE_DRILL_QUEUE_MAX;
    }
    memcpy(session->queue.items, queue, count * sizeof *queue);
    session->queue.count = count;
    session->has_save_error = 0;
    session->save_error[0] = '\0';
    bt_case_drill_advance(session);
    BT_TRACE_EXIT("app.case_drill_install");
}

void bt_case_drill_advance(bt_case_drill_session *session) {
    BT_TRACE_ENTER("app.case_drill_advance");
    session->has_current = session->queue.count > 0;
    if (session->has_current) {
        session->current = session->queue.items[0];
        memmove(&session->queue.items[0], &session->queue.items[1],
                (session->queue.count - 1) * sizeof session->queue.items[0]);
        session->queue.count--;
    }
    session->has_grid = 0;
    session->has_grid_error = 0;
    session->grid_error[0] = '\0';
    if (session->has_current) {
        if (bt_case_drill_facelets(session->current.case_ref.event, session->current.has_size,
                                   session->current.size, session->current.alg, &session->grid,
                                   session->grid_error, sizeof session->grid_error)) {
            session->has_grid = 1;
        } else {
            session->has_grid_error = 1;
        }
    }
    BT_TRACE_EXIT("app.case_drill_advance");
}

void bt_case_drill_case_site_line(const bt_drill_case *case_drill,
                                  char out[BT_CASE_DRILL_TEXT_MAX]) {
    BT_TRACE_ENTER("app.case_drill_case_site_line");
    if (case_drill->has_size) {
        snprintf(out, BT_CASE_DRILL_TEXT_MAX, "%s > %s > %s %lldx%lld", case_drill->case_ref.event,
                 case_drill->case_ref.method, case_drill->case_ref.stage, case_drill->size,
                 case_drill->size);
    } else {
        snprintf(out, BT_CASE_DRILL_TEXT_MAX, "%s > %s > %s", case_drill->case_ref.event,
                 case_drill->case_ref.method, case_drill->case_ref.stage);
    }
    BT_TRACE_EXIT("app.case_drill_case_site_line");
}

static bt_view_case_drill_app_state *shared_case_drill_app;

void bt_view_case_drill_app_init(bt_view_case_drill_app_state *app) {
    BT_TRACE_ENTER("app.case_drill_app_init");
    memset(app, 0, sizeof *app);
    bt_case_cards_init(&app->cards.cards);
    app->initialized = 1;
    BT_TRACE_EXIT("app.case_drill_app_init");
}

bt_view_case_drill_app_state *bt_view_case_drill_app(void) {
    BT_TRACE_ENTER("app.case_drill_app_case_drill");
    if (shared_case_drill_app == NULL) {
        shared_case_drill_app = malloc(sizeof *shared_case_drill_app);
        if (shared_case_drill_app == NULL) {
            BT_TRACE_FAIL("app.case_drill_app_case_drill", "out of memory");
            BT_TRACE_EXIT("app.case_drill_app_case_drill");
            return NULL;
        }
        bt_view_case_drill_app_init(shared_case_drill_app);
    }
    BT_TRACE_EXIT("app.case_drill_app_case_drill");
    return shared_case_drill_app;
}

void bt_view_case_drill_set_pending(const bt_drill_case *queue, size_t count) {
    BT_TRACE_ENTER("app.case_drill_set_pending");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    if (app == NULL || count > BT_CASE_DRILL_QUEUE_MAX) {
        BT_TRACE_EXIT("app.case_drill_set_pending");
        return;
    }
    memcpy(app->pending.items, queue, count * sizeof *queue);
    app->pending.count = count;
    BT_TRACE_EXIT("app.case_drill_set_pending");
}

void bt_view_case_drill_cards_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.case_drill_cards_enter");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    bt_case_cards_free(&app->cards.cards);
    bt_case_cards_init(&app->cards.cards);
    app->cards.has_load_error = 0;
    app->cards.load_error[0] = '\0';
    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx->paths.user_root, db_path);
    if (bt_db_open_at(db_path, &db, &err) != 0) {
        app->cards.has_load_error = 1;
        snprintf(app->cards.load_error, sizeof app->cards.load_error, "%s", err.detail);
        BT_TRACE_FAIL("app.case_drill_cards_enter", "%s", err.detail);
        BT_TRACE_EXIT("app.case_drill_cards_enter");
        return;
    }
    bt_srs_load_error load_err;
    if (bt_repo_cards_load(&db, &ctx->fsrs, &app->cards.cards, &load_err) != 0) {
        bt_srs_load_error_string(&load_err, app->cards.load_error, sizeof app->cards.load_error);
        app->cards.has_load_error = 1;
    }
    bt_db_close(&db);
    BT_TRACE_EXIT("app.case_drill_cards_enter");
}

void bt_view_case_drill_enter_state(bt_app_ctx *ctx, bt_view_case_drill_app_state *app) {
    BT_TRACE_ENTER("app.case_drill_enter_state");
    if (app->pending.count == 0) {
        bt_app_request_state(ctx, BT_APP_ALGS);
        bt_app_apply_pending(ctx);
        BT_TRACE_EXIT("app.case_drill_enter_state");
        return;
    }
    bt_case_drill_install(&app->session, app->pending.items, app->pending.count);
    app->pending.count = 0;
    if (app->cards.has_load_error) {
        app->session.has_save_error = 1;
        snprintf(app->session.save_error, sizeof app->session.save_error, "%s",
                 app->cards.load_error);
    }
    ctx->case_drill_phase = BT_CASE_DRILL_SHOW;
    BT_TRACE_EXIT("app.case_drill_enter_state");
}

void bt_view_case_drill_key(bt_app_ctx *ctx, bt_view_case_drill_app_state *app, int key) {
    BT_TRACE_ENTER("app.case_drill_key");
    if (ctx->case_drill_phase == BT_CASE_DRILL_SHOW) {
        if (!app->session.has_current) {
            BT_TRACE_EXIT("app.case_drill_key");
            return;
        }
        if (key == KEY_SPACE) {
            ctx->case_drill_phase = BT_CASE_DRILL_GRADE;
        }
        BT_TRACE_EXIT("app.case_drill_key");
        return;
    }
    if (!app->session.has_current) {
        BT_TRACE_EXIT("app.case_drill_key");
        return;
    }
    bt_grade rating;
    if (!bt_view_rating_of(key, &rating)) {
        BT_TRACE_EXIT("app.case_drill_key");
        return;
    }
    if (app->cards.has_load_error) {
        app->session.has_save_error = 1;
        snprintf(app->session.save_error, sizeof app->session.save_error, "%s",
                 app->cards.load_error);
        BT_TRACE_EXIT("app.case_drill_key");
        return;
    }
    long long day = bt_views_now_day();
    bt_card fresh;
    bt_card_init(&fresh, day);
    const bt_card *card =
        bt_case_cards_entry(&app->cards.cards, &app->session.current.case_ref, &fresh);
    bt_card_free(&fresh);
    if (card == NULL) {
        BT_TRACE_FAIL("app.case_drill_key", "card entry failed");
        BT_TRACE_EXIT("app.case_drill_key");
        return;
    }
    bt_card_review((bt_card *)card, rating, day, &ctx->fsrs);
    bt_db db;
    bt_db_error err;
    char db_path[BT_PATH_MAX];
    bt_data_db_path_at(ctx->paths.user_root, db_path);
    if (bt_db_open_at(db_path, &db, &err) != 0 ||
        bt_repo_cards_save_card(&db, &app->session.current.case_ref, card, &err) != 0) {
        app->session.has_save_error = 1;
        snprintf(app->session.save_error, sizeof app->session.save_error, "%s", err.detail);
    }
    if (db.conn != NULL) {
        bt_db_close(&db);
    }
    app->session.done++;
    bt_case_drill_advance(&app->session);
    if (app->session.has_current) {
        ctx->case_drill_phase = BT_CASE_DRILL_SHOW;
    } else {
        bt_app_request_state(ctx, BT_APP_ALGS);
        bt_app_apply_pending(ctx);
    }
    BT_TRACE_EXIT("app.case_drill_key");
}

void bt_view_case_drill_enter(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.case_drill_enter");
    bt_view_case_drill_enter_state(ctx, bt_view_case_drill_app());
    BT_TRACE_EXIT("app.case_drill_enter");
}

void bt_view_case_drill_update(bt_app_ctx *ctx) {
    BT_TRACE_ENTER("app.case_drill_update");
    bt_view_case_drill_app_state *app = bt_view_case_drill_app();
    int keys[5] = {KEY_SPACE, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};
    for (int i = 0; i < 5; i++) {
        if (IsKeyPressed(keys[i])) {
            bt_view_case_drill_key(ctx, app, keys[i]);
        }
    }
    BT_TRACE_EXIT("app.case_drill_update");
}
