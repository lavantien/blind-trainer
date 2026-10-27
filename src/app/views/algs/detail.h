#ifndef BT_APP_VIEWS_ALGS_DETAIL_H
#define BT_APP_VIEWS_ALGS_DETAIL_H

#include "app/views/algs/browse.h"
#include "data/repo_user.h"

#include <stddef.h>

#define BT_DETAIL_LINES_MAX 32

typedef enum {
    BT_DETAIL_TITLE,
    BT_DETAIL_ALG,
    BT_DETAIL_META,
    BT_DETAIL_WARN,
} bt_detail_kind;

typedef struct {
    bt_detail_kind kind;
    char text[BT_ALGS_TEXT_MAX];
} bt_detail_line;

typedef struct {
    bt_case_ref case_ref;
    int has_size;
    long long size;
} bt_case_site;

typedef struct {
    int has_case;
    bt_case_site site;
    char text[BT_ALGS_TEXT_MAX];
    int has_error;
    char error[BT_ALGS_TEXT_MAX];
} bt_view_override_draft;

typedef struct {
    char alg[BT_ALGS_TEXT_MAX];
    int overridden;
} bt_effective_alg;

void bt_view_override_draft_default(bt_view_override_draft *draft);
bt_effective_alg bt_view_effective_alg(const bt_case_ref *case_ref, const char *seed_alg,
                                       const bt_overrides *store);
int bt_view_case_expand_line(const char *event, const char *alg, char out[BT_ALGS_TEXT_MAX]);
int bt_view_case_site_of(const bt_repo_browser *browser, bt_case_site *out);
int bt_view_geometry_check(const char *event, int has_size, long long size, const char *text,
                           char *error, size_t error_len);
void bt_view_repo_detail_lines(const bt_repo_browser *browser, const bt_overrides *store,
                               const bt_view_override_draft *draft, bt_detail_line *out, size_t cap,
                               size_t *count);

bt_overrides *bt_view_algs_overrides(void);
bt_view_override_draft *bt_view_algs_draft(void);
void bt_view_algs_override_enter(bt_app_ctx *ctx);

#endif
