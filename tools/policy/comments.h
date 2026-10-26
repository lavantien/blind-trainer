#ifndef POLICY_COMMENTS_H
#define POLICY_COMMENTS_H

#include <stddef.h>

#include "policy/lib.h"

int policy_comment_findings(const char *text, size_t len, const char *path, policy_finding **out,
                            size_t *count);
void policy_findings_free(policy_finding *findings, size_t count);

#endif
