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
    gendata_funcs = []
    covered_lines_by_fn = {}
    for func in data["functions"]:
        filename = func["filenames"][0].replace("\\", "/")
        if "tools/gendata/" not in filename:
            continue
        rel = filename.split("tools/gendata/")[-1]
        gendata_funcs.append((rel, func))
        lines = covered_lines_by_fn.setdefault((rel, func["name"]), set())
        for region in func["regions"]:
            if region[4] != 0:
                lines.update(range(region[0], region[2] + 1))
    by_file = {}
    for rel, func in gendata_funcs:
        regions = func["regions"]
        covered_spans = {(r[0], r[1], r[2], r[3]) for r in regions if r[4] != 0}
        outlined = ".c:" in func["name"]
        dark = []
        for region in regions:
            if region[4] != 0:
                continue
            kind = region[7] if len(region) > 7 else 0
            if kind != 0:
                continue
            span = (region[0], region[1], region[2], region[3])
            if span in covered_spans:
                tag = "folded-dup"
            elif outlined and any(
                region[0] in lines
                for (other_rel, other_name), lines in covered_lines_by_fn.items()
                if other_rel == rel and other_name != func["name"]):
                tag = "inlined-dup"
            elif region[0] == 0:
                tag = "line0"
            else:
                tag = "source"
            dark.append((region[0], region[1], region[2], region[3], tag))
        if dark:
            by_file.setdefault(rel, []).append((func["name"], func["count"], dark))
    for rel in sorted(by_file):
        print(f"== {rel}")
        for name, count, dark in by_file[rel]:
            spans = ", ".join(f"[{sl}:{sc}-{el}:{ec}] {tag}" for sl, sc, el, ec, tag in dark)
            print(f"  fn={name} fn_count={count}: {spans}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
