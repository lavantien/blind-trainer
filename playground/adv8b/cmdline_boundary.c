#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#define CAP 64

static int quote_argument(const wchar_t *raw, wchar_t *out, size_t cap) {
    size_t used = 0;
    int needs = raw[0] == L'\0';
    if (needs == 0) {
        for (const wchar_t *cursor = raw; *cursor != L'\0'; cursor++) {
            if (iswspace(*cursor) != 0 || *cursor == L'"') {
                needs = 1;
                break;
            }
        }
    }
    if (needs == 0) {
        if (wcslen(raw) + 1 > cap) {
            return -1;
        }
        wmemcpy(out, raw, wcslen(raw) + 1);
        return 0;
    }
    if (cap < 3) {
        return -1;
    }
    out[used++] = L'"';
    size_t backslashes = 0;
    for (const wchar_t *cursor = raw; *cursor != L'\0'; cursor++) {
        if (*cursor == L'\\') {
            backslashes++;
            continue;
        }
        size_t emit = *cursor == L'"' ? backslashes * 2 + 1 : backslashes;
        if (used + emit + 2 >= cap) {
            return -1;
        }
        for (size_t at = 0; at < emit; at++) {
            out[used++] = L'\\';
        }
        backslashes = 0;
        out[used++] = *cursor;
    }
    if (used + backslashes * 2 + 2 >= cap) {
        return -1;
    }
    for (size_t at = 0; at < backslashes * 2; at++) {
        out[used++] = L'\\';
    }
    out[used++] = L'"';
    out[used] = L'\0';
    return 0;
}

static int command_line(const wchar_t *const payload[], size_t count, wchar_t *out, size_t cap) {
    size_t used = 0;
    out[0] = L'\0';
    for (size_t at = 0; at < count; at++) {
        wchar_t quoted[CAP * 2];
        if (quote_argument(payload[at], quoted, sizeof quoted / sizeof quoted[0]) != 0) {
            return -1;
        }
        size_t len = wcslen(quoted);
        if (used + len + 1 > cap) {
            return -1;
        }
        if (at > 0) {
            out[used++] = L' ';
        }
        wmemcpy(out + used, quoted, len);
        used += len;
        out[used] = L'\0';
    }
    return 0;
}

int main(void) {
    wchar_t buf[CAP + 4];
    for (size_t at = 0; at < CAP + 4; at++) {
        buf[at] = 0xAAAA;
    }
    static wchar_t second[CAP];
    wmemset(second, L'b', 62);
    second[62] = L'\0';
    const wchar_t *payload[2] = {L"a", second};
    int rc = command_line(payload, 2, buf, CAP);
    size_t len = wcslen(buf);
    wprintf(L"rc=%d produced_len=%zu cap=%d\n", rc, len, CAP);
    for (size_t at = CAP; at < CAP + 4; at++) {
        wprintf(L"guard[%zu]=%x%s\n", at, (unsigned)buf[at],
                buf[at] == 0xAAAA ? L"" : L" CLOBBERED");
    }
    for (size_t at = CAP; at < CAP + 4; at++) {
        if (buf[at] != 0xAAAA) {
            return 1;
        }
    }
    return 0;
}
