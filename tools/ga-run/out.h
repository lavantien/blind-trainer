#ifndef BT_GR_OUT_H
#define BT_GR_OUT_H

#include "common/json.h"
#include "common/strbuf.h"
#include "ga-run/lib.h"

#include <stddef.h>
#include <stdint.h>

int bt_gr_display_f64(bt_strbuf *out, double value);

int bt_gr_json_value_text(const bt_json_value *value, bt_strbuf *out);
int bt_gr_cases_text_compact(const bt_gr_syllable_case *cases, size_t count, bt_strbuf *out);
int bt_gr_assignment_text(const bt_gr_syllable_assignment *assignment, bt_strbuf *out);
int bt_gr_report_text(const bt_gr_ga_report *report, bt_strbuf *out);

#endif
