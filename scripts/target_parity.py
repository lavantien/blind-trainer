import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import bt

MAKEFILE_TARGET_RE = re.compile(
    r"^([a-zA-Z0-9_-]+):[^=#]*?##\s*(.+?)\s*$", re.MULTILINE
)
DEFAULT_OUT = "artifacts/progress/t15-targets.md"


def makefile_targets(makefile_path):
    text = makefile_path.read_text(encoding="utf-8")
    return {
        match.group(1): match.group(2) for match in MAKEFILE_TARGET_RE.finditer(text)
    }


def bt_targets():
    return {name: doc for name, doc in bt.target_table(bt.target_functions(bt))}


def parity_rows(make_targets, script_targets):
    names = sorted(set(make_targets) | set(script_targets))
    rows = []
    for name in names:
        in_make = name in make_targets
        in_script = name in script_targets
        doc = script_targets.get(name) or make_targets.get(name) or ""
        rows.append((name, in_make, in_script, doc))
    return rows


def report_text(rows):
    lines = [
        "# t15 target parity",
        "",
        "| target | make | bt.py | doc |",
        "|---|---|---|---|",
    ]
    for name, in_make, in_script, doc in rows:
        lines.append(
            f"| {name} | {'yes' if in_make else 'no'} | {'yes' if in_script else 'no'} | {doc} |"
        )
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(prog="target_parity.py")
    parser.add_argument("--makefile", default="Makefile")
    parser.add_argument("--out", default=DEFAULT_OUT)
    parser.add_argument(
        "--strict",
        action="store_true",
        help="exit nonzero when a make target lacks a bt.py twin",
    )
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])
    root = bt.REPO_ROOT
    makefile = root / args.makefile
    if not makefile.is_file():
        print(f"missing makefile at {makefile}")
        return 2
    make_targets = makefile_targets(makefile)
    if not make_targets:
        print("no ## target docs found in the makefile")
        return 2
    script_targets = bt_targets()
    rows = parity_rows(make_targets, script_targets)
    out_path = root / args.out
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(report_text(rows), encoding="utf-8")
    missing = [
        name for name, in_make, in_script, _ in rows if in_make and not in_script
    ]
    extra = [name for name, in_make, in_script, _ in rows if not in_make and in_script]
    print(
        f"wrote {out_path}: {len(rows)} targets, {len(missing)} missing from bt.py, {len(extra)} new in bt.py"
    )
    for name in missing:
        print(f"missing: {name}")
    for name in extra:
        print(f"new: {name}")
    if args.strict and missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
