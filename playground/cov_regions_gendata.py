import json
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
lane = root / "artifacts/lane/lane-cov-gendata"
profdata = lane / "gendata.profdata"
llvm = "C:/Program Files/LLVM/bin"


def main():
    exes = sorted((lane / "cov-build/bin").glob("*gendata*_test.exe"))
    if not exes:
        print("cov-regions: no instrumented exes, run cov_scope_gendata.py first", file=sys.stderr)
        return 1
    proc = subprocess.run(
        [f"{llvm}/llvm-cov.exe", "export", f"--instr-profile={profdata}",
         "--include-filename-regex=tools[\\\\/]gendata", *map(str, exes)],
        cwd=str(root), capture_output=True, text=True,
        env={**os.environ})
    if proc.returncode != 0:
        print(f"cov-regions: export failed: {proc.stderr[-600:]}", file=sys.stderr)
        return 1
    data = json.loads(proc.stdout)["data"][0]
    for entry in sorted(data["files"], key=lambda f: f["filename"]):
        rel = entry["filename"].replace("\\", "/").split("tools/gendata/")[-1]
        dark = []
        for func in entry["functions"]:
            for region in func["regions"]:
                count = region[4]
                if count != 0:
                    continue
                start_line, start_col, end_line, end_col = region[0], region[1], region[2], region[3]
                kind = region[7] if len(region) > 7 else 0
                if kind != 0:
                    continue
                same_span_covered = any(
                    r[0] == start_line and r[1] == start_col and r[2] == end_line and r[3] == end_col
                    and r[4] != 0 for r in func["regions"])
                tag = "line0" if start_line == 0 else ("folded-dup" if same_span_covered else "source")
                dark.append((func["name"], start_line, start_col, end_line, end_col, tag,
                             func["count"]))
        if not dark:
            continue
        print(f"== {rel}")
        seen = set()
        for name, sl, sc, el, ec, tag, fcount in dark:
            key = (name, sl, sc, el, ec)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {tag:11} fn={name} [{sl}:{sc}-{el}:{ec}] fn_count={fcount}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
