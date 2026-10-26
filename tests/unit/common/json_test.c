#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"

static void parse_ok(const char *text, bt_json_value **out) {
    bt_json_error error;
    int rc = bt_json_parse(text, strlen(text), out, &error);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "parse failed at %zu:%zu: %s", error.line, error.column,
                        error.message);
    }
}

static void parse_err(const char *text, size_t want_line, size_t want_column, const char *needle) {
    bt_json_value *value = NULL;
    bt_json_error error;
    error.line = 0;
    error.column = 0;
    error.message[0] = '\0';
    int rc = bt_json_parse(text, strlen(text), &value, &error);
    BT_CHECK(rc != 0);
    BT_CHECK(value == NULL);
    BT_CHECK_EQ(error.line, want_line);
    BT_CHECK_EQ(error.column, want_column);
    BT_CHECK(strstr(error.message, needle) != NULL);
}

static void serialize_is(const char *text, const char *want) {
    bt_json_value *value = NULL;
    parse_ok(text, &value);
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    BT_CHECK_EQ(bt_json_serialize(value, &sb), 0);
    BT_CHECK(strcmp(sb.data, want) == 0);
    bt_strbuf_free(&sb);
    bt_json_free(value);
}

BT_TEST(json, parses_and_serializes_scalars) {
    serialize_is("null", "null");
    serialize_is("true", "true");
    serialize_is("false", "false");
    serialize_is("0", "0");
    serialize_is("-0", "-0");
    serialize_is("42", "42");
    serialize_is("-17", "-17");
    serialize_is("0.1", "0.1");
    serialize_is("-2.5", "-2.5");
    serialize_is("1e-7", "1e-07");
    serialize_is("\"hi\"", "\"hi\"");
}

BT_TEST(json, object_keys_serialize_sorted) {
    serialize_is("{\"b\": 1, \"a\": 2}", "{\"a\":2,\"b\":1}");
    serialize_is("{\"zebra\": 1, \"apple\": 2, \"Apple\": 3}",
                 "{\"Apple\":3,\"apple\":2,\"zebra\":1}");
}

BT_TEST(json, nested_round_trip_is_exact) {
    serialize_is("{\"a\": [1, 2, {\"deep\": true}], \"b\": \"x\"}",
                 "{\"a\":[1,2,{\"deep\":true}],\"b\":\"x\"}");
    serialize_is("[[], {}, [null]]", "[[],{},[null]]");
}

BT_TEST(json, duplicate_keys_rejected) {
    parse_err("{\"a\": 1, \"a\": 2}", 1, 13, "duplicate");
    parse_err("{\"x\": {\"x\": 1, \"x\": 2}}", 1, 19, "duplicate");
}

BT_TEST(json, line_and_column_pins) {
    parse_err("{\n  \"a\": ,\n}", 2, 8, "value");
    parse_err("[1, 2,]", 1, 7, "value");
    parse_err("{\"a\": 1x}", 1, 8, "object");
    parse_err("1x", 1, 2, "trailing");
    parse_err("01", 1, 2, "number");
    parse_err("1.", 1, 3, "number");
    parse_err("{\"a\"", 1, 5, "expected");
}

BT_TEST(json, control_characters_in_strings_rejected) {
    const char *raw = "{\"a\": \"line\nbreak\"}";
    bt_json_value *value = NULL;
    bt_json_error error;
    int rc = bt_json_parse(raw, strlen(raw), &value, &error);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(error.message, "control") != NULL);
}

BT_TEST(json, string_escapes_decode) {
    bt_json_value *value = NULL;
    parse_ok("\"\\u0041\\n\\t\\\"\\\\\\/\"", &value);
    BT_CHECK_EQ(value->type, BT_JSON_STRING);
    BT_CHECK(strcmp(value->as.string, "A\n\t\"\\/") == 0);
    bt_json_free(value);
}

BT_TEST(json, surrogate_pairs_decode_to_utf8) {
    bt_json_value *value = NULL;
    parse_ok("\"\\uD83D\\uDE00\"", &value);
    const unsigned char *s = (const unsigned char *)value->as.string;
    BT_CHECK_EQ(s[0], 0xF0U);
    BT_CHECK_EQ(s[1], 0x9FU);
    BT_CHECK_EQ(s[2], 0x98U);
    BT_CHECK_EQ(s[3], 0x80U);
    BT_CHECK_EQ(s[4], 0U);
    bt_json_free(value);
}

