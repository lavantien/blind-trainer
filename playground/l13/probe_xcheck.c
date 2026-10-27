#include "ga-run/lib.h"
#include "common/bt_harness.h"
#include "scheme/pairs.h"
#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *temp = getenv("TEMP");
    char root[BT_PATH_MAX];
    snprintf(root, sizeof root, "%s/bt-gr-probe-%d", temp, _getpid());
    _mkdir(root);
    bt_strbuf text;
    bt_strbuf_init(&text, 1 << 12);
    bt_strbuf_append_str(&text, "{\"cases\": [");
    bt_case edge[BT_PAIRS_MAX];
    bt_case corner[BT_PAIRS_MAX];
    size_t edge_count = bt_scheme_enumerate_cases(BT_ORBIT_EDGE, edge);
    size_t corner_count = bt_scheme_enumerate_cases(BT_ORBIT_CORNER, corner);
    for (size_t i = 0; i < edge_count + corner_count; i++) {
        const bt_case *case_id = i < edge_count ? &edge[i] : &corner[i - edge_count];
        char letters[3];
        letters[0] = bt_scheme_letter_of(case_id->t1);
        letters[1] = bt_scheme_letter_of(case_id->t2);
        letters[2] = '\0';
        char l1[2] = {letters[0], '\0'};
        char l2[2] = {letters[1], '\0'};
        bt_strbuf_appendf(&text, "%s{\"orbit\": \"%s\", \"t1\": %u, \"t2\": %u", i > 0 ? "," : "",
                          case_id->orbit == BT_ORBIT_EDGE ? "edge" : "corner",
                          (unsigned)case_id->t1, (unsigned)case_id->t2);
        bt_strbuf_appendf(&text, ", \"t1_letter\": \"%s\"", i == 0 ? "ax" : l1);
        bt_strbuf_appendf(&text, ", \"t2_letter\": \"%s\"", i == 0 ? "b" : l2);
        bt_strbuf_append_str(&text, "}");
    }
    bt_strbuf_append_str(&text, "]}");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/cases.json", root);
    FILE *file = fopen(path, "wb");
    fwrite(text.data, 1, text.len, file);
    fclose(file);
    bt_strbuf_free(&text);
    fprintf(stderr, "fixture written %zu bytes\n", (size_t)0);
    char clean[BT_PATH_MAX];
    snprintf(clean, sizeof clean, "%s", root);
    for (char *cursor = clean; *cursor != '\0'; cursor++) {
        if (*cursor == 92) {
            *cursor = '/';
        }
    }
    char cfg_text[BT_PATH_MAX + 64];
    snprintf(cfg_text, sizeof cfg_text, "{\"paths\": {\"data_vendor\": \"%s\"}}", clean);
    bt_json_value *config = NULL;
    bt_json_error error;
    int prc = bt_json_parse(cfg_text, strlen(cfg_text), &config, &error);
    fprintf(stderr, "config parse rc=%d\n", prc);
    bt_gr_offenders offenders = {0};
    int rc = bt_gr_cross_check_cases(config, &offenders);
    fprintf(stderr, "cross_check rc=%d count=%zu\n", rc, offenders.count);
    for (size_t i = 0; i < offenders.count; i++) {
        fprintf(stderr, "offender: %s\n", offenders.items[i]);
    }
    return 0;
}
