import re
import sys

PATHS = ["tools/mutants-gate/tests.c", "tools/mutate/tests.c"]
ENTRY = re.compile(r"^( +(?:bt_test_(?:gate|mutate)_\w+))\},$")
for path in PATHS:
    lines = open(path, encoding="utf-8").read().splitlines()
    changed = 0
    for index, line in enumerate(lines):
        match = ENTRY.match(line)
        if match is not None:
            lines[index] = f"{match.group(1)},}},"
            changed += 1
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    print(f"{path}: {changed} entries padded")
sys.exit(0)
