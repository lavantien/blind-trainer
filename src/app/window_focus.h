#ifndef BT_APP_WINDOW_FOCUS_H
#define BT_APP_WINDOW_FOCUS_H

int bt_app_focus_no_activate_install(void *hwnd);
int bt_app_focus_no_activate_restore(void);
int bt_app_focus_style_no_activate(const void *hwnd);
void *bt_app_focus_foreground_window(void);
int bt_app_focus_cursor_pos(long *x, long *y);
int bt_app_focus_client_to_screen(const void *hwnd, long x, long y, long *screen_x, long *screen_y);
int bt_app_focus_sentinel_acquire(void);
void bt_app_focus_sentinel_release(void);
void *bt_app_focus_watch_start(const char *title, unsigned long pid);
unsigned long bt_app_focus_current_pid(void);
long bt_app_focus_watch_app_foreground_count(const void *watch);
long bt_app_focus_watch_cursor_jump_count(const void *watch);
void bt_app_focus_watch_stop(void *watch);

#endif