BT_TEST(json, lone_surrogates_rejected) {
    bt_json_value *value = NULL;
    bt_json_error error;
    int rc = bt_json_parse("\"\\uD800\"", strlen("\"\\uD800\""), &value, &error);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(error.message, "surrogate") != NULL);
}

BT_TEST(json, number_range_rejects_infinity) {
    parse_err("1e999", 1, 6, "range");
}

BT_TEST(json, utf8_passthrough_and_validation) {
    bt_json_value *value = NULL;
    parse_ok("\"\xc3\xa9\"", &value);
    BT_CHECK(strcmp(value->as.string, "\xc3\xa9") == 0);
    bt_json_free(value);
    value = NULL;
    bt_json_error error;
    int rc = bt_json_parse("\"\x80\"", strlen("\"\x80\""), &value, &error);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(error.message, "utf-8") != NULL);
}

BT_TEST(json, shortest_roundtrip_doubles) {
    serialize_is("0.1", "0.1");
    serialize_is("0.3333333333333333", "0.3333333333333333");
    serialize_is("9007199254740992", "9007199254740992");
    serialize_is("1e-7", "1e-07");
    bt_json_value *value = NULL;
    parse_ok("1.7976931348623157e308", &value);
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 16), 0);
    BT_CHECK_EQ(bt_json_serialize(value, &sb), 0);
    double round = strtod(sb.data, NULL);
    BT_CHECK(round == 1.7976931348623157e308);
    bt_strbuf_free(&sb);
    bt_json_free(value);
}

BT_TEST(json, builder_serializes_sorted_and_stable) {
    bt_json_value *first = bt_json_new_object();
    bt_json_value *second = bt_json_new_object();
    bt_json_value *a1 = bt_json_new_number(1);
    bt_json_value *b2 = bt_json_new_number(2);
    bt_json_value *b2b = bt_json_new_number(2);
    bt_json_value *a1b = bt_json_new_number(1);
    BT_CHECK(first != NULL && second != NULL);
    BT_CHECK_EQ(bt_json_object_set(first, "zeta", a1), 0);
    BT_CHECK_EQ(bt_json_object_set(first, "alpha", b2), 0);
    BT_CHECK_EQ(bt_json_object_set(second, "alpha", b2b), 0);
    BT_CHECK_EQ(bt_json_object_set(second, "zeta", a1b), 0);
    bt_strbuf one;
    bt_strbuf two;
    BT_CHECK_EQ(bt_strbuf_init(&one, 8), 0);
    BT_CHECK_EQ(bt_strbuf_init(&two, 8), 0);
    BT_CHECK_EQ(bt_json_serialize(first, &one), 0);
    BT_CHECK_EQ(bt_json_serialize(second, &two), 0);
    BT_CHECK(strcmp(one.data, "{\"alpha\":2,\"zeta\":1}") == 0);
    BT_CHECK(strcmp(one.data, two.data) == 0);
    bt_strbuf_free(&one);
    bt_strbuf_free(&two);
    bt_json_free(first);
    bt_json_free(second);
}

BT_TEST(json, getters_and_accessors) {
    bt_json_value *value = NULL;
    parse_ok("{\"app\": {\"reps\": 6, \"on\": true, \"name\": \"bt\"}, \"list\": [10, 20]}",
             &value);
    bt_json_value *app = bt_json_get(value, "app");
    BT_CHECK(app != NULL);
    BT_CHECK_EQ(bt_json_get(app, "reps")->as.number, 6.0);
    BT_CHECK_EQ(bt_json_get(app, "on")->as.boolean, 1);
    BT_CHECK(strcmp(bt_json_get(app, "name")->as.string, "bt") == 0);
    bt_json_value *list = bt_json_get(value, "list");
    BT_CHECK_EQ(bt_json_at(list, 0)->as.number, 10.0);
    BT_CHECK_EQ(bt_json_at(list, 1)->as.number, 20.0);
    BT_CHECK(bt_json_at(list, 2) == NULL);
    BT_CHECK(bt_json_get(value, "missing") == NULL);
    bt_json_free(value);
}

BT_TEST(json, deep_nesting_is_capped) {
    bt_strbuf sb;
    BT_CHECK_EQ(bt_strbuf_init(&sb, 64), 0);
    for (int i = 0; i < 600; i++) {
        BT_CHECK_EQ(bt_strbuf_append_str(&sb, "["), 0);
    }
    bt_json_value *value = NULL;
    bt_json_error error;
    int rc = bt_json_parse(sb.data, sb.len, &value, &error);
    BT_CHECK(rc != 0);
    BT_CHECK(strstr(error.message, "depth") != NULL);
    bt_strbuf_free(&sb);
}
