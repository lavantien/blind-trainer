#include "policy/comments.h"

#include <stdlib.h>

int policy_comment_findings(const char *text, size_t len, const char *path, policy_finding **out,
                            size_t *count) {
    (void)text;
    (void)len;
    (void)path;
    *out = NULL;
    *count = 0;
    return 0;
}

void policy_findings_free(policy_finding *findings, size_t count) {
    for (size_t at = 0; at < count; at++) {
        policy_finding_free(&findings[at]);
    }
    free(findings);
}
