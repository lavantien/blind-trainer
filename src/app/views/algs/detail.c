#include "app/views/algs/detail.h"

#include "app/views/drill.h"
#include "common/bt_trace.h"
#include "common/space.h"
#include "common/strbuf.h"
#include "core/alg.h"
#include "core/expand.h"
#include "data/db.h"
#include "data/notation.h"
#include "puzzles/fto.h"
#include "puzzles/mega.h"
#include "puzzles/nxn.h"
#include "puzzles/pyra.h"
#include "puzzles/skewb.h"
#include "puzzles/sq1.h"
#include "puzzles/tier1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bt_view_override_draft_default(bt_view_override_draft *draft) {
    BT_TRACE_ENTER("app.algs_draft_default");
    memset(draft, 0, sizeof *draft);
    BT_TRACE_EXIT("app.algs_draft_default");
}

bt_effective_alg bt_view_effective_alg(const bt_case_ref *case_ref, const char *seed_alg,
                                       const bt_overrides *store) {
    BT_TRACE_ENTER("app.algs_effective_alg");
    bt_effective_alg effective;
    const char *alg = bt_overrides_get(store, case_ref);
    if (alg != NULL) {
        snprintf(effective.alg, sizeof effective.alg, "%s", alg);
        effective.overridden = 1;
    } else {
        snprintf(effective.alg, sizeof effective.alg, "%s", seed_alg != NULL ? seed_alg : "");
        effective.overridden = 0;
    }
    BT_TRACE_EXIT("app.algs_effective_alg");
    return effective;
}

int bt_view_case_expand_line(const char *event, const char *alg, char out[BT_ALGS_TEXT_MAX]) {
    BT_TRACE_ENTER("app.algs_case_expand_line");
    int written = 0;
    if (strcmp(event, BT_DB_EVENT_3BLD) == 0 && bt_view_expand_flat_line(alg, out)) {
        written = 1;
    }
    BT_TRACE_EXIT("app.algs_case_expand_line");
    return written;
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

int bt_view_case_site_of(const bt_repo_browser *browser, bt_case_site *out) {
    BT_TRACE_ENTER("app.algs_case_site_of");
    memset(out, 0, sizeof *out);
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node == NULL || node->kind != BT_REPO_CASE || browser->arm >= browser->arm_count) {
        BT_TRACE_EXIT("app.algs_case_site_of");
        return 0;
    }
    const bt_repo_node *arm = &browser->arms[browser->arm];
    const char *method = NULL;
    const char *stage = NULL;
    for (size_t depth = 0; depth < browser->depth; depth++) {
        const bt_repo_node *walked = bt_repo_node_at(browser->arms, browser->arm_count,
                                                     browser->arm, browser->path, depth + 1);
        if (walked == NULL) {
            BT_TRACE_EXIT("app.algs_case_site_of");
            return 0;
        }
        if (walked->kind == BT_REPO_SIZE) {
            out->has_size = 1;
            out->size = size_label_value(walked->label);
        } else if (walked->kind == BT_REPO_METHOD) {
            method = walked->label;
        } else if (walked->kind == BT_REPO_STAGE) {
            stage = walked->label;
        }
    }
    if (method == NULL || stage == NULL) {
        BT_TRACE_EXIT("app.algs_case_site_of");
        return 0;
    }
    bt_case_ref_of(&out->case_ref, arm->label, method, stage, node->label);
    BT_TRACE_EXIT("app.algs_case_site_of");
    return 1;
}

static int nxn_check(size_t n, const char *text, char *error, size_t error_len) {
    bt_pz_nxn nxn;
    if (!bt_pz_nxn_init(&nxn, n)) {
        snprintf(error, error_len, "no tier 1 geometry for a %zux%zu cube", n, n);
        return -1;
    }
    bt_tier1_error err;
    bt_pz_engine engine = bt_pz_nxn_engine(&nxn);
    bt_tier1_status status = bt_verify_tier1(&engine, text, &err);
    if (status != BT_TIER1_OK) {
        bt_strbuf message;
        bt_strbuf_init(&message, 256);
        bt_tier1_error_message(&err, &message);
        snprintf(error, error_len, "%s", message.data);
        free(message.data);
        return -1;
    }
    return 0;
}

