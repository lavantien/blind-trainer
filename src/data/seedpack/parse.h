#ifndef BT_DATA_SEEDPACK_PARSE_H
#define BT_DATA_SEEDPACK_PARSE_H

#include "mod.h"

#define BT_SEEDPACK_NOTES_KEY "loader_notes"
#define BT_SEEDPACK_HEADER "case_key\talg\tfront_json\tsource_url\tsource_at"

int bt_seedpack_load_pack(const char *packs_root, const bt_pack_entry *entry, bt_seed_rows *out,
                          bt_seed_error *err);
int bt_seedpack_parse_pack(const bt_pack_entry *entry, const char *text, bt_seed_rows *out,
                           bt_seed_error *err);

#endif
