#include "app/ui.h"

#include "common/bt_trace.h"

#include <string.h>

void bt_ui_input_clear(bt_ui_input *input) {
    memset(input, 0, sizeof *input);
}

int bt_ui_contains(const bt_ui_rect *rect, float x, float y) {
    return x >= rect->x && x < rect->x + rect->width && y >= rect->y && y < rect->y + rect->height;
}

bt_ui_button_state bt_ui_button_hit(const bt_ui_rect *rect, const bt_ui_input *input) {
    BT_TRACE_ENTER("app.ui_button_hit");
    bt_ui_button_state state = {0, 0, 0};
    state.hovered = bt_ui_contains(rect, input->x, input->y);
    state.clicked = state.hovered && input->click;
    state.pressed = state.hovered && input->down;
    BT_TRACE_EXIT("app.ui_button_hit");
    return state;
}

int bt_ui_list_scroll(bt_ui_list_window *window, int row_count, int visible_rows, int wheel) {
    BT_TRACE_ENTER("app.ui_list_scroll");
    int span = row_count - visible_rows;
    if (span < 0) {
        span = 0;
    }
    int first = window->first + wheel;
    if (first < 0) {
        first = 0;
    }
    if (first > span) {
        first = span;
    }
    int moved = first != window->first;
    window->first = first;
    BT_TRACE_EXIT("app.ui_list_scroll");
    return moved;
}

void bt_ui_list_show(bt_ui_list_window *window, int row_count, int visible_rows) {
    BT_TRACE_ENTER("app.ui_list_show");
    if (window->selected < window->first) {
        window->first = window->selected;
    }
    if (window->selected >= window->first + visible_rows) {
        window->first = window->selected - visible_rows + 1;
    }
    int span = row_count - visible_rows;
    if (span < 0) {
        span = 0;
    }
    if (window->first > span) {
        window->first = span;
    }
    if (window->first < 0) {
        window->first = 0;
    }
    BT_TRACE_EXIT("app.ui_list_show");
}

int bt_ui_field_edit(char *buffer, size_t cap, size_t *cursor, const bt_ui_input *input,
                     int *committed, int *canceled) {
    BT_TRACE_ENTER("app.ui_field_edit");
    *committed = 0;
    *canceled = 0;
    if (input->escape) {
        *canceled = 1;
        BT_TRACE_EXIT("app.ui_field_edit");
        return 1;
    }
    size_t len = strlen(buffer);
    if (input->backspace && *cursor > 0) {
        memmove(buffer + *cursor - 1, buffer + *cursor, len - *cursor + 1);
        *cursor -= 1;
        len -= 1;
    }
    for (int i = 0; i < input->char_count; i++) {
        unsigned int code = (unsigned int)input->chars[i];
        if (input->chars[i] < 0x20 || input->chars[i] > 0x10FFFF) {
            continue;
        }
        size_t width;
        if (code < 0x80) {
            width = 1;
        } else if (code < 0x800) {
            width = 2;
        } else if (code < 0x10000) {
            width = 3;
        } else {
            width = 4;
        }
        if (len + width >= cap) {
            break;
        }
        char encoded[4];
        unsigned int rest = code;
        if (width == 1) {
            encoded[0] = (char)code;
        } else {
            for (size_t at = width; at > 1; at--) {
                encoded[at - 1] = (char)(0x80U + rest % 64U);
                rest /= 64U;
            }
            unsigned int lead;
            if (width == 2) {
                lead = 0xC0U;
            } else if (width == 3) {
                lead = 0xE0U;
            } else {
                lead = 0xF0U;
            }
            encoded[0] = (char)(lead + rest);
        }
        memmove(buffer + *cursor + width, buffer + *cursor, len - *cursor + 1);
        memcpy(buffer + *cursor, encoded, width);
        *cursor += width;
        len += width;
    }
    if (input->enter) {
        *committed = 1;
    }
    BT_TRACE_EXIT("app.ui_field_edit");
    return 0;
}

int64_t bt_ui_number_clamp(int64_t value, int64_t low, int64_t high) {
    BT_TRACE_ENTER("app.ui_number_clamp");
    int64_t clamped = value;
    if (clamped < low) {
        clamped = low;
    }
    if (clamped > high) {
        clamped = high;
    }
    BT_TRACE_EXIT("app.ui_number_clamp");
    return clamped;
}
