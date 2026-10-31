#include "app/ui.h"

#include "common/bt_trace.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

static void ui_select_clamp_cursor(bt_ui_select_state *state, int row_count, int selected) {
    if (selected < 0 || selected >= row_count) {
        selected = 0;
    }
    if (state->cursor < 0 || state->cursor >= row_count) {
        state->cursor = selected;
    }
}

static int ui_select_opens_upward(const bt_ui_rect *rect, int row_count, float max_y) {
    float down_bottom = rect->y + rect->height * (float)(row_count + 1);
    float up_top = rect->y - rect->height * (float)row_count;
    return down_bottom > max_y && up_top >= 0.0F;
}

bt_ui_rect bt_ui_select_slot(const bt_ui_rect *rect, int row_count, int index, float max_y) {
    BT_TRACE_ENTER("app.ui_select_slot");
    int upward = ui_select_opens_upward(rect, row_count, max_y);
    float y = upward ? rect->y - rect->height * (float)(row_count - index)
                     : rect->y + rect->height * (float)(index + 1);
    bt_ui_rect slot = {rect->x, y, rect->width, rect->height};
    BT_TRACE_EXIT("app.ui_select_slot");
    return slot;
}

int bt_ui_select_transition(bt_ui_select_state *state, const bt_ui_rect *rect, int row_count,
                            int selected, const bt_ui_input *input, int *edited, float max_y) {
    BT_TRACE_ENTER("app.ui_select_transition");
    *edited = 0;
    if (row_count <= 0) {
        state->open = 0;
        BT_TRACE_FAIL("app.ui_select_transition", "no rows to select from");
        BT_TRACE_EXIT("app.ui_select_transition");
        return selected;
    }
    if (selected < 0 || selected >= row_count) {
        selected = 0;
    }
    ui_select_clamp_cursor(state, row_count, selected);
    if (!state->open) {
        if (input->click && bt_ui_contains(rect, input->x, input->y)) {
            state->open = 1;
            state->cursor = selected;
        }
        BT_TRACE_EXIT("app.ui_select_transition");
        return selected;
    }
    if (input->escape) {
        state->open = 0;
        BT_TRACE_EXIT("app.ui_select_transition");
        return selected;
    }
    if (input->arrow_up && state->cursor > 0) {
        state->cursor -= 1;
    }
    if (input->arrow_down && state->cursor < row_count - 1) {
        state->cursor += 1;
    }
    for (int i = 0; i < input->char_count; i++) {
        int c = input->chars[i];
        if (c >= '1' && c <= '9' && c - '1' < row_count) {
            state->cursor = c - '1';
        }
    }
    if (input->enter) {
        state->open = 0;
        int picked = state->cursor;
        if (picked != selected) {
            *edited = 1;
        }
        BT_TRACE_EXIT("app.ui_select_transition");
        return picked;
    }
    if (input->click) {
        if (bt_ui_contains(rect, input->x, input->y)) {
            state->open = 0;
            BT_TRACE_EXIT("app.ui_select_transition");
            return selected;
        }
        for (int i = 0; i < row_count; i++) {
            bt_ui_rect slot = bt_ui_select_slot(rect, row_count, i, max_y);
            if (bt_ui_contains(&slot, input->x, input->y)) {
                state->open = 0;
                if (i != selected) {
                    *edited = 1;
                }
                BT_TRACE_EXIT("app.ui_select_transition");
                return i;
            }
        }
        state->open = 0;
        BT_TRACE_EXIT("app.ui_select_transition");
        return selected;
    }
    BT_TRACE_EXIT("app.ui_select_transition");
    return selected;
}

#include "app/theme.h"
#include "app/window.h"

#include "raylib.h"

static void ui_fill(const bt_ui_rect *rect, bt_theme_color color) {
    DrawRectangle((int)rect->x, (int)rect->y, (int)rect->width, (int)rect->height,
                  bt_app_color(color));
}

static void ui_lines(const bt_ui_rect *rect, bt_theme_color color) {
    DrawRectangleLines((int)rect->x, (int)rect->y, (int)rect->width, (int)rect->height,
                       bt_app_color(color));
}

static void ui_text(const struct Font *font, const char *text, float x, float y, float size,
                    bt_theme_color color) {
    DrawTextEx(*(const Font *)font, text, (Vector2){x, y}, size, 1.0F, bt_app_color(color));
}

static Vector2 ui_measure(const struct Font *font, const char *text, float size) {
    return MeasureTextEx(*(const Font *)font, text, size, 1.0F);
}

static float ui_center_offset(float total, float used) {
    return (total - used) / 2.0F;
}

