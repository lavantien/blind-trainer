#include "ga/hash.h"

#include "common/bt_trace.h"
#include "common/sha256.h"

void bt_ga_sha256_hex(const void *data, size_t len, char out[BT_GA_SHA256_HEX_LEN]) {
    BT_TRACE_ENTER("ga.sha256_hex");
    unsigned char digest[32];
    bt_sha256(data, len, digest);
    bt_sha256_hex(digest, out);
    BT_TRACE_EXIT("ga.sha256_hex");
}
