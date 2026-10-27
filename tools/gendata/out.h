#ifndef GD_OUT_H
#define GD_OUT_H

#include <stddef.h>

int bt_gd_write_atomic(const char *path, const void *bytes, size_t len, char *err, size_t err_len);

#endif
