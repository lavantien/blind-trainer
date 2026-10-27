#ifndef GD_AUDIT_H
#define GD_AUDIT_H

#include <stddef.h>

#include "data/corpus.h"
#include "gendata/build.h"

void bt_gd_audit(const bt_gd_built *built, const bt_corpus *corpus, size_t digest_rows, char ***out,
                 size_t *count);

#endif
