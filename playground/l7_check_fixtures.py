import json
import re
import sys

TEXT = open("tools/mutants-gate/tests.c", encoding="utf-8").read()


def decode(lines):
    out = []
    for line in lines:
        assert line.startswith('"'), line
        assert line.endswith('"') or line.endswith('";'), line
        body = line[line.index('"') + 1 : line.rindex('"')]
        i = 0
        while i < len(body):
            c = body[i]
            if c == "\\":
                nxt = body[i + 1]
                if nxt == "n":
                    out.append("\n")
                elif nxt == "t":
                    out.append("\t")
                elif nxt == '"':
                    out.append('"')
                elif nxt == "\\":
                    out.append("\\")
                else:
                    raise AssertionError("escape " + repr(body[i : i + 2]))
                i += 2
            else:
                out.append(c)
                i += 1
    return "".join(out)


pattern = re.compile(r"static const char (FX_\w+)\[\] =\n((?:    .*\"\n?)+?    .*\";)", re.M)
failed = 0
for name, block in pattern.findall(TEXT):
    lines = [line.strip() for line in block.strip().splitlines()]
    value = decode(lines)
    if name in ("FX_INVALID_JSON", "FX_REAL_ALLOWLIST", "FX_COMMENTED_ALLOWLIST"):
        print(name, "ok (non-json)", len(value))
        continue
    try:
        parsed = json.loads(value)
        outcomes = parsed.get("outcomes", [])
        kinds = []
        for outcome in outcomes:
            scenario = outcome.get("scenario")
            if scenario is None:
                kinds.append("MISSING_SCENARIO")
            elif isinstance(scenario, str):
                kinds.append(scenario)
            else:
                kinds.append(sorted(scenario.keys()))
        print(name, "ok", len(outcomes), kinds)
    except json.JSONDecodeError as exc:
        failed += 1
        print(name, "PARSE FAIL", exc.msg, "at col", exc.colno)
        around = value[max(0, exc.pos - 60) : exc.pos + 40]
        print("  context:", repr(around))
sys.exit(1 if failed else 0)
