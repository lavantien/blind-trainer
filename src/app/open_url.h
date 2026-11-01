#ifndef BT_APP_OPEN_URL_H
#define BT_APP_OPEN_URL_H

int bt_app_open_url(const char *url);
int bt_app_url_is_openable(const char *url);
void bt_app_open_url_bind(int (*launch)(const char *));
void bt_app_open_url_unbind(void);

#endif
