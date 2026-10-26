#include "policy/comments.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "policy/lib.h"

int policy_comment_findings(const char *text, size_t len, const char *path, policy_finding **out,
                            size_t *count) {
    BT_TRACE_ENTER("policy.comment_findings");
    policy_lang lang = policy_lang_of_path(path);
    policy_masked masked;
    policy_mask_source(text, len, lang, &masked);
    size_t found = masked.comment_count;
    policy_finding *findings = policy_xalloc(found * sizeof *findings);
    for (size_t at = 0; at < found; at++) {
        policy_comment_span *span = &masked.comments[at];
        findings[at].path = policy_xstrdup(path);
        findings[at].line = policy_line_of(text, span->start);
        findings[at].kind = "comment";
        findings[at].detail = policy_xalloc(32);
        snprintf(findings[at].detail, 32, "%s comment", policy_comment_kind_str(span->kind));
    }
    policy_masked_free(&masked);
    *out = findings;
    *count = found;
    BT_TRACE_EXIT("policy.comment_findings");
    return 0;
}

void policy_findings_free(policy_finding *findings, size_t count) {
    for (size_t at = 0; at < count; at++) {
        policy_finding_free(&findings[at]);
    }
    free(findings);
}
