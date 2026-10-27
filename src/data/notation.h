#ifndef BT_DATA_NOTATION_H
#define BT_DATA_NOTATION_H

#include <stddef.h>

int bt_notation_is_rotation(const char *token);
int bt_notation_strip_rotations(const char *alg, char *out, size_t out_len);

#endif
