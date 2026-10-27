#include "gendata/lib.h"

#include <string.h>

#include "common/bt_trace.h"
#include "common/sha256.h"
#include "common/strbuf.h"
#include "data/digest.h"
#include "data/paths.h"

const bt_gd_special_count BT_GD_SPECIAL_COUNTS[BT_GD_SPECIAL_COUNTS_LEN] = {
    {"cherry", "flips", 25}, {"cherry", "twists", 20}, {"cherry", "parity", 26},
    {"cherry", "ltct", 8},   {"eggins", "flips", 0},   {"eggins", "twists", 5},
    {"eggins", "parity", 2}, {"eggins", "ltct", 3},
};

const char *bt_gd_crate_id(void) {
    return "bt-gendata";
}

static int has_prefix_root(const char *path) {
    if (path[0] == '\0') {
        return 0;
    }
    if ((path[0] == '/' || path[0] == '\\')) {
        return 1;
    }
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
        path[1] == ':') {
        return 1;
    }
    return 0;
}

int bt_gd_synth_params_sha256(const bt_json_value *config, const char *generator, char out[65],
                              char *err, size_t err_len) {
    BT_TRACE_ENTER("gd.lib.synth_params_sha256");
    const bt_json_value *section = config != NULL ? bt_json_get(config, "synth") : NULL;
    if (section == NULL) {
        snprintf(err, err_len, "%s", "config section missing: synth");
        BT_TRACE_FAIL("gd.lib.synth_params_sha256", err);
        return -1;
    }
    bt_strbuf canonical;
    bt_strbuf_init(&canonical, 0);
    if (bt_json_serialize(section, &canonical) != 0) {
        bt_strbuf_free(&canonical);
        snprintf(err, err_len, "%s", "synth section serialization failed");
        BT_TRACE_FAIL("gd.lib.synth_params_sha256", err);
        return -1;
    }
    bt_sha256_ctx ctx;
    bt_sha256_init(&ctx);
    bt_sha256_update(&ctx, canonical.data, canonical.len);
    bt_sha256_update(&ctx, "\0", 1);
    bt_sha256_update(&ctx, generator, strlen(generator));
    unsigned char digest[32];
    bt_sha256_final(&ctx, digest);
    bt_sha256_hex(digest, out);
    bt_strbuf_free(&canonical);
    BT_TRACE_EXIT("gd.lib.synth_params_sha256");
    return 0;
}

static int integer_field(const bt_json_value *section, const char *key, size_t *out, char *err,
                         size_t err_len) {
    const bt_json_value *node = bt_json_get(section, key);
    if (node == NULL || node->type != BT_JSON_NUMBER || !bt_json_is_integer(node) ||
        node->integer < 0) {
        snprintf(err, err_len, "config section missing: synth.%s", key);
        return -1;
    }
    *out = (size_t)node->integer;
    return 0;
}

int bt_gd_limits_from_config(const bt_json_value *config, bt_gd_synth_limits *out, char *err,
                             size_t err_len) {
    BT_TRACE_ENTER("gd.lib.limits_from_config");
    const bt_json_value *section = config != NULL ? bt_json_get(config, "synth") : NULL;
    if (section == NULL) {
        snprintf(err, err_len, "%s", "config section missing: synth");
        BT_TRACE_FAIL("gd.lib.limits_from_config", err);
        return -1;
    }
    if (integer_field(section, "max_setup", &out->max_setup, err, err_len) != 0 ||
        integer_field(section, "max_insert", &out->max_insert, err, err_len) != 0 ||
        integer_field(section, "node_budget", &out->node_budget, err, err_len) != 0) {
        BT_TRACE_FAIL("gd.lib.limits_from_config", err);
        return -1;
    }
    BT_TRACE_EXIT("gd.lib.limits_from_config");
    return 0;
}

int bt_gd_data_vendor_path(const bt_json_value *config, char out[BT_PATH_MAX], char *err,
                           size_t err_len) {
    BT_TRACE_ENTER("gd.lib.data_vendor_path");
    const bt_json_value *node = config != NULL ? bt_json_get(config, "paths") : NULL;
    const bt_json_value *relative = node != NULL ? bt_json_get(node, "data_vendor") : NULL;
    if (relative == NULL || relative->type != BT_JSON_STRING) {
        snprintf(err, err_len, "%s", "config section missing: paths.data_vendor");
        BT_TRACE_FAIL("gd.lib.data_vendor_path", err);
        return -1;
    }
    if (has_prefix_root(relative->as.string)) {
        snprintf(out, BT_PATH_MAX, "%s", relative->as.string);
    } else {
        char root[BT_PATH_MAX];
        bt_data_workspace_root(root);
        snprintf(out, BT_PATH_MAX, "%s/%s", root, relative->as.string);
    }
    BT_TRACE_EXIT("gd.lib.data_vendor_path");
    return 0;
}
