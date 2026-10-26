#ifndef BT_JSON_H
#define BT_JSON_H

#include <stddef.h>

#include "strbuf.h"

#define BT_JSON_MAX_DEPTH 512

typedef enum {
    BT_JSON_NULL,
    BT_JSON_BOOL,
    BT_JSON_NUMBER,
    BT_JSON_STRING,
    BT_JSON_ARRAY,
    BT_JSON_OBJECT,
} bt_json_type;

typedef struct bt_json_value bt_json_value;

struct bt_json_value {
    bt_json_type type;
    union {
        int boolean;
        double number;
        char *string;
        struct {
            bt_json_value **items;
            size_t count;
        } array;
        struct {
            char **keys;
            bt_json_value **values;
            size_t count;
        } object;
    } as;
};

typedef struct {
    size_t line;
    size_t column;
    char message[96];
} bt_json_error;

int bt_json_parse(const char *text, size_t len, bt_json_value **out, bt_json_error *error);
void bt_json_free(bt_json_value *value);

bt_json_value *bt_json_new_null(void);
bt_json_value *bt_json_new_bool(int value);
bt_json_value *bt_json_new_number(double value);
bt_json_value *bt_json_new_string(const char *value);
bt_json_value *bt_json_new_array(void);
bt_json_value *bt_json_new_object(void);
int bt_json_array_push(bt_json_value *array, bt_json_value *item);
int bt_json_object_set(bt_json_value *object, const char *key, bt_json_value *value);

bt_json_value *bt_json_get(const bt_json_value *object, const char *key);
bt_json_value *bt_json_at(const bt_json_value *array, size_t index);

int bt_json_serialize(const bt_json_value *value, bt_strbuf *out);

#endif