static int text_is_blank(const char *text) {
    size_t at = 0;
    size_t len = strlen(text);
    while (at < len) {
        size_t run = bt_space_len(text, at, len);
        if (run == 0) {
            return 0;
        }
        at += run;
    }
    return 1;
}

static int btcore_cube_check(const char *text, char *error, size_t error_len) {
    if (text_is_blank(text)) {
        snprintf(error, error_len, "alg is empty");
        return -1;
    }
    bt_alg expanded;
    bt_alg_init(&expanded);
    if (bt_expand(text, &expanded)) {
        bt_strbuf formatted;
        bt_strbuf_init(&formatted, 256);
        bt_alg_format(&expanded, &formatted);
        int verdict = nxn_check(3, formatted.data, error, error_len);
        free(formatted.data);
        bt_alg_free(&expanded);
        return verdict;
    }
    bt_alg_free(&expanded);
    bt_alg parsed;
    bt_alg_init(&parsed);
    char err_token[64];
    if (bt_alg_try_parse(text, &parsed)) {
        int verdict = nxn_check(3, text, error, error_len);
        bt_alg_free(&parsed);
        return verdict;
    }
    bt_alg_free(&parsed);
    (void)err_token;
    snprintf(error, error_len, "alg does not parse in the bt-core notation");
    return -1;
}

static int skewb_check(const char *text, char *error, size_t error_len) {
    char stripped[BT_ALGS_TEXT_MAX];
    bt_notation_strip_rotations(text, stripped, sizeof stripped);
    if (stripped[0] == '\0') {
        snprintf(error, error_len, "alg is empty after rotation stripping");
        return -1;
    }
    bt_skewb skewb = bt_skewb_new();
    bt_pz_engine engine = bt_skewb_engine(&skewb);
    bt_tier1_error err;
    if (bt_verify_tier1(&engine, stripped, &err) != BT_TIER1_OK) {
        bt_strbuf message;
        bt_strbuf_init(&message, 256);
        bt_tier1_error_message(&err, &message);
        snprintf(error, error_len, "%s", message.data);
        free(message.data);
        return -1;
    }
    return 0;
}

static int tier1_engine_check(bt_pz_engine engine, const char *text, char *error,
                              size_t error_len) {
    bt_tier1_error err;
    if (bt_verify_tier1(&engine, text, &err) != BT_TIER1_OK) {
        bt_strbuf message;
        bt_strbuf_init(&message, 256);
        bt_tier1_error_message(&err, &message);
        snprintf(error, error_len, "%s", message.data);
        free(message.data);
        return -1;
    }
    return 0;
}

int bt_view_geometry_check(const char *event, int has_size, long long size, const char *text,
                           char *error, size_t error_len) {
    BT_TRACE_ENTER("app.algs_geometry_check");
    int verdict = 0;
    if (strcmp(event, "2x2") == 0) {
        verdict = nxn_check(2, text, error, error_len);
    } else if (strcmp(event, "3x3") == 0 || strcmp(event, "oh") == 0 ||
               strcmp(event, "3bld") == 0) {
        verdict = btcore_cube_check(text, error, error_len);
    } else if (strcmp(event, "bigcube") == 0) {
        if (!has_size) {
            snprintf(error, error_len, "no cube size in the browser path for a bigcube case");
            verdict = -1;
        } else {
            long long clamped = size;
            if (clamped < 2) {
                clamped = 2;
            }
            if (clamped > 7) {
                clamped = 7;
            }
            verdict = nxn_check((size_t)clamped, text, error, error_len);
        }
    } else if (strcmp(event, "skewb") == 0) {
        verdict = skewb_check(text, error, error_len);
    } else if (strcmp(event, "pyraminx") == 0) {
        verdict = tier1_engine_check(bt_pyra_engine(), text, error, error_len);
    } else if (strcmp(event, "megaminx") == 0) {
        verdict = tier1_engine_check(bt_mega_engine(), text, error, error_len);
    } else if (strcmp(event, "fto") == 0) {
        bt_fto fto = bt_fto_new();
        verdict = tier1_engine_check(bt_fto_engine(&fto), text, error, error_len);
    } else if (strcmp(event, "sq1") == 0) {
        verdict = tier1_engine_check(bt_pz_sq1_engine(), text, error, error_len);
    } else {
        snprintf(error, error_len, "no tier 1 geometry for event %s", event);
        verdict = -1;
    }
    BT_TRACE_EXIT("app.algs_geometry_check");
    return verdict;
}

