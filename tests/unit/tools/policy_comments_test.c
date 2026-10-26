#include "common/bt_harness.h"

#include <string.h>

#include "policy/comments.h"
#include "policy/lib.h"

BT_TEST(policy_comments, every_comment_becomes_a_finding_with_line_and_kind) {
    const char *input = "int a = 1;\n// line two\n/* block\nspanning */ int b = 2;";
    policy_finding *found = NULL;
    size_t count = 0;
    int rc = policy_comment_findings(input, strlen(input), "src/dirty.c", &found, &count);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(count, 2);
    BT_CHECK(strcmp(found[0].path, "src/dirty.c") == 0);
    BT_CHECK_EQ(found[0].line, 2);
    BT_CHECK(strcmp(found[0].kind, "comment") == 0);
    BT_CHECK(strcmp(found[0].detail, "line comment") == 0);
    BT_CHECK_EQ(found[1].line, 3);
    BT_CHECK(strcmp(found[1].detail, "block comment") == 0);
    policy_findings_free(found, count);
}

BT_TEST(policy_comments, comment_free_source_yields_no_findings) {
    const char *input = "const char *s = \"http://example.org/a//b\";\nint t = 1;";
    policy_finding *found = NULL;
    size_t count = 0;
    int rc = policy_comment_findings(input, strlen(input), "clean.c", &found, &count);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(count, 0);
}

BT_TEST(policy_comments, python_hash_comment_becomes_a_finding) {
    const char *input = "x = 1\n# note\ny = 2";
    policy_finding *found = NULL;
    size_t count = 0;
    int rc = policy_comment_findings(input, strlen(input), "src/dirty.py", &found, &count);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(count, 1);
    BT_CHECK_EQ(found[0].line, 2);
    BT_CHECK(strcmp(found[0].kind, "comment") == 0);
    BT_CHECK(strcmp(found[0].detail, "hash comment") == 0);
    policy_findings_free(found, count);
}
