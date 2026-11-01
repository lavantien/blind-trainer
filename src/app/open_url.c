#include "app/open_url.h"

#include "common/bt_trace.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

#include <stddef.h>
#include <string.h>

enum {
    BT_OPEN_URL_WIDE_MAX = 2048,
};

static int url_bytes_clean(const char *url) {
    for (size_t at = 0; url[at] != '\0'; at++) {
        unsigned char byte = (unsigned char)url[at];
        if (byte <= 0x20U || byte == 0x7FU) {
            return 0;
        }
    }
    return 1;
}

static int host_follows(const char *rest) {
    return rest[0] != '\0' && rest[0] != '/' && rest[0] != ':' && rest[0] != '?' && rest[0] != '#';
}

int bt_app_url_is_openable(const char *url) {
    BT_TRACE_ENTER("app.url_is_openable");
    int openable = 0;
    if (url != NULL && url[0] != '\0') {
        size_t scheme = 0;
        if (strncmp(url, "http://", 7) == 0) {
            scheme = 7;
        } else if (strncmp(url, "https://", 8) == 0) {
            scheme = 8;
        }
        openable = scheme != 0 && host_follows(url + scheme) && url_bytes_clean(url);
    }
    BT_TRACE_EXIT("app.url_is_openable");
    return openable;
}

#ifdef _WIN32
static int open_url_platform(const char *url) {
    BT_TRACE_ENTER("app.open_url_platform");
    int code = -1;
    int need = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
    if (need > 0 && (size_t)need <= BT_OPEN_URL_WIDE_MAX) {
        wchar_t wide[BT_OPEN_URL_WIDE_MAX];
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wide, need);
        HINSTANCE opened = ShellExecuteW(NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL);
        if ((intptr_t)opened > 32) {
            code = 0;
        } else {
            BT_TRACE_FAIL("app.open_url_platform", "shell execute returned %d",
                          (int)(intptr_t)opened);
        }
    } else {
        BT_TRACE_FAIL("app.open_url_platform", "cannot widen url, need %d", need);
    }
    BT_TRACE_EXIT("app.open_url_platform");
    return code;
}
#else
static int open_url_platform(const char *url) {
    BT_TRACE_ENTER("app.open_url_platform");
#ifdef __APPLE__
    const char *program = "open";
#else
    const char *program = "xdg-open";
#endif
    pid_t pid = 0;
    char *const argv[3] = {(char *)program, (char *)url, NULL};
    if (posix_spawn(&pid, program, NULL, NULL, argv, environ) != 0) {
        BT_TRACE_FAIL("app.open_url_platform", "posix_spawn %s failed", program);
        BT_TRACE_EXIT("app.open_url_platform");
        return -1;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        BT_TRACE_FAIL("app.open_url_platform", "%s did not exit cleanly", program);
        BT_TRACE_EXIT("app.open_url_platform");
        return -1;
    }
    BT_TRACE_EXIT("app.open_url_platform");
    return 0;
}
#endif

static int (*open_url_launch)(const char *) = open_url_platform;

void bt_app_open_url_bind(int (*launch)(const char *)) {
    BT_TRACE_ENTER("app.open_url_bind");
    open_url_launch = launch;
    BT_TRACE_EXIT("app.open_url_bind");
}

void bt_app_open_url_unbind(void) {
    BT_TRACE_ENTER("app.open_url_unbind");
    open_url_launch = open_url_platform;
    BT_TRACE_EXIT("app.open_url_unbind");
}

int bt_app_open_url(const char *url) {
    BT_TRACE_ENTER("app.open_url");
    if (!bt_app_url_is_openable(url)) {
        BT_TRACE_FAIL("app.open_url", "rejected url %s", url != NULL ? url : "(null)");
        BT_TRACE_EXIT("app.open_url");
        return -1;
    }
    int code = open_url_launch(url);
    if (code != 0) {
        code = -1;
    }
    BT_TRACE_EXIT("app.open_url");
    return code;
}
