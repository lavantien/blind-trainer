#include <stdlib.h>
#include <string.h>

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/strbuf.h"

static const char *DEEP_OBJECT = "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6,\"g\":7}";
static const char *DEEP_ARRAY = "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]";

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
    BT_CHECK_EQ(bt_strbuf_init(&out, 16), 0);
    bt_alloc_fail_arm(0);
    int serialize_rc = bt_json_serialize(value, &out);
    bt_alloc_fail_disarm();
    BT_CHECK(serialize_rc != 0);
    bt_strbuf_free(&out);
    bt_strbuf healthy;
    BT_CHECK_EQ(bt_strbuf_init(&healthy, 16), 0);
    bt_alloc_fail_arm(1000);
    int full_rc = bt_json_serialize(value, &healthy);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(full_rc, 0);
    BT_CHECK(healthy.len > 0);
    bt_strbuf_free(&healthy);
    bt_json_free(value);
}