bt_ui_button_state bt_ui_button(const struct Font *font, const bt_ui_rect *rect, const char *label,
                                const bt_ui_input *input) {
    BT_TRACE_ENTER("app.ui_button");
    bt_ui_button_state state = bt_ui_button_hit(rect, input);
    bt_theme_color fill = state.hovered ? bt_theme_panel_raised() : bt_theme_panel();
    if (state.pressed) {
        fill = bt_theme_mix(fill, bt_theme_accent(), 0.25F);
    }
    ui_fill(rect, fill);
    ui_lines(rect, bt_theme_hairline());
    float size = bt_theme_table()->font_body * bt_theme_table()->root_rem;
    Vector2 measured = ui_measure(font, label, size);
    ui_text(font, label, rect->x + ui_center_offset(rect->width, measured.x),
            rect->y + ui_center_offset(rect->height, measured.y), size, bt_theme_text_primary());
    BT_TRACE_EXIT("app.ui_button");
    return state;
}

bt_ui_rect bt_ui_panel(const struct Font *font, const bt_ui_rect *rect, const char *title) {
    BT_TRACE_ENTER("app.ui_panel");
    ui_fill(rect, bt_theme_panel());
    ui_lines(rect, bt_theme_hairline());
    const bt_theme *theme = bt_theme_table();
    float pad = theme->panel_padding_rem * theme->root_rem;
    float size = theme->font_title * theme->root_rem;
    ui_text(font, title, rect->x + pad, rect->y + pad, size, bt_theme_text_primary());
    bt_ui_rect content = {
        rect->x + pad,
        rect->y + pad + size,
        rect->width - pad * 2.0F,
        rect->height - pad * 2.0F - size,
    };
    BT_TRACE_EXIT("app.ui_panel");
    return content;
}

bt_ui_list_window bt_ui_list(const struct Font *font, const bt_ui_rect *rect,
                             const char *const *rows, int row_count, int visible_rows,
                             bt_ui_list_window window, const bt_ui_input *input) {
    BT_TRACE_ENTER("app.ui_list");
    ui_fill(rect, bt_theme_panel());
    ui_lines(rect, bt_theme_hairline());
    bt_ui_list_scroll(&window, row_count, visible_rows, input->wheel);
    const bt_theme *theme = bt_theme_table();
    float size = theme->font_body * theme->root_rem;
    float row_height = rect->height / (float)visible_rows;
    for (int at = 0; at < visible_rows; at++) {
        int row = window.first + at;
        if (row >= row_count) {
            break;
        }
        bt_ui_rect slot = {rect->x, rect->y + row_height * (float)at, rect->width, row_height};
        int hovered = bt_ui_contains(&slot, input->x, input->y);
        if (row == window.selected) {
            ui_fill(&slot, bt_theme_accent());
        } else if (hovered) {
            ui_fill(&slot, bt_theme_panel_raised());
        }
        ui_text(font, rows[row], slot.x + theme->spacing_rem * theme->root_rem,
                slot.y + ui_center_offset(row_height, size), size, bt_theme_text_primary());
        if (hovered && input->click) {
            window.selected = row;
            bt_ui_list_show(&window, row_count, visible_rows);
        }
    }
    BT_TRACE_EXIT("app.ui_list");
    return window;
}

int bt_ui_text_field(const struct Font *font, const bt_ui_rect *rect, char *buffer, size_t cap,
                     size_t *cursor, int focused, const bt_ui_input *input, int *committed) {
    BT_TRACE_ENTER("app.ui_text_field");
    int clicked = bt_ui_contains(rect, input->x, input->y) ? input->click : 0;
    int active = focused || clicked;
    ui_fill(rect, active ? bt_theme_panel_raised() : bt_theme_panel());
    ui_lines(rect, active ? bt_theme_accent() : bt_theme_hairline());
    const bt_theme *theme = bt_theme_table();
    float size = theme->font_body * theme->root_rem;
    ui_text(font, buffer, rect->x + theme->spacing_rem * theme->root_rem,
            rect->y + ui_center_offset(rect->height, size), size, bt_theme_text_primary());
    int canceled = 0;
    if (active) {
        bt_ui_field_edit(buffer, cap, cursor, input, committed, &canceled);
        if (canceled) {
            active = 0;
        }
    } else {
        *committed = 0;
    }
    BT_TRACE_EXIT("app.ui_text_field");
    return active;
}

static int number_text_apply(const char *text, int64_t low, int64_t high, int64_t *out) {
    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (end == text || errno == ERANGE) {
        return 0;
    }
    *out = bt_ui_number_clamp((int64_t)parsed, low, high);
    return 1;
}

