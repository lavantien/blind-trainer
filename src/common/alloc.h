#ifndef BT_ALLOC_H
#define BT_ALLOC_H

#include <stddef.h>

void *bt_malloc(size_t size);
void *bt_calloc(size_t count, size_t size);
void *bt_realloc(void *ptr, size_t size);
void bt_alloc_fail_arm(size_t ok_count);
void bt_alloc_fail_disarm(void);
size_t bt_alloc_fail_count(void);

#endif
