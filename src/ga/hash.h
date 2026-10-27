#ifndef BT_GA_HASH_H
#define BT_GA_HASH_H

#include <stddef.h>

#define BT_GA_SHA256_HEX_LEN 65

void bt_ga_sha256_hex(const void *data, size_t len, char out[BT_GA_SHA256_HEX_LEN]);

#endif
