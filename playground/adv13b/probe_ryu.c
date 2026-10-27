#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/bt_trace.h"
#include "common/json.h"

#include <stdio.h>
#include <string.h>

static void print_f64_json(const char *key, const char *literal) {
    char text[128];
    snprintf(text, sizeof text, "{\"v\": %s}", literal);
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        printf("f64 %s parse failed: %s\n", key, error.message);
        return;
    }
    bt_strbuf out;
    bt_strbuf_init(&out, 256);
    if (bt_gr_json_value_text(value, &out) != 0) {
        printf("f64 %s serialize failed\n", key);
    } else {
        printf("f64 %s -> %s\n", key, out.data);
    }
    bt_strbuf_free(&out);
    bt_json_free(value);
}

static void print_f32_assignment(const char *key, float best_fitness) {
    bt_gr_syllable_assignment assignment;
    memset(&assignment, 0, sizeof assignment);
    assignment.best_fitness = best_fitness;
    bt_strbuf out;
    bt_strbuf_init(&out, 256);
    if (bt_gr_assignment_text(&assignment, &out) != 0) {
        printf("f32 %s serialize failed\n", key);
    } else {
        const char *line = strstr(out.data, "\"best_fitness\": ");
        printf("f32 %s -> %s", key, line ? line + strlen("\"best_fitness\": ") : "?");
    }
    bt_strbuf_free(&out);
}

int main(void) {
    bt_trace_init(65536, 8, 256);
    print_f64_json("1e-7", "1e-7");
    print_f64_json("5e-7", "5e-7");
    print_f64_json("1e-6", "1e-6");
    print_f64_json("1e-5", "1e-5");
    print_f64_json("1.23e-6", "1.23e-6");
    print_f64_json("1e15", "1e15");
    print_f64_json("1e16", "1e16");
    print_f64_json("1.234e16", "1.234e16");
    print_f64_json("1e100", "1e100");
    print_f64_json("0.1", "0.1");
    print_f64_json("123.456", "123.456");
    print_f32_assignment("1e12f", 1e12f);
    print_f32_assignment("1e13f", 1e13f);
    print_f32_assignment("1e15f", 1e15f);
    print_f32_assignment("1e-6f", 1e-6f);
    print_f32_assignment("1e-7f", 1e-7f);
    bt_trace_shutdown();
    return 0;
}
