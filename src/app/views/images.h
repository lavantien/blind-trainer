#ifndef BT_APP_VIEWS_IMAGES_H
#define BT_APP_VIEWS_IMAGES_H

#include <stddef.h>

#define BT_VIEW_IMAGES_COUNT 620

typedef struct {
    char key[3];
    const char *word;
} bt_view_image_entry;

extern const bt_view_image_entry BT_VIEW_IMAGES[BT_VIEW_IMAGES_COUNT];

const char *bt_view_images_for(const char letters[2]);
void bt_view_images_install(void);

#endif
