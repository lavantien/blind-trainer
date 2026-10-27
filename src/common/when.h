#ifndef BT_WHEN_H
#define BT_WHEN_H

#include <stddef.h>
#include <stdint.h>

#define BT_WHEN_ISO8601_LEN 21

void bt_when_iso8601(uint64_t unix_secs, char *out, size_t out_len);
void bt_when_now(char *out, size_t out_len);

#endif
