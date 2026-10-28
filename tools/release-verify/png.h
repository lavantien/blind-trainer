#ifndef BT_RV_PNG_H
#define BT_RV_PNG_H

#include <stddef.h>

int bt_rv_png_check(const char *path, unsigned int *width, unsigned int *height, char *err,
                    size_t err_cap);

#endif
