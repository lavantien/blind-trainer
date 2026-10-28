#ifndef BT_RV_INFLATE_H
#define BT_RV_INFLATE_H

#include <stddef.h>

int bt_rv_inflate_raw(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len,
                      char *err, size_t err_cap);
int bt_rv_inflate_zlib(const unsigned char *in, size_t in_len, unsigned char **out, size_t *out_len,
                       char *err, size_t err_cap);

#endif
