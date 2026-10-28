#ifndef BT_RV_PNG_H
#define BT_RV_PNG_H

#include <stddef.h>

int bt_rv_png_check(const char *path, unsigned int *width, unsigned int *height, char *err,
                    size_t err_cap);
int bt_rv_png_raw(const char *path, unsigned int *width, unsigned int *height,
                  unsigned char **raw, size_t *raw_len, char *err, size_t err_cap);

#endif
