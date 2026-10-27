#ifndef VERIFY_PACKS_REPORT_H
#define VERIFY_PACKS_REPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "common/strbuf.h"
#include "data/seedpack/mod.h"
#include "data/seedpack/parse.h"
#include "verify/packs.h"

typedef struct {
    char event[BT_SEED_FIELD_MAX];
    char method[BT_SEED_FIELD_MAX];
    char stage[BT_SEED_FIELD_MAX];
    char file[BT_SEED_FILE_MAX];
    size_t rows;
    size_t passed;
    size_t failed;
    size_t skipped;
    size_t excused;
    size_t ruled;
    char honesty[VERIFY_HONESTY_CAP][VERIFY_HONESTY_MAX];
    size_t honesty_count;
} verify_pack_section_summary;

typedef struct {
    verify_strlist drift;
    verify_strlist offenders;
    verify_pack_section_summary *sections;
    size_t section_count;
    size_t section_cap;
} verify_pack_audit;

int verify_row_offender(const char *event, const char *method, const char *stage,
                        const verify_row_outcome *row, char *out, size_t out_len);
void verify_summarize(const bt_pack_entry *entry, const verify_section_result *section,
                      verify_pack_section_summary *summary, verify_strlist *offenders);
int verify_audit_packs(const char *packs_root, verify_pack_audit *out);
void verify_pack_audit_free(verify_pack_audit *audit);
void verify_render_packs_markdown(const verify_pack_audit *audit, bt_strbuf *out);

#endif
