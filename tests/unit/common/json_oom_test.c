#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"

static const char *DEEP_OBJECT = "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6,\"g\":7}";
static const char *DEEP_ARRAY = "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]";
static const size_t OUT_CAP = 16;

static int armed_serialize(bt_json_value *value, size_t prefill_len, size_t ok_allocs) {
    bt_strbuf out;
    if (bt_strbuf_init(&out, OUT_CAP) != 0) {
        return -2;
    }
    for (size_t i = 0; i < prefill_len; i++) {
        if (bt_strbuf_append(&out, "x", 1) != 0) {
            bt_strbuf_free(&out);
            return -2;
        }
    }
    bt_alloc_fail_arm(ok_allocs);
    int rc = bt_json_serialize(value, &out);
    bt_alloc_fail_disarm();
    bt_strbuf_free(&out);
    return rc;
}

BT_TEST(json_oom, the_first_allocation_failing_fails_the_parse) {
    bt_json_error err;
    bt_json_value *value = NULL;
    bt_alloc_fail_arm(0);
    int rc = bt_json_parse(DEEP_OBJECT, strlen(DEEP_OBJECT), &value, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK(value == NULL);
}

BT_TEST(json_oom, every_interior_failure_point_fails_the_parse_cleanly) {
    for (size_t ok = 1; ok < 24; ok++) {
        bt_json_error err;
        bt_json_value *value = NULL;
        bt_alloc_fail_arm(ok);
        int rc = bt_json_parse(DEEP_OBJECT, strlen(DEEP_OBJECT), &value, &err);
        bt_alloc_fail_disarm();
        BT_CHECK(rc != 0);
        BT_CHECK(value == NULL);
        bt_json_value *array = NULL;
        bt_alloc_fail_arm(ok);
        rc = bt_json_parse(DEEP_ARRAY, strlen(DEEP_ARRAY), &array, &err);
        bt_alloc_fail_disarm();
        BT_CHECK(rc != 0);
        BT_CHECK(array == NULL);
    }
}

BT_TEST(json_oom, object_set_growth_failure_fails_closed_and_keeps_the_object_intact) {
    bt_json_error err;
    bt_json_value *object = NULL;
    int rc = bt_json_parse(DEEP_OBJECT, strlen(DEEP_OBJECT), &object, &err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(object != NULL);
    size_t count = object->as.object.count;
    bt_alloc_fail_arm(0);
    bt_json_value *starved = bt_json_new_number(99);
    bt_alloc_fail_disarm();
    BT_CHECK(starved == NULL);
    bt_alloc_fail_arm(1);
    bt_json_value *replacement = bt_json_new_number(99);
    BT_CHECK(replacement != NULL);
    int set_rc = bt_json_object_set(object, "zz", replacement);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(set_rc, -1);
    bt_json_free(replacement);
    BT_CHECK_EQ(object->as.object.count, count);
    bt_alloc_fail_arm(1000);
    bt_json_value *fresh = bt_json_new_number(7);
    int full_rc = bt_json_object_set(object, "zz", fresh);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(full_rc, 0);
    BT_CHECK_EQ(object->as.object.count, count + 1);
    bt_json_value *probe = bt_json_get(object, "g");
    BT_CHECK(probe != NULL);
    BT_CHECK_EQ((int)(probe->type == BT_JSON_NUMBER), 1);
    bt_json_free(object);
}

BT_TEST(json_oom, array_push_growth_failure_fails_closed) {
    bt_json_error err;
    bt_json_value *array = NULL;
    int rc = bt_json_parse(DEEP_ARRAY, strlen(DEEP_ARRAY), &array, &err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK(array != NULL);
    size_t count = array->as.array.count;
    bt_alloc_fail_arm(0);
    bt_json_value *starved = bt_json_new_number(1);
    bt_alloc_fail_disarm();
    BT_CHECK(starved == NULL);
    bt_alloc_fail_arm(1);
    bt_json_value *item = bt_json_new_number(1);
    BT_CHECK(item != NULL);
    int push_rc = bt_json_array_push(array, item);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(push_rc, -1);
    bt_json_free(item);
    BT_CHECK_EQ(array->as.array.count, count);
    bt_alloc_fail_arm(1000);
    bt_json_value *healthy = bt_json_new_number(2);
    int full_rc = bt_json_array_push(array, healthy);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(full_rc, 0);
    BT_CHECK_EQ(array->as.array.count, count + 1);
    bt_json_free(array);
}

BT_TEST(json_oom, serialize_reports_the_failure_and_writes_nothing_partial) {
    bt_json_error err;
    bt_json_value *value = NULL;
    int rc = bt_json_parse(DEEP_OBJECT, strlen(DEEP_OBJECT), &value, &err);
    BT_CHECK_EQ(rc, 0);
    bt_strbuf out;
    BT_CHECK_EQ(bt_strbuf_init(&out, OUT_CAP), 0);
    bt_alloc_fail_arm(0);
    int serialize_rc = bt_json_serialize(value, &out);
    bt_alloc_fail_disarm();
    BT_CHECK(serialize_rc != 0);
    bt_strbuf_free(&out);
    bt_strbuf healthy;
    BT_CHECK_EQ(bt_strbuf_init(&healthy, OUT_CAP), 0);
    bt_alloc_fail_arm(1000);
    int full_rc = bt_json_serialize(value, &healthy);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(full_rc, 0);
    BT_CHECK(healthy.len > 0);
    bt_strbuf_free(&healthy);
    bt_json_free(value);
}

BT_TEST(json_oom, new_string_allocation_arms_fail_clean) {
    bt_alloc_fail_arm(0);
    bt_json_value *starved = bt_json_new_string("s");
    bt_alloc_fail_disarm();
    BT_CHECK(starved == NULL);
    bt_alloc_fail_arm(1);
    bt_json_value *no_copy = bt_json_new_string("s");
    bt_alloc_fail_disarm();
    BT_CHECK(no_copy == NULL);
}

BT_TEST(json_oom, array_value_allocation_failure_fails_the_parse) {
    bt_json_error err;
    bt_json_value *value = NULL;
    bt_alloc_fail_arm(0);
    int rc = bt_json_parse(DEEP_ARRAY, strlen(DEEP_ARRAY), &value, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK(value == NULL);
    BT_CHECK(strstr(err.message, "array allocation") != NULL);
}

BT_TEST(json_oom, long_string_growth_failure_fails_the_parse) {
    const char *text = "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"";
    bt_json_error err;
    bt_json_value *value = NULL;
    bt_alloc_fail_arm(1);
    int rc = bt_json_parse(text, strlen(text), &value, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK(value == NULL);
    BT_CHECK(strstr(err.message, "string allocation") != NULL);
}

BT_TEST(json_oom, object_set_key_copy_failure_fails_closed) {
    bt_json_value *object = bt_json_new_object();
    bt_json_value *value = bt_json_new_integer(4);
    BT_CHECK(object != NULL && value != NULL);
    bt_alloc_fail_arm(2);
    int rc = bt_json_object_set(object, "kk", value);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(rc, -1);
    BT_CHECK_EQ(object->as.object.count, 0);
    bt_json_free(value);
    bt_json_free(object);
}

BT_TEST(json_oom, serialize_string_arms_fail_at_every_growth_point) {
    bt_json_value *plain = bt_json_new_string("s");
    bt_json_value *esc = bt_json_new_string("1234567890123\"");
    bt_json_value *ctrl = bt_json_new_string("123456789\x01");
    bt_json_value *wide = bt_json_new_string("abcdefghijklmno");
    bt_json_value *shut = bt_json_new_string("abcdefghijklmn");
    BT_CHECK(plain != NULL && esc != NULL && ctrl != NULL && wide != NULL && shut != NULL);
    BT_CHECK_EQ(armed_serialize(plain, OUT_CAP - 1, 0), -1);
    BT_CHECK_EQ(armed_serialize(esc, 0, 0), -1);
    BT_CHECK_EQ(armed_serialize(ctrl, 0, 0), -1);
    BT_CHECK_EQ(armed_serialize(wide, 0, 0), -1);
    BT_CHECK_EQ(armed_serialize(shut, 0, 0), -1);
    bt_json_free(plain);
    bt_json_free(esc);
    bt_json_free(ctrl);
    bt_json_free(wide);
    bt_json_free(shut);
}

BT_TEST(json_oom, serialize_array_arms_fail_at_every_growth_point) {
    bt_json_value *empty = bt_json_new_array();
    bt_json_value *spread = bt_json_new_array();
    bt_json_value *pair = bt_json_new_array();
    bt_json_value *wide = bt_json_new_string("aaaaaaaaaaaaaaaaaaaa");
    bt_json_value *big = bt_json_new_integer(12345678901234LL);
    bt_json_value *five = bt_json_new_integer(5);
    BT_CHECK(empty != NULL && spread != NULL && pair != NULL);
    BT_CHECK(wide != NULL && big != NULL && five != NULL);
    BT_CHECK_EQ(bt_json_array_push(spread, wide), 0);
    BT_CHECK_EQ(bt_json_array_push(pair, big), 0);
    BT_CHECK_EQ(bt_json_array_push(pair, five), 0);
    BT_CHECK_EQ(armed_serialize(empty, OUT_CAP - 1, 0), -1);
    BT_CHECK_EQ(armed_serialize(spread, 0, 0), -1);
    BT_CHECK_EQ(armed_serialize(pair, 0, 0), -1);
    bt_json_free(empty);
    bt_json_free(spread);
    bt_json_free(pair);
}

BT_TEST(json_oom, serialize_object_arms_fail_at_every_growth_point) {
    bt_json_value *empty = bt_json_new_object();
    bt_json_value *spread = bt_json_new_object();
    bt_json_value *deep = bt_json_new_object();
    bt_json_value *one = bt_json_new_integer(1);
    bt_json_value *also_one = bt_json_new_integer(1);
    bt_json_value *wide = bt_json_new_integer(12345678901234567LL);
    BT_CHECK(empty != NULL && spread != NULL && deep != NULL);
    BT_CHECK(one != NULL && also_one != NULL && wide != NULL);
    BT_CHECK_EQ(bt_json_object_set(spread, "kkkkkkkkkk", one), 0);
    BT_CHECK_EQ(bt_json_object_set(spread, "z", also_one), 0);
    BT_CHECK_EQ(bt_json_object_set(deep, "k", wide), 0);
    BT_CHECK_EQ(armed_serialize(empty, OUT_CAP - 1, 0), -1);
    BT_CHECK_EQ(armed_serialize(spread, 0, 1), -1);
    BT_CHECK_EQ(armed_serialize(deep, 0, 1), -1);
    bt_json_free(empty);
    bt_json_free(spread);
    bt_json_free(deep);
}
