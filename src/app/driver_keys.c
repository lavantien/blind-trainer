#include "app/driver_keys.h"

#include "common/bt_trace.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int bt_app_driver_press_vk(unsigned int vk) {
    BT_TRACE_ENTER("app.driver_press_vk");
    INPUT events[2];
    memset(events, 0, sizeof events);
    events[0].type = INPUT_KEYBOARD;
    events[0].ki.wVk = (WORD)vk;
    events[1].type = INPUT_KEYBOARD;
    events[1].ki.wVk = (WORD)vk;
    events[1].ki.dwFlags = KEYEVENTF_KEYUP;
    UINT sent = SendInput(2, events, sizeof(INPUT));
    BT_TRACE_EXIT("app.driver_press_vk");
    return sent == 2;
}

int bt_app_driver_press_unicode(unsigned int ch) {
    BT_TRACE_ENTER("app.driver_press_unicode");
    INPUT events[2];
    memset(events, 0, sizeof events);
    events[0].type = INPUT_KEYBOARD;
    events[0].ki.wScan = (WORD)ch;
    events[0].ki.dwFlags = KEYEVENTF_UNICODE;
    events[1].type = INPUT_KEYBOARD;
    events[1].ki.wScan = (WORD)ch;
    events[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    UINT sent = SendInput(2, events, sizeof(INPUT));
    BT_TRACE_EXIT("app.driver_press_unicode");
    return sent == 2;
}
