import re
from pathlib import Path

path = Path("tests/unit/tools/jobwrap_ceiling_test.c")
text = path.read_text(encoding="utf-8")

def widen(match):
    body = match.group(2)
    if "script" in body:
        return match.group(0)
    body = re.sub(r'"((?:[^"\\]|\\.)*)"', r'L"\1"', body)
    return "const wchar_t *" + match.group(1) + "[] = {" + body + "};"

text, count = re.subn(r"const char \*(\w+)\[\] = \{([^}]*)\};", widen, text)
print("arrays widened:", count)

anchor = '#include "common/bt_harness.h"'
assert anchor in text
text = text.replace(
    anchor,
    '#include "common/bt_harness.h"\n#include "jobwrap/lib.h"',
    1,
)

new_test = '''
BT_TEST(ceiling, non_ascii_payload_arguments_survive_the_wrap) {
    ceiling_fixture fx;
    fixture_ceiling(&fx, 268435456ULL);
    const wchar_t *argv[] = {L"--",  L"pwsh",  L"-NoProfile",
                             L"-Command", L"exit ($args[0].Length - 4)", L"café", NULL,};
    int code = -1;
    int rc = run_wrapped(fx.config, argv, CEILING_WAIT_EXIT_MS, &code);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
}
'''
marker = "BT_TEST(ceiling, forwards_payload_exit_codes) {"
assert marker in text
text = text.replace(marker, new_test.strip() + "\n\n" + marker, 1)

path.write_text(text, encoding="utf-8")
print("done")
