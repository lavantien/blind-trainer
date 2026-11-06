#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/json.h"
#include "gendata/model.h"

static void drop_key(bt_json_value *object, const char *key) {
    bt_json_value *dropped = NULL;
    size_t keep = 0;
    for (size_t i = 0; i < object->as.object.count; i++) {
        if (strcmp(object->as.object.keys[i], key) == 0) {
            dropped = object->as.object.values[i];
            continue;
        }
        object->as.object.keys[keep] = object->as.object.keys[i];
        object->as.object.values[keep] = object->as.object.values[i];
        keep++;
    }
    object->as.object.count = keep;
    bt_json_free(dropped);
}

static bt_json_value *manifest_json(void) {
    bt_json_value *file = bt_json_new_object();
    bt_json_object_set(file, "solver", bt_json_new_string("cherry"));
    bt_json_object_set(file, "sheet", bt_json_new_string("edge"));
    bt_json_object_set(file, "rows", bt_json_new_integer(551));
    bt_json_object_set(file, "sha256", bt_json_new_string("cd"));
    bt_json_value *files = bt_json_new_array();
    bt_json_array_push(files, file);
    bt_json_value *edge = bt_json_new_object();
    bt_json_object_set(edge, "total", bt_json_new_integer(440));
    bt_json_object_set(edge, "covered", bt_json_new_integer(372));
    bt_json_object_set(edge, "uncovered", bt_json_new_integer(68));
    bt_json_value *corner = bt_json_new_object();
    bt_json_object_set(corner, "total", bt_json_new_integer(378));
    bt_json_object_set(corner, "covered", bt_json_new_integer(288));
    bt_json_object_set(corner, "uncovered", bt_json_new_integer(90));
    bt_json_value *cases = bt_json_new_object();
    bt_json_object_set(cases, "path", bt_json_new_string("cases.json"));
    bt_json_object_set(cases, "sha256", bt_json_new_string("ef"));
    bt_json_object_set(cases, "entries", bt_json_new_integer(818));
    bt_json_object_set(cases, "edge", edge);
    bt_json_object_set(cases, "corner", corner);
    bt_json_value *count = bt_json_new_object();
    bt_json_object_set(count, "solver", bt_json_new_string("cherry"));
    bt_json_object_set(count, "class", bt_json_new_string("flips"));
    bt_json_object_set(count, "rows", bt_json_new_integer(25));
    bt_json_value *counts = bt_json_new_array();
    bt_json_array_push(counts, count);
    bt_json_value *specials = bt_json_new_object();
    bt_json_object_set(specials, "path", bt_json_new_string("specials.json"));
    bt_json_object_set(specials, "sha256", bt_json_new_string("12"));
    bt_json_object_set(specials, "entries", bt_json_new_integer(89));
    bt_json_object_set(specials, "counts", counts);
    bt_json_value *root = bt_json_new_object();
    bt_json_object_set(root, "generated_at", bt_json_new_string("2026-10-14T00:00:00Z"));
    bt_json_object_set(root, "generator", bt_json_new_string("bt-gendata"));
    bt_json_object_set(root, "synth_params_sha256", bt_json_new_string("34"));
    bt_json_object_set(root, "corpus_sha256", bt_json_new_string("ab"));
    bt_json_object_set(root, "corpus_rows", bt_json_new_integer(1077));
    bt_json_object_set(root, "corpus_files", files);
    bt_json_object_set(root, "rows_in_pair_space", bt_json_new_integer(928));
    bt_json_object_set(root, "rows_other_buffer", bt_json_new_integer(60));
    bt_json_object_set(root, "cases", cases);
    bt_json_object_set(root, "specials", specials);
    return root;
}

static void mutate(bt_json_value *root, int variant) {
    switch (variant) {
    case 0:
        drop_key(root, "corpus_files");
        break;
    case 1:
        bt_json_object_set(root, "corpus_files", bt_json_new_number(1.5));
        break;
    case 2:
        drop_key(root, "cases");
        break;
    case 3:
        drop_key(root, "specials");
        break;
    case 4:
        bt_json_object_set(root, "generated_at", bt_json_new_number(1.5));
        break;
    case 5:
        bt_json_object_set(root, "corpus_rows", bt_json_new_string("x"));
        break;
    case 6:
        bt_json_object_set(root, "rows_other_buffer", bt_json_new_integer(-1));
        break;
    case 7:
        drop_key(bt_json_at(bt_json_get(root, "corpus_files"), 0), "rows");
        break;
    case 8:
        bt_json_object_set(bt_json_at(bt_json_get(root, "corpus_files"), 0), "solver",
                           bt_json_new_number(1.5));
        break;
    case 9:
        drop_key(bt_json_get(root, "cases"), "edge");
        break;
    case 10:
        drop_key(bt_json_get(root, "cases"), "path");
        break;
    case 11:
        bt_json_object_set(bt_json_get(root, "specials"), "counts", bt_json_new_number(1.5));
        break;
    case 12:
        bt_json_object_set(bt_json_at(bt_json_get(bt_json_get(root, "specials"), "counts"), 0),
                           "solver", bt_json_new_number(1.5));
        break;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: model_crash_probe <variant 0-12>\n");
        return 2;
    }
    int variant = atoi(argv[1]);
    bt_json_value *root = manifest_json();
    mutate(root, variant);
    bt_gd_manifest out;
    char err[96];
    err[0] = '\0';
    int rc = bt_gd_manifest_from_json(root, &out, err, sizeof err);
    printf("variant %d rc %d err %s\n", variant, rc, err);
    bt_json_free(root);
    return 0;
}