int64_t bt_ui_number_field(const struct Font *font, const bt_ui_rect *rect, int64_t value,
                           int64_t low, int64_t high, bt_ui_number_state *state,
                           const bt_ui_input *input, int *edited) {
    BT_TRACE_ENTER("app.ui_number_field");
    *edited = 0;
    value = bt_ui_number_clamp(value, low, high);
    float step_width = rect->height;
    bt_ui_rect minus = {rect->x, rect->y, step_width, rect->height};
    bt_ui_rect plus = {rect->x + rect->width - step_width, rect->y, step_width, rect->height};
    int in_minus = bt_ui_contains(&minus, input->x, input->y);
    int in_plus = bt_ui_contains(&plus, input->x, input->y);
    int inside = bt_ui_contains(rect, input->x, input->y);
    if (input->click && (in_minus || in_plus)) {
        state->focused = 0;
        value = bt_ui_number_clamp(value + (in_plus ? 1 : -1), low, high);
        *edited = 1;
    } else if (input->click && inside && !state->focused) {
        state->focused = 1;
        state->selected = 1;
        snprintf(state->text, BT_UI_NUMBER_TEXT, "%lld", (long long)value);
    } else if (input->click && !inside && state->focused) {
        state->focused = 0;
    }
    if (state->focused) {
        size_t len = strlen(state->text);
        for (int i = 0; i < input->char_count; i++) {
            int c = input->chars[i];
            if (c >= '0' && c <= '9') {
                if (state->selected) {
                    len = 0;
                    state->selected = 0;
                }
                if (len + 1 < BT_UI_NUMBER_TEXT) {
                    state->text[len] = (char)c;
                    len++;
                    state->text[len] = '\0';
                }
            } else if (c == '-') {
                if (state->selected) {
                    len = 0;
                    state->selected = 0;
                }
                if (len == 0 && len + 1 < BT_UI_NUMBER_TEXT) {
                    state->text[len] = '-';
                    len++;
                    state->text[len] = '\0';
                }
            } else if (c == '\b' || c == 127) {
                if (state->selected) {
                    len = 0;
                    state->selected = 0;
                } else if (len > 0) {
                    len--;
                    state->text[len] = '\0';
                }
            }
        }
        if (input->backspace && !state->selected && len > 0) {
            len--;
            state->text[len] = '\0';
        }
        if (input->enter || input->escape) {
            state->focused = 0;
        }
        int64_t parsed_value = value;
        if (number_text_apply(state->text, low, high, &parsed_value) && parsed_value != value) {
            value = parsed_value;
            *edited = 1;
        }
    }
    ui_fill(rect, state->focused ? bt_theme_panel_raised() : bt_theme_panel());
    ui_lines(rect, state->focused ? bt_theme_accent() : bt_theme_hairline());
    const bt_theme *theme = bt_theme_table();
    float size = theme->font_body * theme->root_rem;
    bt_ui_button_state minus_hit = bt_ui_button_hit(&minus, input);
    bt_ui_button_state plus_hit = bt_ui_button_hit(&plus, input);
    char display[BT_UI_NUMBER_TEXT];
    if (state->focused) {
        snprintf(display, sizeof display, "%s", state->text);
    } else {
        snprintf(display, sizeof display, "%lld", (long long)value);
    }
    ui_fill(&minus, minus_hit.hovered ? bt_theme_panel_raised() : bt_theme_background());
    ui_fill(&plus, plus_hit.hovered ? bt_theme_panel_raised() : bt_theme_background());
    ui_text(font, "-", minus.x + ui_center_offset(step_width, 0),
            minus.y + ui_center_offset(rect->height, size), size, bt_theme_text_primary());
    ui_text(font, "+", plus.x + ui_center_offset(step_width, 0),
            plus.y + ui_center_offset(rect->height, size), size, bt_theme_text_primary());
    Vector2 measured = ui_measure(font, display, size);
    ui_text(font, display, rect->x + ui_center_offset(rect->width, measured.x),
            rect->y + ui_center_offset(rect->height, size), size, bt_theme_text_primary());
    BT_TRACE_EXIT("app.ui_number_field");
    return value;
}

int bt_ui_select(const struct Font *font, const bt_ui_rect *rect, const char *const *rows,
                 int row_count, int selected, bt_ui_select_state *state, const bt_ui_input *input,
                 int *edited, float max_y) {
    BT_TRACE_ENTER("app.ui_select");
    int picked = bt_ui_select_transition(state, rect, row_count, selected, input, edited, max_y);
    if (picked < 0 || picked >= row_count) {
        picked = 0;
    }
    ui_fill(rect, state->open ? bt_theme_panel_raised() : bt_theme_panel());
    ui_lines(rect, state->open ? bt_theme_accent() : bt_theme_hairline());
    const bt_theme *theme = bt_theme_table();
    float size = theme->font_body * theme->root_rem;
    float mark_w = rect->height;
    ui_text(font, rows[picked], rect->x + theme->spacing_rem * theme->root_rem,
            rect->y + ui_center_offset(rect->height, size), size, bt_theme_text_primary());
    ui_text(font, "v", rect->x + rect->width - mark_w + ui_center_offset(mark_w, 0.0F),
            rect->y + ui_center_offset(rect->height, size), size, bt_theme_text_muted());
    if (state->open) {
        for (int i = 0; i < row_count; i++) {
            bt_ui_rect slot = bt_ui_select_slot(rect, row_count, i, max_y);
            if (i == state->cursor) {
                ui_fill(&slot, bt_theme_panel_raised());
            } else {
                ui_fill(&slot, bt_theme_panel());
            }
            ui_lines(&slot, bt_theme_hairline());
            bt_theme_color ink = i == picked ? bt_theme_accent() : bt_theme_text_primary();
            ui_text(font, rows[i], slot.x + theme->spacing_rem * theme->root_rem,
                    slot.y + ui_center_offset(slot.height, size), size, ink);
        }
    }
    BT_TRACE_EXIT("app.ui_select");
    return picked;
}
