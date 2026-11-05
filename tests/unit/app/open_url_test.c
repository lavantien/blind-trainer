#include "app/open_url.h"

#include "common.h"

enum {
    BT_OPEN_URL_LAST_MAX = 256,
};

static int g_open_calls;
static int g_open_result;
static char g_open_last[BT_OPEN_URL_LAST_MAX];

static int stub_launch(const char *url) {
    g_open_calls++;
    snprintf(g_open_last, sizeof g_open_last, "%s", url);
    return g_open_result;
}

static void stub_reset(int result) {
    g_open_calls = 0;
    g_open_last[0] = '\0';
    g_open_result = result;
}

BT_TEST(open_url, validation_accepts_http_and_https_with_a_host) {
    static const char *const accepted[] = {
        "http://example.com",
        "https://example.com",
        "https://sub.domain.example.co/recon?tab=2#frag",
        "http://localhost:8080/drill",
        "https://example.com:443/recon/x",
    };
    for (size_t at = 0; at < sizeof accepted / sizeof accepted[0]; at++) {
        BT_CHECK_EQ(bt_app_url_is_openable(accepted[at]), 1);
    }
}

BT_TEST(open_url, validation_rejects_empty_foreign_and_malformed_urls) {
    static const char *const rejected[] = {
        NULL,
        "",
        "example.com",
        "ftp://example.com",
        "file:///C:/Users/x",
        "javascript:alert(1)",
        "mailto:someone@example.com",
        "http://",
        "https://",
        "http:///path",
        "https://?q=1",
        "http://:8080/path",
        " http://example.com",
        "http://example.com ",
        "\thttps://example.com",
        "https://example.com\n",
        "http://exa mple.com",
        "http://example.com/a\tb",
        "http://example.com/a\nb",
        "http://example.com/a\177b",
        "HTTP://EXAMPLE.COM",
        "Https://example.com",
    };
    for (size_t at = 0; at < sizeof rejected / sizeof rejected[0]; at++) {
        BT_CHECK_EQ(bt_app_url_is_openable(rejected[at]), 0);
    }
}

BT_TEST(open_url, open_routes_a_valid_url_through_the_bound_launcher) {
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    BT_CHECK_EQ(bt_app_open_url("https://example.test/recon"), 0);
    BT_CHECK_EQ(g_open_calls, 1);
    BT_CHECK(strcmp(g_open_last, "https://example.test/recon") == 0);
    BT_CHECK_EQ(bt_app_open_url("http://example.test/video?v=2"), 0);
    BT_CHECK_EQ(g_open_calls, 2);
    BT_CHECK(strcmp(g_open_last, "http://example.test/video?v=2") == 0);
    bt_app_open_url_unbind();
}

BT_TEST(open_url, open_returns_minus_one_without_calling_the_launcher_on_rejects) {
    static const char *const rejected[] = {
        NULL,
        "",
        "file:///C:/Users/x",
        "javascript:alert(1)",
        " http://example.com",
        "http://example.com ",
        "http://example.com/a\tb",
        "http://",
    };
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    for (size_t at = 0; at < sizeof rejected / sizeof rejected[0]; at++) {
        BT_CHECK_EQ(bt_app_open_url(rejected[at]), -1);
    }
    BT_CHECK_EQ(g_open_calls, 0);
    bt_app_open_url_unbind();
}

BT_TEST(open_url, launcher_failure_maps_to_minus_one) {
    stub_reset(-7);
    bt_app_open_url_bind(stub_launch);
    BT_CHECK_EQ(bt_app_open_url("https://example.test/recon"), -1);
    BT_CHECK_EQ(g_open_calls, 1);
    stub_reset(3);
    BT_CHECK_EQ(bt_app_open_url("https://example.test/recon"), -1);
    BT_CHECK_EQ(g_open_calls, 1);
    bt_app_open_url_unbind();
}

BT_TEST(open_url, unbind_restores_the_default_launcher) {
    stub_reset(0);
    bt_app_open_url_bind(stub_launch);
    BT_CHECK_EQ(bt_app_open_url("https://example.test/a"), 0);
    BT_CHECK_EQ(g_open_calls, 1);
    bt_app_open_url_unbind();
    bt_app_open_url_unbind();
    bt_app_open_url_bind(stub_launch);
    BT_CHECK_EQ(bt_app_open_url("https://example.test/b"), 0);
    BT_CHECK_EQ(g_open_calls, 2);
    BT_CHECK(strcmp(g_open_last, "https://example.test/b") == 0);
    bt_app_open_url_unbind();
    stub_reset(0);
    BT_CHECK_EQ(bt_app_open_url(NULL), -1);
    BT_CHECK_EQ(bt_app_open_url("javascript:alert(1)"), -1);
    BT_CHECK_EQ(g_open_calls, 0);
}

BT_TEST(open_url, an_over_long_url_fails_widening_before_any_launch) {
    char url[6000];
    strcpy(url, "https://example.test/");
    memset(url + strlen(url), 'a', sizeof url - strlen(url) - 1);
    bt_app_open_url_unbind();
    BT_CHECK_EQ(bt_app_open_url(url), -1);
}
