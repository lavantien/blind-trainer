#ifndef UNIT_TOOLS_GA_RUN_FIXTURE_UTIL_H
#define UNIT_TOOLS_GA_RUN_FIXTURE_UTIL_H

#include "common/bt_harness.h"
#include "common/strbuf.h"
#include "ga-run/lib.h"
#include "scheme/pairs.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>

static void ga_run_fixture_cases_text(bt_strbuf *text) {
    bt_strbuf_append_str(text, "{\"cases\": [");
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    for (size_t i = 0; i < edge_count + corner_count; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        char l1[2] = {bt_scheme_letter_of(case_id->t1), '\0'};
        char l2[2] = {bt_scheme_letter_of(case_id->t2), '\0'};
        bt_strbuf_appendf(text,
                          "%s{\"orbit\": \"%s\", \"t1\": %u, \"t2\": %u, "
                          "\"t1_letter\": \"%s\", \"t2_letter\": \"%s\"}",
                          i > 0 ? "," : "", case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner",
                          (unsigned)case_id->t1, (unsigned)case_id->t2, l1, l2);
    }
    bt_strbuf_append_str(text, "]}");
}

static void ga_run_fixture_vendor(const char *parent, char out[BT_PATH_MAX]) {
    snprintf(out, BT_PATH_MAX, "%s/vendor-fixture", parent);
    _mkdir(out);
    bt_strbuf text;
    bt_strbuf_init(&text, 65536);
    ga_run_fixture_cases_text(&text);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/cases.json", out);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(text.data, 1, text.len, file);
    fclose(file);
    bt_strbuf_free(&text);
}

#endif
