#ifndef BT_GA_ALLOC_H
#define BT_GA_ALLOC_H

#include <stddef.h>

typedef void *(*bt_ga_alloc_hook)(size_t bytes, const char *site, void *user);
typedef void (*bt_ga_free_hook)(void *block, void *user);

void bt_ga_alloc_install(bt_ga_alloc_hook alloc, bt_ga_free_hook release, void *user);
void bt_ga_alloc_reset(void);
void *bt_ga_alloc(size_t bytes, const char *site);
void bt_ga_free(void *block);

#endif
