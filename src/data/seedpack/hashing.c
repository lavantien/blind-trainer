#include "data/seedpack/hashing.h"

#include "common/bt_trace.h"
#include "common/sha256.h"
#include "common/strbuf.h"

#include <stdio.h>
#include <string.h>

void bt_seedpack_row_hash(const bt_seed_row *row, char out[BT_SEED_HASH_LEN]) {
    BT_TRACE_ENTER("seedpack.row_hash");
    bt_strbuf joined;
    bt_strbuf_init(&joined, 128);
    bt_strbuf_appendf(&joined, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", row->event, row->method,
                      row->stage, row->case_key, row->alg, row->front_json, row->source_url,
                      row->source_at);
    unsigned char digest[32];
    bt_sha256(joined.data, joined.len, digest);
    bt_sha256_hex(digest, out);
    bt_strbuf_free(&joined);
    BT_TRACE_EXIT("seedpack.row_hash");
}
