import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LLVM_COV = Path(r"C:\Program Files\LLVM\bin\llvm-cov.exe")
BIN = REPO / "build" / "bin-cov"
ROOT_ESC = re.escape(str(REPO))

CANDIDATES = {
    "current": json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))["coverage"]["include_regex"],
    "anchored": ROOT_ESC + r"[\\/](src|tools)[\\/]|" + ROOT_ESC + r"[\\/]tests[\\/]fuzz[\\/]",
}


def export_files(regex):
    exes = sorted(BIN.glob("*.exe"))
    argv = [str(LLVM_COV), "export", "-empty-profile", f"--include-filename-regex={regex}", str(exes[0])]
    argv += [f"--object={e}" for e in exes[1:]]
    proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8")
    if proc.returncode != 0:
        raise SystemExit(proc.stderr[-2000:])
    payload = json.loads(proc.stdout)["data"][0]
    rels = {}
    for f in payload["files"]:
        rel = str(f["filename"]).replace("\\", "/")[len(str(REPO)) + 1:]
        rels[rel] = f["summary"]["regions"]
    return rels


def inventory():
    wanted = []
    for base in ("src", "tools", "tests/fuzz", "tests/unit"):
        for p in sorted((REPO / base).rglob("*")):
            if p.is_file() and p.suffix in (".c", ".h"):
                wanted.append(str(p.relative_to(REPO)).replace("\\", "/"))
    return wanted


files = {name: export_files(rx) for name, rx in CANDIDATES.items()}
inv = inventory()
in_scope = [p for p in inv if not p.startswith("tests/unit/")]
unit = [p for p in inv if p.startswith("tests/unit/")]

for name, rels in files.items():
    unit_leaked = [p for p in rels if p.startswith("tests/unit/")]
    missing = [p for p in in_scope if p not in rels]
    print(f"{name}: files={len(rels)} unit_leaked={len(unit_leaked)} missing_in_scope={len(missing)}")
    for p in unit_leaked[:3]:
        print(f"  leaked: {p}")
    for p in missing[:5]:
        print(f"  missing: {p}")

anchored = files["anchored"]
extra = [p for p in anchored if not (p.startswith("src/") or p.startswith("tools/") or p.startswith("tests/fuzz/"))]
print("anchored out-of-scope residue:", extra[:5], len(extra))
covered = sum(int(v["covered"]) for v in anchored.values())
count = sum(int(v["count"]) for v in anchored.values())
print(f"anchored static totals: {covered}/{count}")
mains = [p for p in anchored if p.endswith("main.c")]
print("main.c files in anchored set:", sorted(mains))