static void push_line(bt_detail_line *out, size_t cap, size_t *count, bt_detail_kind kind,
                      const char *text) {
    if (*count >= cap) {
        return;
    }
    out[*count].kind = kind;
    snprintf(out[*count].text, BT_ALGS_TEXT_MAX, "%s", text);
    (*count)++;
}

void bt_view_repo_detail_lines(const bt_repo_browser *browser, const bt_overrides *store,
                               const bt_view_override_draft *draft, bt_detail_line *out, size_t cap,
                               size_t *count) {
    BT_TRACE_ENTER("app.algs_repo_detail_lines");
    *count = 0;
    bt_case_site site;
    int has_site = bt_view_case_site_of(browser, &site);
    char crumb[BT_ALGS_TEXT_MAX];
    bt_repo_breadcrumb(browser, crumb, sizeof crumb);
    push_line(out, cap, count, BT_DETAIL_META, crumb);
    const bt_repo_node *node = bt_repo_browser_node(browser);
    if (node == NULL) {
        push_line(out, cap, count, BT_DETAIL_META, "no repository");
        BT_TRACE_EXIT("app.algs_repo_detail_lines");
        return;
    }
    push_line(out, cap, count, BT_DETAIL_TITLE, node->label);
    char text[BT_ALGS_TEXT_MAX];
    if (node->kind == BT_REPO_LETTERS) {
        push_line(out, cap, count, BT_DETAIL_META, "the 3bld pair browser");
    } else if (node->kind == BT_REPO_CASE && node->has_case) {
        if (!has_site) {
            push_line(out, cap, count, BT_DETAIL_ALG, node->case_alg);
            snprintf(text, sizeof text, "source %s %s", node->case_source_url,
                     node->case_source_at);
            push_line(out, cap, count, BT_DETAIL_META, text);
        } else {
            bt_effective_alg effective =
                bt_view_effective_alg(&site.case_ref, node->case_alg, store);
            push_line(out, cap, count, BT_DETAIL_ALG, effective.alg);
            push_line(out, cap, count, BT_DETAIL_META, effective.overridden ? "override" : "seed");
            char expand[BT_ALGS_TEXT_MAX];
            if (bt_view_case_expand_line(site.case_ref.event, effective.alg, expand)) {
                push_line(out, cap, count, BT_DETAIL_META, expand);
            }
            char error[BT_ALGS_TEXT_MAX];
            if (bt_view_geometry_check(site.case_ref.event, site.has_size, site.size, effective.alg,
                                       error, sizeof error) != 0) {
                push_line(out, cap, count, BT_DETAIL_WARN, error);
            }
            if (draft->has_case && bt_case_ref_cmp(&draft->site.case_ref, &site.case_ref) == 0) {
                push_line(out, cap, count, BT_DETAIL_META, "editing");
                push_line(out, cap, count, BT_DETAIL_ALG, draft->text);
            }
            snprintf(text, sizeof text, "source %s %s", node->case_source_url,
                     node->case_source_at);
            push_line(out, cap, count, BT_DETAIL_META, text);
        }
    } else if (node->kind == BT_REPO_METHOD) {
        snprintf(text, sizeof text, "stages %zu cases %zu", bt_repo_count_kind(node, BT_REPO_STAGE),
                 bt_repo_count_kind(node, BT_REPO_CASE));
        push_line(out, cap, count, BT_DETAIL_META, text);
    } else if (node->kind == BT_REPO_STAGE) {
        snprintf(text, sizeof text, "cases %zu", bt_repo_count_kind(node, BT_REPO_CASE));
        push_line(out, cap, count, BT_DETAIL_META, text);
    } else {
        snprintf(text, sizeof text, "methods %zu stages %zu cases %zu",
                 bt_repo_count_kind(node, BT_REPO_METHOD), bt_repo_count_kind(node, BT_REPO_STAGE),
                 bt_repo_count_kind(node, BT_REPO_CASE));
        push_line(out, cap, count, BT_DETAIL_META, text);
    }
    if (draft->has_error) {
        push_line(out, cap, count, BT_DETAIL_WARN, draft->error);
    }
    BT_TRACE_EXIT("app.algs_repo_detail_lines");
}
