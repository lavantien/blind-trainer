#ifndef BT_APP_DRIVER_KEYS_H
#define BT_APP_DRIVER_KEYS_H

#include <stddef.h>

#define BT_VK_RETURN 0x0Du
#define BT_VK_ESCAPE 0x1Bu
#define BT_VK_SPACE 0x20u
#define BT_VK_LEFT 0x25u
#define BT_VK_UP 0x26u
#define BT_VK_RIGHT 0x27u
#define BT_VK_DOWN 0x28u
#define BT_VK_OEM_2 0xBFu

int bt_app_driver_key_down(void *hwnd, unsigned int vk);
int bt_app_driver_key_up(void *hwnd, unsigned int vk);
int bt_app_driver_key_char(void *hwnd, unsigned int ch);

#endif
