#ifndef BT_SLEEPER_LIB_H
#define BT_SLEEPER_LIB_H

#include <stddef.h>

#define BT_SLEEPER_ERR_MAX 160

enum {
    BT_SLEEPER_PLAIN = 0,
    BT_SLEEPER_ALLOC_MB = 1,
    BT_SLEEPER_HOLD_SECS = 2,
};

int bt_sleeper_parse_args(int argc, char *const argv[], double *secs, char *err, size_t err_cap);
int bt_sleeper_parse_mode(int argc, char *const argv[], int *mode, unsigned long long *value,
                          char *err, size_t err_cap);

#endif
