#ifndef BT_DATA_SEEDPACK_REGISTRY_H
#define BT_DATA_SEEDPACK_REGISTRY_H

#include "mod.h"

int bt_seedpack_parse_registry(const char *text, bt_pack_entry **out_entries, size_t *out_count,
                               bt_seed_error *err);
void bt_seedpack_free_entries(bt_pack_entry *entries);
int bt_seedpack_load_registry(const char *packs_root, bt_pack_entry **out_entries,
                              size_t *out_count, bt_seed_error *err);

#endif
